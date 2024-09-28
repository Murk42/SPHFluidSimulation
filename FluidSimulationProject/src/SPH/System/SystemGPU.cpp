#include "pch.h"
#include "SystemGPU.h"
#include "SPH/Graphics/SystemGPURenderer.h"
#include "SPH/SPHFunctions.h"
#include "gl/glew.h"
#include "GL/glext.h"
#undef min

namespace SPH
{	
	static cl::Program BuildOpenCLProgram(OpenCLContext& clContext, ArrayView<Path> paths, const Map<String, String>& values)
	{			
		cl_int ret;

		std::vector<std::string> sources;

		for (auto& path : paths)		
		{
			File sourceFile{ path, FileAccessPermission::Read };
			sources.push_back(std::string(sourceFile.GetSize(), ' '));
			sourceFile.Read(sources.back().data(), sourceFile.GetSize());
		}		
		
		auto program = cl::Program(clContext.context, sources, &ret);
		CL_CHECK(program);		

		String options;
		for (auto& pair : values)
			if (pair.value.Empty())
				options += " -D " + pair.key;
			else
				options += " -D " + pair.key + "=" + pair.value;

		if ((ret = program.build(clContext.device, options.Ptr())) == CL_BUILD_PROGRAM_FAILURE)
		{
			auto buildInfo = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(clContext.device, &ret);
			CL_CHECK(program);

			if (!buildInfo.empty())
				Debug::Logger::LogDebug("Client", "Build log: \n" + StringView(buildInfo.data(), buildInfo.size()));
		}
		else
			CL_CHECK(program);

		return program;
	}
	
