#include "pch.h"
#include "SPHSystemGPU.h"
#include "SPHSystemGPURenderer.h"
#include "gl/glew.h"
#include "GL/glext.h"
#undef min

namespace SPH
{
	static void ParseOpenCLSource(String& source, const Map<String, String>& values)
	{
		auto tokens = StringParsing::Split(source, ' ');
		
		source.Clear();
		for (auto& token : tokens)
		{
			if (token[0] == '$')
			{
				String valueName = StringParsing::RemoveSurrounding(token, '$');

				auto it = values.Find(valueName);
				if (!it.IsNull())				
					token = it->value;												
			}

			source += token + " ";
		}
	}
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

	uint ceilToMultiple(uint num, uint multiple)
	{
		return (num + multiple - 1) / multiple * multiple;
	}

	SystemGPU::SystemGPU(OpenCLContext& clContext) :
		clContext(clContext), bufferSetIndex(0)
	{
		if (Graphics::OpenGL::GraphicsContext_OpenGL::IsExtensionSupported("GL_ARB_cl_event"))
			Debug::Logger::LogFatal("Client", "GL_ARB_cl_event extensions not supported!");

		cl_int ret;

		queue = cl::CommandQueue(clContext.context, clContext.device, cl::QueueProperties::None, &ret);
		CL_CHECK();		

		cl_bool value;
		CL_CALL(clGetDeviceInfo(clContext.device(), CL_DEVICE_PREFERRED_INTEROP_USER_SYNC, sizeof(cl_bool), &value, nullptr))

		value = value;
	}

	void SystemGPU::Initialize(const SystemInitParameters& initParams)
	{						
		cl_int ret;

		CL_CALL(queue.finish());

		Array<Particle> particles = GenerateParticles(
			initParams.dynamicParticleGenerationParameters,
			initParams.staticParticleGenerationParameters,
			dynamicParticleCount
		);		
		particleCount = particles.Count();
		
		partialSumProgram = BuildOpenCLProgram(clContext, Array<Path>{ "kernels/partialSum.cl" }, {});		

		scanOnComputeGroupsKernel = cl::Kernel(partialSumProgram, "scanOnComputeGroups", &ret);
		CL_CHECK();
		addToComputeGroupArraysKernel = cl::Kernel(partialSumProgram, "addToComputeGroupArrays", &ret);
		CL_CHECK();

		//Calculate the maximum
		scanKernelElementCountPerGroup = std::min(
			scanOnComputeGroupsKernel.getWorkGroupInfo< CL_KERNEL_WORK_GROUP_SIZE>(clContext.device),
			addToComputeGroupArraysKernel.getWorkGroupInfo< CL_KERNEL_WORK_GROUP_SIZE>(clContext.device)
		) * 2;		
		
		float smoothingKernelConstant = SmoothingKernelConstant(initParams.maxInteractionDistance);
		float selfDensity = initParams.particleMass * SmoothingKernelD0(0, initParams.maxInteractionDistance, smoothingKernelConstant);				
		swapHashMaps = ((uint)std::ceil(std::log2(particleCount))) % 2 == 1;
		hashMapSize = hashesPerParticle * particleCount;					
				
		//find the best combination to be > than hashMapSize but still valid
		uint layerCount = std::ceil(std::log(hashMapSize) / std::log(scanKernelElementCountPerGroup));
		scanKernelElementCountPerGroup = 1 << (uint)std::ceil(std::log2(std::pow<float>(hashMapSize, 1.0f / layerCount)));
		hashMapSize = std::pow(scanKernelElementCountPerGroup, layerCount);

		//TODO settle this
		maxInteractionDistance = initParams.maxInteractionDistance;

		SPHProgram = BuildOpenCLProgram(clContext, Array<Path>{ "kernels/SPH.cl" }, {
			{ "particleMass",            "(float)" + StringParsing::Convert(initParams.particleMass) },
			{ "gasConstant",             "(float)" + StringParsing::Convert(initParams.gasConstant) },
			{ "elasticity",              "(float)" + StringParsing::Convert(initParams.elasticity) },
			{ "viscosity",               "(float)" + StringParsing::Convert(initParams.viscosity) },
			 
			{ "boundingBoxSize",         "(float3)(" + StringParsing::Convert(initParams.boundingBoxSize.x) + "," + StringParsing::Convert(initParams.boundingBoxSize.y) + "," + StringParsing::Convert(initParams.boundingBoxSize.z) + ")" },
			{ "maxInteractionDistance",  "(float)" + StringParsing::Convert(initParams.maxInteractionDistance) },
			{ "restDensity",             "(float)" + StringParsing::Convert(initParams.restDensity) },
			 
			{ "smoothingKernelConstant", "(float)" + StringParsing::Convert(smoothingKernelConstant) },
			{ "selfDensity",             "(float)" + StringParsing::Convert(selfDensity) },						
			{ "hashMapSize",             "(uint)" + StringParsing::Convert(hashMapSize)},
			{ "particleCount",           "(uint)" + StringParsing::Convert(particleCount) }
		});		

		computeParticleHashesKernel = cl::Kernel(SPHProgram, "computeParticleHashes", &ret);
		CL_CHECK();		
		computeParticleMapKernel = cl::Kernel(SPHProgram, "computeParticleMap", &ret);
		CL_CHECK();
		updateParticlesPressureKernel = cl::Kernel(SPHProgram, "updateParticlesPressure", &ret);
		CL_CHECK();		
		updateParticlesDynamicsKernel = cl::Kernel(SPHProgram, "updateParticlesDynamics", &ret);		
		CL_CHECK();
				
		if (initParams.bufferCount < 2)
		{
			Debug::Logger::LogWarning("Client", "SystemInitParameters bufferCount member was set to " + StringParsing::Convert(initParams.bufferCount) + ". Only values above 1 are valid. The value was set to 2");			
			bufferSets.Resize(2);
		}
		else
			bufferSets.Resize(initParams.bufferCount);

		for (auto& bufferSet : bufferSets)
		{
			bufferSet.particleBufferGL.Allocate(particles.Ptr(), sizeof(Particle) * particleCount, Graphics::OpenGLWrapper::MutableGraphicsBufferUseFrequency::Dynamic);

			bufferSet.vertexArray.EnableVertexAttribute(0);
			bufferSet.vertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(Particle, position));
			bufferSet.vertexArray.SetVertexAttributeBuffer(0, &bufferSet.particleBufferGL, sizeof(Particle), 0);
			bufferSet.vertexArray.SetVertexAttributeDivisor(0, 1);
			bufferSet.vertexArray.EnableVertexAttribute(1);
			bufferSet.vertexArray.SetVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 4, false, offsetof(Particle, color));
			bufferSet.vertexArray.SetVertexAttributeBuffer(1, &bufferSet.particleBufferGL, sizeof(Particle), 0);
			bufferSet.vertexArray.SetVertexAttributeDivisor(1, 1);			