	SystemGPU::SystemGPU(OpenCLContext& clContext) :
		clContext(clContext), bufferSetIndex(0)
	{
		if (Graphics::OpenGL::GraphicsContext_OpenGL::IsExtensionSupported("GL_ARB_cl_event"))
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension supported. The application implementation could be made better to use this extension. This is just informative");
		else
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension not supported. Even if it was supported it isn't implemented in the application. This is just informative");

		cl_int ret;

		queue = cl::CommandQueue(clContext.context, clContext.device, cl::QueueProperties::None, &ret);
		CL_CHECK();		


		cl_bool value;
		CL_CALL(clGetDeviceInfo(clContext.device(), CL_DEVICE_PREFERRED_INTEROP_USER_SYNC, sizeof(cl_bool), &value, nullptr))				
	}
	void SystemGPU::Initialize(const SystemInitParameters& initParams)
	{						
		hashesPerParticle = initParams.hashesPerParticle;
		hashesPerStaticParticle = initParams.hashesPerStaticParticle;
		cl_int ret;
		
		Array<StaticParticle> staticParticles;
		Array<DynamicParticle> dynamicParticles;
		Array<uint32> dynamicHashMap;
		Array<uint32> staticHashMap;

		if (initParams.staticParticleGenerationParameters.generator)
			initParams.staticParticleGenerationParameters.generator->Generate(staticParticles);
		if (initParams.dynamicParticleGenerationParameters.generator)
			initParams.dynamicParticleGenerationParameters.generator->Generate(dynamicParticles);
		staticParticleCount = staticParticles.Count();							
		dynamicParticleCount = dynamicParticles.Count();
		
		CL_CALL(queue.finish());
		LoadKernels(initParams.particleBehaviourParameters, initParams.particleBoundParameters);

		GenerateHashes(staticParticles, dynamicHashMap, staticHashMap, initParams.particleBehaviourParameters.maxInteractionDistance);
			
		CreateBuffers(initParams.bufferCount, dynamicParticles, staticParticles, dynamicHashMap, staticHashMap);
	
		EnqueueComputeParticleHashesKernel(&bufferSets[bufferSetIndex].writeFinishedEvent());				
		EnqueuePartialSumKernels();		
		EnqueueComputeParticleMapKernel(ParticleBufferCL(bufferSetIndex));
	}
	void SystemGPU::LoadKernels(const ParticleBehaviourParameters& behaviourParameters, const ParticleBoundParameters& boundingParameters)
	{
		BuildPartialSumProgram();

		//Calculate the maximum
		scanKernelElementCountPerGroup = std::min(
			scanOnComputeGroupsKernel.getWorkGroupInfo< CL_KERNEL_WORK_GROUP_SIZE>(clContext.device),
			addToComputeGroupArraysKernel.getWorkGroupInfo< CL_KERNEL_WORK_GROUP_SIZE>(clContext.device)
		) * 2;

		swapHashMaps = ((uint)std::ceil(std::log2(dynamicParticleCount))) % 2 == 1;
		hashMapSize = hashesPerParticle * dynamicParticleCount;
		staticHashMapSize = hashesPerStaticParticle * staticParticleCount;

		//find the best combination to be > than hashMapSize but still valid
		uint layerCount = std::ceil(std::log(hashMapSize) / std::log(scanKernelElementCountPerGroup));
		scanKernelElementCountPerGroup = 1Ui64 << (uint)std::ceil(std::log2(std::pow<float>(hashMapSize, 1.0f / layerCount)));
		hashMapSize = std::pow(scanKernelElementCountPerGroup, layerCount);

		BuildSPHProgram(behaviourParameters, boundingParameters);
	}
	void SystemGPU::Update(float deltaTime)
	{
		cl_int ret;

		bufferSets[bufferSetIndex].readFinishedFence.BlockClient(0);

		if (clContext.supportedCLGLInterop)
		{
			cl_mem acquireObjects[]{
				ParticleBufferCL(bufferSetIndex)(),
				ParticleBufferCL(nextBufferSetIndex)()
			};

			clEnqueueAcquireGLObjects(queue(), 2, acquireObjects, 0, nullptr, nullptr);
		}

		EnqueueUpdateParticlesPressureKernel();

		EnqueueUpdateParticlesDynamicsKernel(deltaTime);

		if (clContext.supportedCLGLInterop)
		{
			cl_mem acquireObjects[]{
				ParticleBufferCL(bufferSetIndex)(),
				ParticleBufferCL(nextBufferSetIndex)()
			};

			clEnqueueReleaseGLObjects(queue(), 2, acquireObjects, 0, nullptr, &bufferSets[bufferSetIndex].writeFinishedEvent());
		}
		else
			CL_CALL(clEnqueueReadBuffer(queue(), bufferSets[nextBufferSetIndex].noInterop.particleBufferCL(), CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, bufferSets[nextBufferSetIndex].noInterop.particleBufferMap, 0, nullptr, &bufferSets[bufferSetIndex].writeFinishedEvent()))

		std::swap(newHashMapBuffer, hashMapBuffer);		

		const uint32 pattern = 0;
		clEnqueueFillBuffer(queue(), newHashMapBuffer(), &pattern, sizeof(uint32), 0, sizeof(uint32) * hashMapSize, 0, nullptr, nullptr);

		EnqueuePartialSumKernels();

		EnqueueComputeParticleMapKernel(ParticleBufferCL(nextBufferSetIndex));

		//if (particleMoveElapsedTime == 0)
		//{
		//	clFinish(queue());
		//	Array<uint32> particleMap;
		//	particleMap.Resize(dynamicParticleCount);
		//	clEnqueueReadBuffer(queue(), particleMapBuffer(), CL_TRUE, 0, 4 * dynamicParticleCount, particleMap.Ptr(), 0, nullptr, nullptr);
		//
		//	__debugbreak();
		//}

		bufferSetIndex = nextBufferSetIndex;
		nextBufferSetIndex = (bufferSetIndex + 1) % bufferSets.Count();		
	}	
	void SystemGPU::StartRender(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
	{
		bufferSets[bufferSetIndex].writeFinishedEvent.wait();
	}
	Graphics::OpenGLWrapper::VertexArray& SystemGPU::GetDynamicParticlesVertexArray()
	{
		return bufferSets[bufferSetIndex].dynamicParticleVertexArray;
	}
	Graphics::OpenGLWrapper::VertexArray& SystemGPU::GetStaticParticlesVertexArray()
	{
		return bufferSets[bufferSetIndex].staticParticleVertexArray;
	}
	void SystemGPU::EndRender()
	{
		bufferSets[bufferSetIndex].readFinishedFence.SetFence();
	}		
	void SystemGPU::GenerateHashes(Array<StaticParticle>& staticParticles, Array<uint32>& dynamicHashMap, Array<uint32>& staticHashMap, float maxInteractionDistance)
	{
		auto GetStaticParticleHash = [gridSize = maxInteractionDistance, mod = staticHashMapSize](const StaticParticle& particle) {
			return GetHash(Vec3i(particle.position / gridSize)) % mod;
			};

		dynamicHashMap.Resize(hashMapSize + 1, 0);
		dynamicHashMap.Last() = dynamicParticleCount;

		if (staticHashMapSize > 0)
		{
			staticHashMap.Resize(staticHashMapSize + 1);
			GenerateHashMap(staticParticles, staticHashMap, GetStaticParticleHash);
		}

		{
			Array<StaticParticle> stagingStaticParticles;
			stagingStaticParticles.Resize(staticParticleCount);
			for (const auto& particle : staticParticles)
				stagingStaticParticles[--staticHashMap[GetStaticParticleHash(particle)]] = particle;
			std::swap(stagingStaticParticles, staticParticles);
		}
	}
	void SystemGPU::CreateBuffers(uintMem bufferCount, const Array<DynamicParticle>& dynamicParticles, const Array<StaticParticle>& staticParticles, const Array<uint32>& dynamicHashMap, const Array<uint32>& staticHashMap)
	{
		cl_int ret;

		if (bufferCount < 2)
		{
			Debug::Logger::LogWarning("Client", "SystemInitParameters bufferCount member was set to " + StringParsing::Convert(bufferCount) + ". Only values above 1 are valid. The value was set to 2");
			bufferSets = Array<ParticleBufferSet>(2, clContext.supportedCLGLInterop);
		}
		else
			bufferSets = Array<ParticleBufferSet>(bufferCount, clContext.supportedCLGLInterop);

		if (clContext.supportedCLGLInterop)
			for (auto& bufferSet : bufferSets)
			{
				bufferSet.interop.dynamicParticleBufferGL.Allocate(dynamicParticles.Ptr(), sizeof(DynamicParticle) * dynamicParticles.Count());
				bufferSet.interop.staticParticleBufferGL.Allocate(staticParticles.Ptr(), sizeof(StaticParticle) * staticParticles.Count());
				bufferSet.interop.particleBufferCL = cl::BufferGL(clContext.context, CL_MEM_READ_WRITE, bufferSet.interop.dynamicParticleBufferGL.GetHandle(), &ret);

				bufferSet.dynamicParticleVertexArray.EnableVertexAttribute(0);
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, position));
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeBuffer(0, &bufferSet.interop.dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeDivisor(0, 1);

				bufferSet.dynamicParticleVertexArray.EnableVertexAttribute(1);
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, velocity));
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeBuffer(1, &bufferSet.interop.dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeDivisor(1, 1);

				bufferSet.dynamicParticleVertexArray.EnableVertexAttribute(2);
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeFormat(2, Graphics::OpenGLWrapper::VertexAttributeType::Float, 1, false, offsetof(DynamicParticle, pressure));
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeBuffer(2, &bufferSet.interop.dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeDivisor(2, 1);

				bufferSet.staticParticleVertexArray.EnableVertexAttribute(0);
				bufferSet.staticParticleVertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(StaticParticle, position));
				bufferSet.staticParticleVertexArray.SetVertexAttributeBuffer(0, &bufferSet.interop.staticParticleBufferGL, sizeof(StaticParticle), 0);
				bufferSet.staticParticleVertexArray.SetVertexAttributeDivisor(0, 1);
			}
		else 
			for (auto& bufferSet : bufferSets)
			{
				bufferSet.noInterop.dynamicParticleBufferGL.Allocate(dynamicParticles.Ptr(), sizeof(DynamicParticle) * dynamicParticleCount, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentUncoherent);
				bufferSet.noInterop.staticParticleBufferGL.Allocate(staticParticles.Ptr(), sizeof(StaticParticle) * staticParticles.Count());
				bufferSet.noInterop.particleBufferMap = bufferSet.noInterop.dynamicParticleBufferGL.MapBufferRange(0, sizeof(DynamicParticle) * dynamicParticleCount, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush | Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::Unsynchronized);
				bufferSet.noInterop.particleBufferCL = cl::Buffer(clContext.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(DynamicParticle) * dynamicParticleCount, (void*)dynamicParticles.Ptr(), &ret);

				bufferSet.dynamicParticleVertexArray.EnableVertexAttribute(0);
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, position));
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeBuffer(0, &bufferSet.noInterop.dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeDivisor(0, 1);

				bufferSet.dynamicParticleVertexArray.EnableVertexAttribute(1);
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, velocity));
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeBuffer(1, &bufferSet.noInterop.dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeDivisor(1, 1);

				bufferSet.dynamicParticleVertexArray.EnableVertexAttribute(2);
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeFormat(2, Graphics::OpenGLWrapper::VertexAttributeType::Float, 1, false, offsetof(DynamicParticle, pressure));
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeBuffer(2, &bufferSet.noInterop.dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
				bufferSet.dynamicParticleVertexArray.SetVertexAttributeDivisor(2, 1);

				bufferSet.staticParticleVertexArray.EnableVertexAttribute(0);
				bufferSet.staticParticleVertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(StaticParticle, position));
				bufferSet.staticParticleVertexArray.SetVertexAttributeBuffer(0, &bufferSet.noInterop.staticParticleBufferGL, sizeof(StaticParticle), 0);
				bufferSet.staticParticleVertexArray.SetVertexAttributeDivisor(0, 1);
			}

		bufferSetIndex = 0;
		nextBufferSetIndex = (bufferSetIndex + 1) % bufferSets.Count();

		hashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(uint32) * dynamicHashMap.Count(), (void*)dynamicHashMap.Ptr(), &ret);
		CL_CHECK();
		newHashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(uint32) * dynamicHashMap.Count(), (void*)dynamicHashMap.Ptr(), &ret);
		CL_CHECK();
		particleMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * dynamicParticleCount, nullptr, &ret);
		CL_CHECK();
		if (!staticParticles.Empty())
		{
			staticHashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(uint32) * staticHashMap.Count(), (void*)staticHashMap.Ptr(), &ret);
			CL_CHECK();
			staticParticleBuffer = cl::Buffer(clContext.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(StaticParticle) * staticParticles.Count(), (void*)staticParticles.Ptr(), &ret);
			CL_CHECK();
		}
	}
	void SystemGPU::BuildPartialSumProgram()
	{
		cl_int ret;

		partialSumProgram = BuildOpenCLProgram(clContext, Array<Path>{ "kernels/partialSum.cl" }, {});

		scanOnComputeGroupsKernel = cl::Kernel(partialSumProgram, "scanOnComputeGroups", &ret);
		CL_CHECK();
		addToComputeGroupArraysKernel = cl::Kernel(partialSumProgram, "addToComputeGroupArrays", &ret);
		CL_CHECK();
	}
	void SystemGPU::BuildSPHProgram(const ParticleBehaviourParameters& behaviourParameters, const ParticleBoundParameters& boundParameters)
	{
		cl_int ret;

		float smoothingKernelConstant = SmoothingKernelConstant(behaviourParameters.maxInteractionDistance);
		float selfDensity = behaviourParameters.particleMass * SmoothingKernelD0(0, behaviourParameters.maxInteractionDistance) * smoothingKernelConstant;
		Vec3f boxPoint1 = boundParameters.boxOffset;
		Vec3f boxPoint2 = boundParameters.boxOffset + boundParameters.boxSize;
		
		Map<String, String> values = {
			{ "CL_COMPILER" },
			{ "PARTICLE_MASS",            "(float)" + StringParsing::Convert(behaviourParameters.particleMass) },
			{ "GAS_CONSTANT",             "(float)" + StringParsing::Convert(behaviourParameters.gasConstant) },
			{ "ELASTICITY",              "(float)" + StringParsing::Convert(behaviourParameters.elasticity) },
			{ "VISCOSITY",               "(float)" + StringParsing::Convert(behaviourParameters.viscosity) },

			{ "BOUNDING_BOX_POINT_1",       "(float3)(" + StringParsing::Convert(boxPoint1.x) + "," + StringParsing::Convert(boxPoint1.x) + "," + StringParsing::Convert(boxPoint1.x) + ")" },
			{ "BOUNDING_BOX_POINT_2",       "(float3)(" + StringParsing::Convert(boxPoint2.x) + "," + StringParsing::Convert(boxPoint2.x) + "," + StringParsing::Convert(boxPoint2.x) + ")" },
			{ "MAX_INTERACTION_DISTANCE",  "(float)" + StringParsing::Convert(behaviourParameters.maxInteractionDistance) },
			{ "REST_DENSITY",             "(float)" + StringParsing::Convert(behaviourParameters.restDensity) },
			{ "GRAVITY",                 "(float3)(" + StringParsing::Convert(behaviourParameters.gravity.x) + "," + StringParsing::Convert(behaviourParameters.gravity.y) + "," + StringParsing::Convert(behaviourParameters.gravity.z) + ")" },

			{ "SMOOTHING_KERNEL_CONSTANT", "(float)" + StringParsing::Convert(smoothingKernelConstant) },
			{ "SELF_DENSITY",             "(float)" + StringParsing::Convert(selfDensity) },
			{ "HASH_MAP_SIZE",             "(uint)" + StringParsing::Convert(hashMapSize)},
			{ "PARTICLE_COUNT",           "(uint)" + StringParsing::Convert(dynamicParticleCount) },
			{ "STATIC_HASH_MAP_SIZE",       StringParsing::Convert(staticHashMapSize) },
			{ "STATIC_PARTICLE_COUNT",     StringParsing::Convert(staticParticleCount)  }
		};

		if (boundParameters.bounded) values.Insert("BOUND_PARTICLES");
		if (boundParameters.boundedByRoof) values.Insert("BOUND_TOP");
		if (boundParameters.boundedByWalls) values.Insert("BOUND_WALLS");

		SPHProgram = BuildOpenCLProgram(clContext, Array<Path>{ "kernels/CL_CPP_SPHFunctions.h", "kernels/SPH.cl" }, values);
		
		computeParticleHashesKernel = cl::Kernel(SPHProgram, "computeParticleHashes", &ret);
		CL_CHECK();
		computeParticleMapKernel = cl::Kernel(SPHProgram, "computeParticleMap", &ret);
		CL_CHECK();
		updateParticlesPressureKernel = cl::Kernel(SPHProgram, "updateParticlesPressure", &ret);
		CL_CHECK();
		updateParticlesDynamicsKernel = cl::Kernel(SPHProgram, "updateParticlesDynamics", &ret);
		CL_CHECK();
	}	
	void SystemGPU::EnqueueComputeParticleHashesKernel(cl_event* event)
	{
		cl_int ret;

		CL_CALL(computeParticleHashesKernel.setArg(0, ParticleBufferCL(bufferSetIndex)));
		CL_CALL(computeParticleHashesKernel.setArg(1, hashMapBuffer));
		CL_CALL(computeParticleHashesKernel.setArg(2, particleMapBuffer));
		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = 1;
		CL_CALL(clEnqueueNDRangeKernel(queue(), computeParticleHashesKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, event));
	}

	void SystemGPU::EnqueueComputeParticleMapKernel(cl::Buffer& particleBuffer)
	{
		cl_int ret;

		CL_CALL(computeParticleMapKernel.setArg(0, particleBuffer));		
		CL_CALL(computeParticleMapKernel.setArg(1, hashMapBuffer));		
		CL_CALL(computeParticleMapKernel.setArg(2, particleMapBuffer));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = 1;
		CL_CALL(clEnqueueNDRangeKernel(queue(), computeParticleMapKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));	
	}
	void SystemGPU::EnqueueUpdateParticlesPressureKernel()
	{
		cl_int ret;

		CL_CALL(updateParticlesPressureKernel.setArg(0, ParticleBufferCL(bufferSetIndex)));
		CL_CALL(updateParticlesPressureKernel.setArg(1, hashMapBuffer));		
		CL_CALL(updateParticlesPressureKernel.setArg(2, particleMapBuffer));						
		CL_CALL(updateParticlesPressureKernel.setArg(3, staticHashMapBuffer));
		CL_CALL(updateParticlesPressureKernel.setArg(4, staticParticleBuffer));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = 1;
		CL_CALL(clEnqueueNDRangeKernel(queue(), updateParticlesPressureKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));		
	}
	void SystemGPU::EnqueueUpdateParticlesDynamicsKernel(float deltaTime)
	{
		bool moveParticles = false;
		//if (particleMoveElapsedTime > 1.0f)
		//{
		//	moveParticles = true;
		//	particleMoveElapsedTime = 0.0f;
		//}
		//else
		//	particleMoveElapsedTime += deltaTime;

		cl_int ret;

		CL_CALL(updateParticlesDynamicsKernel.setArg(0, ParticleBufferCL(bufferSetIndex)));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(1, ParticleBufferCL(nextBufferSetIndex)));
		CL_CALL(updateParticlesDynamicsKernel.setArg(2, hashMapBuffer));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(3, newHashMapBuffer));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(4, particleMapBuffer));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(5, staticParticleBuffer));
		CL_CALL(updateParticlesDynamicsKernel.setArg(6, staticHashMapBuffer));
		CL_CALL(updateParticlesDynamicsKernel.setArg(7, deltaTime));
		CL_CALL(updateParticlesDynamicsKernel.setArg(8, (int)moveParticles));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = 1;
		CL_CALL(clEnqueueNDRangeKernel(queue(), updateParticlesDynamicsKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));		
	}
	void SystemGPU::EnqueuePartialSumKernels()
	{		
		cl_int ret;				

		for (uintMem size = hashMapSize; size != 1; size /= scanKernelElementCountPerGroup)
		{									
			CL_CALL(scanOnComputeGroupsKernel.setArg(0, (scanKernelElementCountPerGroup + 1) * sizeof(uint32), nullptr));
			CL_CALL(scanOnComputeGroupsKernel.setArg(1, hashMapBuffer));
			CL_CALL(scanOnComputeGroupsKernel.setArg(2, (uint32)(hashMapSize / size)));

			size_t globalWorkOffset = 0;
			size_t globalWorkSize = size / 2;
			size_t localWorkSize = scanKernelElementCountPerGroup / 2;
			CL_CALL(clEnqueueNDRangeKernel(queue(), scanOnComputeGroupsKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));			
		}

		for (uintMem i = 1; i != hashMapSize / scanKernelElementCountPerGroup; i *= scanKernelElementCountPerGroup)
		{			
			CL_CALL(addToComputeGroupArraysKernel.setArg(0, hashMapBuffer));
			CL_CALL(addToComputeGroupArraysKernel.setArg(1, (uint32)i));			

			size_t globalWorkOffset = 0;
			size_t globalWorkSize = hashMapSize / i - scanKernelElementCountPerGroup - scanKernelElementCountPerGroup + 1;
			size_t localWorkSize = scanKernelElementCountPerGroup - 1;
			CL_CALL(clEnqueueNDRangeKernel(queue(), addToComputeGroupArraysKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));
		}
			
	}
	cl::Buffer& SystemGPU::ParticleBufferCL(uintMem index)
	{
		if (clContext.supportedCLGLInterop)
			return bufferSets[index].interop.particleBufferCL;
		else
			return bufferSets[index].noInterop.particleBufferCL;
	}		
	SystemGPU::ParticleBufferSet::ParticleBufferSet(bool hasInterop) 		
		: hasInterop(hasInterop)
	{
		if (hasInterop)
			std::construct_at(&interop);
		else
			std::construct_at(&noInterop);
	}
	SystemGPU::ParticleBufferSet::~ParticleBufferSet()
	{
		if (hasInterop)
			std::destroy_at(&interop);
		else
			std::destroy_at(&noInterop);
	}
}