			bufferSet.particleBufferCL = cl::Buffer(clContext.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(Particle) * particleCount, particles.Ptr(), &ret);
			CL_CHECK();

			bufferSet.particles.Resize(particleCount);
			//bufferSet.particleBufferCL = cl::BufferGL(clContext.context, CL_MEM_READ_WRITE, bufferSet.particleBufferGL.GetHandle(), &ret);
			//CL_CHECK();

			//bufferSet.readFinishedEvent = clCreateEventFromGLsyncKHR(clContext.context(), (cl_GLsync)bufferSet.readFinishedFence.GetHandle(), &ret);
			//CL_CHECK();					
			//			
			//bufferSet.writeFinishedFence = Graphics::OpenGLWrapper::Fence(glCreateSyncFromCLeventARB(clContext.context(), bufferSet.writeFinishedEvent(), 0));
		}
		bufferSetIndex = 0;
		nextBufferSetIndex = (bufferSetIndex + 1) % bufferSets.Count();		

		Array<uintMem> hashes;
		hashes.Resize(hashMapSize + 1);
		hashes.Last() = particleCount;		
		
		hashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(uint32) * (hashMapSize + 1), hashes.Ptr(), &ret);
		CL_CHECK();		
		newHashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(uint32) * (hashMapSize + 1), hashes.Ptr(), &ret);
		CL_CHECK();
		particleMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * particleCount, nullptr, &ret);
		CL_CHECK();		
	
		//clEnqueueAcquireGLObjects(queue(), 1, &bufferSets[bufferSetIndex].particleBufferCL(), 0, nullptr, nullptr);
		//CL_CALL(queue.finish());
		EnqueueComputeParticleHashesKernel();
		CL_CALL(queue.finish());
		//clEnqueueReleaseGLObjects(queue(), 1, &bufferSets[nextBufferSetIndex].particleBufferCL(), 0, nullptr, nullptr);
		//CL_CALL(queue.finish());
		EnqueuePartialSumKernels();
		CL_CALL(queue.finish());

	}

	void SystemGPU::Update(float deltaTime)
	{		
		cl_int ret;

										
		CL_CALL(queue.finish());

		//clEnqueueAcquireGLObjects(queue(), 1, &bufferSets[bufferSetIndex].particleBufferCL(), 0, nullptr, nullptr);
		//CL_CALL(queue.finish());
		//clEnqueueAcquireGLObjects(queue(), 1, &bufferSets[nextBufferSetIndex].particleBufferCL(), 0, nullptr, nullptr);
		//CL_CALL(queue.finish());

		EnqueueComputeParticleMapKernel();
		CL_CALL(queue.finish());
		EnqueueUpdateParticlesPressureKernel();
		CL_CALL(queue.finish());
		//clEnqueueAcquireGLObjects(queue(), 1, &bufferSets[nextBufferSetIndex].particleBufferCL(), 1, &bufferSets[nextBufferSetIndex].readFinishedEvent(), nullptr);
		EnqueueUpdateParticlesDynamicsKernel(deltaTime);
		CL_CALL(queue.finish());
		
		CL_CALL(queue.enqueueReadBuffer(bufferSets[nextBufferSetIndex].particleBufferCL, CL_TRUE, 0, sizeof(Particle) * particleCount, bufferSets[nextBufferSetIndex].particles.Ptr()))
		bufferSets[nextBufferSetIndex].particleBufferGL.WriteData(bufferSets[nextBufferSetIndex].particles.Ptr(), sizeof(Particle) * particleCount, 0);
		glFinish();

		//clEnqueueReleaseGLObjects(queue(), 1, &bufferSets[nextBufferSetIndex].particleBufferCL(), 0, nullptr, nullptr);
		//CL_CALL(queue.finish());
		//clEnqueueReleaseGLObjects(queue(), 1, &bufferSets[bufferSetIndex].particleBufferCL(), 0, nullptr, nullptr);		
		//CL_CALL(queue.finish());

		std::swap(newHashMapBuffer, hashMapBuffer);		

		const uint32 pattern = 0;
		clEnqueueFillBuffer(queue(), newHashMapBuffer(), &pattern, sizeof(uint32), 0, sizeof(uint) * hashMapSize, 0, nullptr, nullptr);

		EnqueuePartialSumKernels();

		CL_CALL(queue.finish());


		bufferSetIndex = nextBufferSetIndex;
		nextBufferSetIndex = (bufferSetIndex + 1) % bufferSets.Count();
	}
	Array<Particle> SystemGPU::GetParticles()
	{
		return Array<Particle>();
	}
	void SystemGPU::StartRender(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
	{
	//	bufferSets[bufferSetIndex].writeFinishedFence.BlockServer();
		graphicsContext.SelectVertexArray(&bufferSets[bufferSetIndex].vertexArray);
	}
	void SystemGPU::EndRender()
	{
	//	bufferSets[bufferSetIndex].readFinishedFence.SetFence();
	}

	/*	
	Array<Particle> SystemGPU::GetParticles()
	{
		cl_int ret;
		Array<Particle> particles{ particleCount };
		CL_CALL(queue.enqueueReadBuffer(particleBuffer, CL_TRUE, 0, sizeof(Particle) * particleCount, particles.Ptr()), { })
		CL_CALL(queue.finish(), { });
		return particles;
	}
	uint getHash(Vec3i cell, uintMem hashMapSize)
	{
		return (
			(uint)(cell.x * 73856093)
			^ (uint)(cell.y * 19349663)
			^ (uint)(cell.z * 83492791)) % hashMapSize;
	}
	Array<uintMem> SystemGPU::FindNeighbors(Array<Particle>& particles, Vec3f position)
	{
		cl_int ret;
		Array<uint32> hashMap;
		Array<uint32> particleMap;
		hashMap.Resize(hashMapSize + 1);
		particleMap.Resize(particleCount);
		CL_CALL(queue.enqueueReadBuffer(hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (hashMapSize + 1), hashMap.Ptr()), { });
		CL_CALL(queue.enqueueReadBuffer(particleMapBuffer, CL_TRUE, 0, sizeof(uint32) * particleCount, particleMap.Ptr()), { });
		CL_CALL(queue.finish(), { });		
				

		Vec3i cell = Vec3i(position / maxInteractionDistance);

		Vec3i beginCell = cell - (Vec3i)(1, 1, 1);
		Vec3i endCell = cell + (Vec3i)(2, 2, 2);

		float influenceSum = 0;

		Array<uintMem> indicies;

		Vec3i otherCell;
		for (otherCell.x = beginCell.x; otherCell.x < endCell.x; ++otherCell.x)
			for (otherCell.y = beginCell.y; otherCell.y < endCell.y; ++otherCell.y)
				for (otherCell.z = beginCell.z; otherCell.z < endCell.z; ++otherCell.z)
				{
					uint otherHash = getHash(otherCell, hashMapSize);

					uint beginIndex = hashMap[otherHash];
					uint endIndex = hashMap[otherHash + 1];

					for (uint i = beginIndex; i < endIndex; ++i)
					{					
						uintMem index = particleMap[i];
						struct Particle otherParticle = particles[index];

						Vec3f dir = otherParticle.position - position;
						float distSqr = dir.SqrLenght();

 						if (distSqr > maxInteractionDistance * maxInteractionDistance)
							continue;

						indicies.AddBack(index);
					}
				}

		return indicies;
	}
	*/

	void SystemGPU::EnqueueComputeParticleHashesKernel()
	{
		cl_int ret;

		CL_CALL(computeParticleHashesKernel.setArg(0, bufferSets[bufferSetIndex].particleBufferCL));
		CL_CALL(computeParticleHashesKernel.setArg(1, hashMapBuffer));
		CL_CALL(computeParticleHashesKernel.setArg(2, particleMapBuffer));
		CL_CALL(queue.enqueueNDRangeKernel(computeParticleHashesKernel, 0, particleCount, 1));
	}

	void SystemGPU::EnqueueComputeParticleMapKernel()
	{
		cl_int ret;

		CL_CALL(computeParticleMapKernel.setArg(0, bufferSets[bufferSetIndex].particleBufferCL));
		CL_CALL(queue.finish());
		CL_CALL(computeParticleMapKernel.setArg(1, hashMapBuffer));
		CL_CALL(queue.finish());
		CL_CALL(computeParticleMapKernel.setArg(2, particleMapBuffer));		
		CL_CALL(queue.finish());

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = particleCount;
		size_t localWorkSize = 1;
		CL_CALL(clEnqueueNDRangeKernel(queue(), computeParticleMapKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));
		CL_CALL(queue.finish());
	}
	void SystemGPU::EnqueueUpdateParticlesPressureKernel()
	{
		cl_int ret;

		CL_CALL(updateParticlesPressureKernel.setArg(0, bufferSets[bufferSetIndex].particleBufferCL));
		CL_CALL(queue.finish());
		CL_CALL(updateParticlesPressureKernel.setArg(1, hashMapBuffer));
		CL_CALL(queue.finish());
		CL_CALL(updateParticlesPressureKernel.setArg(2, particleMapBuffer));				
		CL_CALL(queue.finish());

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = particleCount;
		size_t localWorkSize = 1;
		CL_CALL(clEnqueueNDRangeKernel(queue(), updateParticlesPressureKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));
		CL_CALL(queue.finish());
	}
	void SystemGPU::EnqueueUpdateParticlesDynamicsKernel(float deltaTime)
	{
		cl_int ret;

		CL_CALL(updateParticlesDynamicsKernel.setArg(0, bufferSets[bufferSetIndex].particleBufferCL));
		CL_CALL(queue.finish());
		CL_CALL(updateParticlesDynamicsKernel.setArg(1, bufferSets[nextBufferSetIndex].particleBufferCL));
		CL_CALL(queue.finish());
		CL_CALL(updateParticlesDynamicsKernel.setArg(2, hashMapBuffer));
		CL_CALL(queue.finish());
		CL_CALL(updateParticlesDynamicsKernel.setArg(3, newHashMapBuffer));
		CL_CALL(queue.finish());
		CL_CALL(updateParticlesDynamicsKernel.setArg(4, particleMapBuffer));
		CL_CALL(queue.finish());
		CL_CALL(updateParticlesDynamicsKernel.setArg(5, deltaTime));
		CL_CALL(queue.finish());

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = particleCount;
		size_t localWorkSize = 1;
		CL_CALL(clEnqueueNDRangeKernel(queue(), updateParticlesDynamicsKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));
		CL_CALL(queue.finish());
	}
	void SystemGPU::EnqueuePartialSumKernels()
	{		
		cl_int ret;		

		for (uintMem size = hashMapSize; size != 1; size /= scanKernelElementCountPerGroup)
		{									
			CL_CALL(scanOnComputeGroupsKernel.setArg(0, (scanKernelElementCountPerGroup + 1) * sizeof(uint), nullptr));
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
			size_t globalWorkSize = hashMapSize / i - scanKernelElementCountPerGroup;
			size_t localWorkSize = scanKernelElementCountPerGroup - 1;
			CL_CALL(clEnqueueNDRangeKernel(queue(), addToComputeGroupArraysKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));
		}
	}
}
