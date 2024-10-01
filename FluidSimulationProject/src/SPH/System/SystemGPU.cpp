#include "pch.h"
#include "SystemGPU.h"
#include "SPH/Graphics/SystemGPURenderer.h"
#include "SPH/SPHFunctions.h"
#include "gl/glew.h"
#include "GL/glext.h"

namespace SPH
{	
	ParticleBufferSet::ParticleBufferSet(const Array<StaticParticle>& staticParticles)
	{
		staticParticleBufferGL.Allocate(staticParticles.Ptr(), sizeof(StaticParticle) * staticParticles.Count());		
	}	

	ParticleBufferSetInterop::ParticleBufferSetInterop(OpenCLContext& clContext, const Array<DynamicParticle>& dynamicParticles, const Array<StaticParticle>& staticParticles) :
		ParticleBufferSet(staticParticles)
	{
		cl_int ret;

		dynamicParticleBufferGL.Allocate(dynamicParticles.Ptr(), sizeof(DynamicParticle) * dynamicParticles.Count());		
		dynamicParticleBufferCL = cl::BufferGL(clContext.context, CL_MEM_READ_WRITE, dynamicParticleBufferGL.GetHandle(), &ret);
		CL_CHECK();

#ifdef VISUALIZE_NEIGHBOURS
		dynamicParticleColorBufferGL.Allocate(nullptr, sizeof(uint32) * dynamicParticles.Count());
		staticParticleColorBufferGL.Allocate(nullptr, sizeof(uint32) * staticParticles.Count());

		dynamicParticleColorBufferCL = cl::BufferGL(clContext.context, CL_MEM_READ_WRITE, dynamicParticleColorBufferGL.GetHandle(), &ret);
		CL_CHECK();
		staticParticleColorBufferCL = cl::BufferGL(clContext.context, CL_MEM_READ_WRITE, staticParticleColorBufferGL.GetHandle(), &ret);
		CL_CHECK();
#endif



		dynamicParticleVertexArray.EnableVertexAttribute(0);
		dynamicParticleVertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, position));
		dynamicParticleVertexArray.SetVertexAttributeBuffer(0, &dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(0, 1);

		dynamicParticleVertexArray.EnableVertexAttribute(1);
		dynamicParticleVertexArray.SetVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, velocity));
		dynamicParticleVertexArray.SetVertexAttributeBuffer(1, &dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(1, 1);

		dynamicParticleVertexArray.EnableVertexAttribute(2);
		dynamicParticleVertexArray.SetVertexAttributeFormat(2, Graphics::OpenGLWrapper::VertexAttributeType::Float, 1, false, offsetof(DynamicParticle, pressure));
		dynamicParticleVertexArray.SetVertexAttributeBuffer(2, &dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(2, 1);

		staticParticleVertexArray.EnableVertexAttribute(0);
		staticParticleVertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(StaticParticle, position));
		staticParticleVertexArray.SetVertexAttributeBuffer(0, &staticParticleBufferGL, sizeof(StaticParticle), 0);
		staticParticleVertexArray.SetVertexAttributeDivisor(0, 1);

#ifdef VISUALIZE_NEIGHBOURS
		dynamicParticleVertexArray.EnableVertexAttribute(3);
		dynamicParticleVertexArray.SetVertexAttributeFormat(3, Graphics::OpenGLWrapper::VertexAttributeType::Uint32, 1, false, 0);
		dynamicParticleVertexArray.SetVertexAttributeBuffer(3, &dynamicParticleColorBufferGL, sizeof(float), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(3, 1);

		staticParticleVertexArray.EnableVertexAttribute(3);
		staticParticleVertexArray.SetVertexAttributeFormat(3, Graphics::OpenGLWrapper::VertexAttributeType::Uint32, 1, false, 0);
		staticParticleVertexArray.SetVertexAttributeBuffer(3, &staticParticleColorBufferGL, sizeof(float), 0);
		staticParticleVertexArray.SetVertexAttributeDivisor(3, 1);
#endif
	}
	void ParticleBufferSetInterop::StartRender(cl::CommandQueue& queue)
	{
		readFinishedFence.Clear();
		writeFinishedEvent.wait();
	}
	void ParticleBufferSetInterop::EndRender(cl::CommandQueue& queue)
	{
		readFinishedFence.SetFence();
	}
	void ParticleBufferSetInterop::StartSimulationRead(cl::CommandQueue& queue)
	{
		cl_mem acquireObjects[]{
			dynamicParticleBufferCL()
		};

		clEnqueueAcquireGLObjects(queue(), _countof(acquireObjects), acquireObjects, 0, nullptr, nullptr);
	}
	void ParticleBufferSetInterop::StartSimulationWrite(cl::CommandQueue& queue)
	{
		readFinishedFence.BlockClient(2);

		cl_mem acquireObjects[]{
			dynamicParticleBufferCL(),
#ifdef VISUALIZE_NEIGHBOURS		
			dynamicParticleColorBufferCL(),
			staticParticleColorBufferCL(),
#endif
		};

		clEnqueueAcquireGLObjects(queue(), _countof(acquireObjects), acquireObjects, 0, nullptr, nullptr);
	}
	void ParticleBufferSetInterop::EndSimulationRead(cl::CommandQueue& queue)
	{
		cl_mem acquireObjects[]{
			dynamicParticleBufferCL()
		};

		clEnqueueReleaseGLObjects(queue(), _countof(acquireObjects), acquireObjects, 0, nullptr, &writeFinishedEvent());
	}
	void ParticleBufferSetInterop::EndSimulationWrite(cl::CommandQueue& queue)
	{
		cl_mem acquireObjects[]{
			dynamicParticleBufferCL(),
#ifdef VISUALIZE_NEIGHBOURS		
			dynamicParticleColorBufferCL(),
			staticParticleColorBufferCL(),
#endif
		};

		clEnqueueReleaseGLObjects(queue(), _countof(acquireObjects), acquireObjects, 0, nullptr, &writeFinishedEvent());
	}
	
	
	ParticleBufferSetNoInterop::ParticleBufferSetNoInterop(OpenCLContext& clContext, const Array<DynamicParticle>& dynamicParticles, const Array<StaticParticle>& staticParticles) :
		ParticleBufferSet(staticParticles), dynamicParticleCount(dynamicParticles.Count()), staticParticleCount(staticParticles.Count())
	{
		cl_int ret;

		dynamicParticleBufferGL.Allocate(dynamicParticles.Ptr(), sizeof(DynamicParticle) * dynamicParticleCount, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentUncoherent);
		dynamicParticleBufferMap = dynamicParticleBufferGL.MapBufferRange(0, sizeof(DynamicParticle) * dynamicParticleCount, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush);

		dynamicParticleBufferCL = cl::Buffer(clContext.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(DynamicParticle) * dynamicParticleCount, (void*)dynamicParticles.Ptr(), &ret);
		CL_CHECK();

#ifdef VISUALIZE_NEIGHBOURS
		dynamicParticleColorBufferGL.Allocate(nullptr, sizeof(uint32) * dynamicParticleCount, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentUncoherent);
		staticParticleColorBufferGL.Allocate(nullptr, sizeof(uint32) * staticParticleCount, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentUncoherent);

		dynamicParticleColorBufferCL = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * dynamicParticleCount, nullptr, &ret);
		CL_CHECK();
		staticParticleColorBufferCL = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * staticParticleCount, nullptr, &ret);
		CL_CHECK();
		
		dynamicParticleColorBufferMap = dynamicParticleColorBufferGL.MapBufferRange(0, sizeof(uint32) * dynamicParticleCount, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush);
		staticParticleColorBufferMap = staticParticleColorBufferGL.MapBufferRange(0, sizeof(uint32) * staticParticleCount, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush);
#endif

		dynamicParticleVertexArray.EnableVertexAttribute(0);
		dynamicParticleVertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, position));
		dynamicParticleVertexArray.SetVertexAttributeBuffer(0, &dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(0, 1);

		dynamicParticleVertexArray.EnableVertexAttribute(1);
		dynamicParticleVertexArray.SetVertexAttributeFormat(1, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(DynamicParticle, velocity));
		dynamicParticleVertexArray.SetVertexAttributeBuffer(1, &dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(1, 1);

		dynamicParticleVertexArray.EnableVertexAttribute(2);
		dynamicParticleVertexArray.SetVertexAttributeFormat(2, Graphics::OpenGLWrapper::VertexAttributeType::Float, 1, false, offsetof(DynamicParticle, pressure));
		dynamicParticleVertexArray.SetVertexAttributeBuffer(2, &dynamicParticleBufferGL, sizeof(DynamicParticle), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(2, 1);

		staticParticleVertexArray.EnableVertexAttribute(0);
		staticParticleVertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(StaticParticle, position));
		staticParticleVertexArray.SetVertexAttributeBuffer(0, &staticParticleBufferGL, sizeof(StaticParticle), 0);
		staticParticleVertexArray.SetVertexAttributeDivisor(0, 1);

		dynamicParticleVertexArray.DisableVertexAttribute(3);
		staticParticleVertexArray.DisableVertexAttribute(3);
		glVertexAttrib1ui(,);

#ifdef VISUALIZE_NEIGHBOURS
		dynamicParticleVertexArray.EnableVertexAttribute(3);
		dynamicParticleVertexArray.SetVertexAttributeFormat(3, Graphics::OpenGLWrapper::VertexAttributeType::Uint32, 1, false, 0);
		dynamicParticleVertexArray.SetVertexAttributeBuffer(3, &dynamicParticleColorBufferGL, sizeof(float), 0);
		dynamicParticleVertexArray.SetVertexAttributeDivisor(3, 1);

		staticParticleVertexArray.EnableVertexAttribute(3);
		staticParticleVertexArray.SetVertexAttributeFormat(3, Graphics::OpenGLWrapper::VertexAttributeType::Uint32, 1, false, 0);
		staticParticleVertexArray.SetVertexAttributeBuffer(3, &staticParticleColorBufferGL, sizeof(float), 0);
		staticParticleVertexArray.SetVertexAttributeDivisor(3, 1);
#endif
	}
	void ParticleBufferSetNoInterop::StartRender(cl::CommandQueue& queue)
	{ 
		cl_int ret;

		readFinishedFence.Clear();
		CL_CALL(writeFinishedEvent.wait());
#ifdef VISUALIZE_NEIGHBOURS
		dynamicParticleColorBufferGL.FlushBufferRange(0, sizeof(float) * dynamicParticleCount);
		staticParticleColorBufferGL.FlushBufferRange(0, sizeof(float) * staticParticleCount);
#endif
		dynamicParticleBufferGL.FlushBufferRange(0, sizeof(DynamicParticle) * dynamicParticleCount);				
	}
	void ParticleBufferSetNoInterop::EndRender(cl::CommandQueue& queue)
	{				
		readFinishedFence.SetFence();		
	}
	void ParticleBufferSetNoInterop::StartSimulationRead(cl::CommandQueue& queue)
	{
	}
	void ParticleBufferSetNoInterop::StartSimulationWrite(cl::CommandQueue& queue)
	{
		writeFinishedEvent = cl::Event();
		auto fenceState = readFinishedFence.BlockClient(2);
		switch (fenceState)
		{
		case Blaze::Graphics::OpenGLWrapper::FenceReturnState::AlreadySignaled:
		case Blaze::Graphics::OpenGLWrapper::FenceReturnState::ConditionSatisfied:
			readFinishedFence = Graphics::OpenGLWrapper::Fence();
			break;
		case Blaze::Graphics::OpenGLWrapper::FenceReturnState::TimeoutExpired:
			Debug::Logger::LogWarning("Client", "System simulation fence timeout");
			break;			
		case Blaze::Graphics::OpenGLWrapper::FenceReturnState::Error:
			Debug::Logger::LogWarning("Client", "System simulation fence error");
			break;
		case Blaze::Graphics::OpenGLWrapper::FenceReturnState::FenceNotSet:
			break;
		default:
			Debug::Logger::LogWarning("Client", "Invalid FenceReturnState enum value");
			break;
		}		

		cl_int ret;
		const byte pattern = (byte)0;

#ifdef VISUALIZE_NEIGHBOURS			
		CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleColorBufferCL(), &pattern, 1, 0, sizeof(float) * dynamicParticleCount, 0, nullptr, nullptr));
		CL_CALL(clEnqueueFillBuffer(queue(), staticParticleColorBufferCL(), &pattern, 1, 0, sizeof(float) * staticParticleCount, 0, nullptr, nullptr));
#endif
	}
	void ParticleBufferSetNoInterop::EndSimulationRead(cl::CommandQueue& queue)
	{
	}
	void ParticleBufferSetNoInterop::EndSimulationWrite(cl::CommandQueue& queue)
	{
		cl_int ret;

#ifdef VISUALIZE_NEIGHBOURS
		CL_CALL(clEnqueueReadBuffer(queue(), dynamicParticleColorBufferCL(), CL_FALSE, 0, sizeof(float) * dynamicParticleCount, dynamicParticleColorBufferMap, 0, nullptr, nullptr))
		CL_CALL(clEnqueueReadBuffer(queue(), staticParticleColorBufferCL(), CL_FALSE, 0, sizeof(float) * staticParticleCount, staticParticleColorBufferMap, 0, nullptr, nullptr))
#endif
		CL_CALL(clEnqueueReadBuffer(queue(), dynamicParticleBufferCL(), CL_FALSE, 0, sizeof(DynamicParticle) * dynamicParticleCount, dynamicParticleBufferMap, 0, nullptr, &writeFinishedEvent()))							
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
		clContext(clContext)
	{
		if (Graphics::OpenGL::GraphicsContext_OpenGL::IsExtensionSupported("GL_ARB_cl_event"))
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension supported. The application implementation could be made better to use this extension. This is just informative");
		else
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension not supported. Even if it was supported it isn't implemented in the application. This is just informative");

		cl_int ret;

		queue = cl::CommandQueue(clContext.context, clContext.device, cl::QueueProperties::None, &ret);
		CL_CHECK();		

		
		CL_CALL(clGetDeviceInfo(clContext.device(), CL_DEVICE_PREFERRED_INTEROP_USER_SYNC, sizeof(cl_bool), &userOpenCLOpenGLSync, nullptr))
	}
	SystemGPU::~SystemGPU()
	{
		for (auto& bufferSetPointer : bufferSetsPointers)
			delete[] bufferSetPointer;
	}
	void SystemGPU::Initialize(const SystemInitParameters& initParams)
	{					
		this->initParams = initParams;

		cl_int ret;
		
		Array<DynamicParticle> dynamicParticles;
		Array<StaticParticle> staticParticles;		

		GenerateParticles(dynamicParticles, staticParticles);
		LoadKernels();			
		Array<uint32> staticHashMap = CalculateStaticParticleHashes(staticParticles, staticParticles.Count() * initParams.hashesPerStaticParticle);
		CreateBuffers(dynamicParticles, staticParticles, dynamicHashMapSize, staticHashMap);		

		renderBufferSetIndex = 0;
		simulationReadBufferSetIndex = 0;
		simulationWriteBufferSetIndex = 1;
		renderBufferSet = bufferSetsPointers[renderBufferSetIndex];	
		simulationReadBufferSet = bufferSetsPointers[simulationReadBufferSetIndex];
		simulationWriteBufferSet = bufferSetsPointers[simulationWriteBufferSetIndex];

		CalculateDynamicParticleHashes(dynamicHashMapSize, dynamicParticleCount);		
	}	
	void SystemGPU::Update(float deltaTime)
	{		
		Array<uint32> hashes;
		hashes.Resize(dynamicHashMapSize + 1);

		cl_int ret;												

		simulationReadBufferSet->StartSimulationRead(queue);
		simulationWriteBufferSet->StartSimulationWrite(queue);				

		EnqueueUpdateParticlesPressureKernel(),
		EnqueueUpdateParticlesDynamicsKernel(deltaTime);

		simulationReadBufferSet->EndSimulationRead(queue);
		simulationWriteBufferSet->EndSimulationWrite(queue);				

		std::swap(newHashMapBuffer, hashMapBuffer);		

		byte* pattern = 0;
		clEnqueueFillBuffer(queue(), newHashMapBuffer(), &pattern, sizeof(byte), 0, sizeof(uint32) * dynamicHashMapSize, 0, nullptr, nullptr);
			

		EnqueuePartialSumKernels(hashMapBuffer, dynamicHashMapSize, scanKernelElementCountPerGroup);

		EnqueueComputeParticleMapKernel(simulationWriteBufferSet, hashMapBuffer, particleMapBuffer, nullptr);


		IncrementSimulationBufferSets();
	}	
	void SystemGPU::StartRender(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
	{		
		renderBufferSet->StartRender(queue);		
	}
	Graphics::OpenGLWrapper::VertexArray& SystemGPU::GetDynamicParticlesVertexArray()
	{
		return renderBufferSet->dynamicParticleVertexArray;
	}
	Graphics::OpenGLWrapper::VertexArray& SystemGPU::GetStaticParticlesVertexArray()
	{
		return renderBufferSet->staticParticleVertexArray;
	}
	void SystemGPU::EndRender()
	{
		renderBufferSet->EndRender(queue);		
		IncrementRenderBufferSet();		
	}		
	void SystemGPU::IncrementRenderBufferSet()
	{
		renderBufferSetIndex = simulationReadBufferSetIndex;		
		renderBufferSet = bufferSetsPointers[renderBufferSetIndex];
	}
	void SystemGPU::IncrementSimulationBufferSets()
	{
		simulationReadBufferSetIndex = (simulationReadBufferSetIndex + 1) % bufferSetsPointers.Count();
		simulationWriteBufferSetIndex = (simulationWriteBufferSetIndex + 1) % bufferSetsPointers.Count();
		simulationReadBufferSet = bufferSetsPointers[simulationReadBufferSetIndex];
		simulationWriteBufferSet = bufferSetsPointers[simulationWriteBufferSetIndex];
	}	
	void SystemGPU::LoadKernels()
	{
		cl_int ret;

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

		swapHashMaps = ((uint)std::ceil(std::log2(dynamicParticleCount))) % 2 == 1;
		dynamicHashMapSize = initParams.hashesPerDynamicParticle * dynamicParticleCount;
		uintMem staticHashMapSize = initParams.hashesPerStaticParticle * staticParticleCount;

		//find the best combination to be > than hashMapSize but still valid
		uint layerCount = std::ceil(std::log(dynamicHashMapSize) / std::log(scanKernelElementCountPerGroup));
		scanKernelElementCountPerGroup = 1Ui64 << (uint)std::ceil(std::log2(std::pow<float>(dynamicHashMapSize, 1.0f / layerCount)));
		dynamicHashMapSize = std::pow(scanKernelElementCountPerGroup, layerCount);

		float smoothingKernelConstant = SmoothingKernelConstant(initParams.particleBehaviourParameters.maxInteractionDistance);
		float selfDensity = initParams.particleBehaviourParameters.particleMass * SmoothingKernelD0(0, initParams.particleBehaviourParameters.maxInteractionDistance) * smoothingKernelConstant;
		Vec3f boxPoint1 = initParams.particleBoundParameters.boxOffset;
		Vec3f boxPoint2 = initParams.particleBoundParameters.boxOffset + initParams.particleBoundParameters.boxSize;

		Map<String, String> values = {
			{ "CL_COMPILER" },
			{ "PARTICLE_MASS",            "(float)" + StringParsing::Convert(initParams.particleBehaviourParameters.particleMass) },
			{ "GAS_CONSTANT",             "(float)" + StringParsing::Convert(initParams.particleBehaviourParameters.gasConstant) },
			{ "ELASTICITY",              "(float)" + StringParsing::Convert(initParams.particleBehaviourParameters.elasticity) },
			{ "VISCOSITY",               "(float)" + StringParsing::Convert(initParams.particleBehaviourParameters.viscosity) },

			{ "BOUNDING_BOX_POINT_1",       "(float3)(" + StringParsing::Convert(boxPoint1.x) + "," + StringParsing::Convert(boxPoint1.x) + "," + StringParsing::Convert(boxPoint1.x) + ")" },
			{ "BOUNDING_BOX_POINT_2",       "(float3)(" + StringParsing::Convert(boxPoint2.x) + "," + StringParsing::Convert(boxPoint2.x) + "," + StringParsing::Convert(boxPoint2.x) + ")" },
			{ "MAX_INTERACTION_DISTANCE",  "(float)" + StringParsing::Convert(initParams.particleBehaviourParameters.maxInteractionDistance) },
			{ "REST_DENSITY",             "(float)" + StringParsing::Convert(initParams.particleBehaviourParameters.restDensity) },
			{ "GRAVITY",                 "(float3)(" + StringParsing::Convert(initParams.particleBehaviourParameters.gravity.x) + "," + StringParsing::Convert(initParams.particleBehaviourParameters.gravity.y) + "," + StringParsing::Convert(initParams.particleBehaviourParameters.gravity.z) + ")" },

			{ "SMOOTHING_KERNEL_CONSTANT", "(float)" + StringParsing::Convert(smoothingKernelConstant) },
			{ "SELF_DENSITY",             "(float)" + StringParsing::Convert(selfDensity) },
			{ "HASH_MAP_SIZE",             "(uint)" + StringParsing::Convert(dynamicHashMapSize)},
			{ "PARTICLE_COUNT",           "(uint)" + StringParsing::Convert(dynamicParticleCount) },
			{ "STATIC_HASH_MAP_SIZE",       StringParsing::Convert(staticHashMapSize) },
			{ "STATIC_PARTICLE_COUNT",     StringParsing::Convert(staticParticleCount)  }
		};

		if (initParams.particleBoundParameters.bounded) values.Insert("BOUND_PARTICLES");
		if (initParams.particleBoundParameters.boundedByRoof) values.Insert("BOUND_TOP");
		if (initParams.particleBoundParameters.boundedByWalls) values.Insert("BOUND_WALLS");
#ifdef VISUALIZE_NEIGHBOURS
		values.Insert("VISUALIZE_NEIGHBOURS");
#endif

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
	void SystemGPU::GenerateParticles(Array<DynamicParticle>& dynamicParticles, Array<StaticParticle>& staticParticles)
	{
		if (initParams.staticParticleGenerationParameters.generator)
			initParams.staticParticleGenerationParameters.generator->Generate(staticParticles);
		if (initParams.dynamicParticleGenerationParameters.generator)
			initParams.dynamicParticleGenerationParameters.generator->Generate(dynamicParticles);		

		staticParticleCount = staticParticles.Count();
		dynamicParticleCount = dynamicParticles.Count();		
	}
	void SystemGPU::CreateBuffers(const Array<DynamicParticle>& dynamicParticles, const Array<StaticParticle>& staticParticles, uintMem dynamicHashMapSize, const Array<uint32>& staticHashMap)
	{
		cl_int ret;
		
		uintMem bufferCount;

		if (initParams.bufferCount < 2)
		{
			Debug::Logger::LogWarning("Client", "SystemInitParameters bufferCount member was set to " + StringParsing::Convert(initParams.bufferCount) + ". Only values above 1 are valid. The value was set to 2");
			bufferCount = 2;
		}
		else
			bufferCount = initParams.bufferCount;

		for (auto& bufferSetPointer : bufferSetsPointers)
			delete[] bufferSetPointer;
				
		auto AllocateBufferSet = [&]<typename T>() {						
			bufferSetsPointers.Resize(bufferCount);
			for (auto& bufferSetPointer : bufferSetsPointers)			
				bufferSetPointer = new T(clContext, dynamicParticles, staticParticles);
		};				

		if (clContext.supportedCLGLInterop)
			AllocateBufferSet.template operator()<ParticleBufferSetInterop>();
		else
			AllocateBufferSet.template operator()<ParticleBufferSetNoInterop>();				

		if (!dynamicParticles.Empty())
		{
			hashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * dynamicHashMapSize, nullptr, &ret);
			CL_CHECK();
			newHashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * dynamicHashMapSize, nullptr, &ret);
			CL_CHECK();
			particleMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * dynamicParticleCount, nullptr, &ret);
			CL_CHECK();
		}
		if (!staticParticles.Empty())
		{
			staticHashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(uint32) * staticHashMap.Count(), (void*)staticHashMap.Ptr(), &ret);
			CL_CHECK();
			staticParticleBuffer = cl::Buffer(clContext.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(StaticParticle) * staticParticles.Count(), (void*)staticParticles.Ptr(), &ret);
			CL_CHECK();
		}		
	}
	void SystemGPU::CalculateDynamicParticleHashes(uintMem dynamicHashMapSize, uintMem dynamicParticleCount)
	{
		uint32 pattern0 = 0;
		clEnqueueFillBuffer(queue(), hashMapBuffer(), &pattern0, sizeof(uint32), 0, dynamicHashMapSize * sizeof(uint32), 0, nullptr, nullptr);

		uint32 patternCount = dynamicParticleCount;
		clEnqueueFillBuffer(queue(), hashMapBuffer(), &patternCount, sizeof(uint32), dynamicHashMapSize * sizeof(uint32), sizeof(uint32), 0, nullptr, nullptr);

		bufferSetsPointers[0]->StartSimulationWrite(queue);
		EnqueueComputeParticleHashesKernel(bufferSetsPointers[0], hashMapBuffer, nullptr);
		EnqueuePartialSumKernels(hashMapBuffer, dynamicHashMapSize, scanKernelElementCountPerGroup);
		EnqueueComputeParticleMapKernel(bufferSetsPointers[0], hashMapBuffer, particleMapBuffer, nullptr);
		bufferSetsPointers[0]->EndSimulationWrite(queue);
	}
	Array<uint32> SystemGPU::CalculateStaticParticleHashes(Array<StaticParticle>& staticParticles, uintMem staticHashMapSize)
	{		
		Array<uint32> staticHashMap;
		
		auto GetStaticParticleHash = [gridSize = initParams.particleBehaviourParameters.maxInteractionDistance, mod = staticHashMapSize](const StaticParticle& particle) {
			return GetHash(Vec3i(particle.position / gridSize)) % mod;
			};				
		
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

		return staticHashMap;		
	}
	void SystemGPU::EnqueueComputeParticleHashesKernel(ParticleBufferSet* bufferSet, cl::Buffer& hashMapBuffer, cl_event* finishedEvent)
	{
		cl_int ret;

		CL_CALL(computeParticleHashesKernel.setArg(0, bufferSet->GetDynamicParticleBufferCL()));
		CL_CALL(computeParticleHashesKernel.setArg(1, hashMapBuffer));		
		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = 1;
		CL_CALL(clEnqueueNDRangeKernel(queue(), computeParticleHashesKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, finishedEvent));
	}
	void SystemGPU::EnqueueComputeParticleMapKernel(ParticleBufferSet* bufferSet, cl::Buffer& hashMapBuffer, cl::Buffer& particleMapBuffer, cl_event* finishedEvent)
	{
		cl_int ret;

		CL_CALL(computeParticleMapKernel.setArg(0, bufferSet->GetDynamicParticleBufferCL()));		
		CL_CALL(computeParticleMapKernel.setArg(1, hashMapBuffer));		
		CL_CALL(computeParticleMapKernel.setArg(2, particleMapBuffer));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = 1;
		CL_CALL(clEnqueueNDRangeKernel(queue(), computeParticleMapKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, finishedEvent));	
	}
	void SystemGPU::EnqueueUpdateParticlesPressureKernel() {
		cl_int ret;

		CL_CALL(updateParticlesPressureKernel.setArg(0, simulationReadBufferSet->GetDynamicParticleBufferCL()));
		CL_CALL(updateParticlesPressureKernel.setArg(1, simulationWriteBufferSet->GetDynamicParticleBufferCL()));
		CL_CALL(updateParticlesPressureKernel.setArg(2, hashMapBuffer));		
		CL_CALL(updateParticlesPressureKernel.setArg(3, particleMapBuffer));						
		CL_CALL(updateParticlesPressureKernel.setArg(4, staticParticleBuffer));		
		CL_CALL(updateParticlesPressureKernel.setArg(5, staticHashMapBuffer));
#ifdef VISUALIZE_NEIGHBOURS
		CL_CALL(updateParticlesPressureKernel.setArg(6, simulationWriteBufferSet->GetDynamicParticleColorBufferCL()));
		CL_CALL(updateParticlesPressureKernel.setArg(7, simulationWriteBufferSet->GetStaticParticleColorBufferCL()));
#endif


		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = 1;
		CL_CALL(clEnqueueNDRangeKernel(queue(), updateParticlesPressureKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));		
	}
	void SystemGPU::EnqueueUpdateParticlesDynamicsKernel(float deltaTime)
	{
		bool moveParticles = false;		

		cl_int ret;

		CL_CALL(updateParticlesDynamicsKernel.setArg(0, simulationReadBufferSet->GetDynamicParticleBufferCL()));
		CL_CALL(updateParticlesDynamicsKernel.setArg(1, simulationWriteBufferSet->GetDynamicParticleBufferCL()));
		CL_CALL(updateParticlesDynamicsKernel.setArg(2, hashMapBuffer));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(3, newHashMapBuffer));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(4, particleMapBuffer));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(5, staticParticleBuffer));
		CL_CALL(updateParticlesDynamicsKernel.setArg(6, staticHashMapBuffer));
		CL_CALL(updateParticlesDynamicsKernel.setArg(7, deltaTime));		

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = 1;
		CL_CALL(clEnqueueNDRangeKernel(queue(), updateParticlesDynamicsKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));		
	}
	void SystemGPU::EnqueuePartialSumKernels(cl::Buffer& buffer, uintMem elementCount, uintMem groupSize)
	{		
		cl_int ret;					

		for (uintMem size = elementCount; size != 1; size /= groupSize)
		{									
			CL_CALL(scanOnComputeGroupsKernel.setArg(0, (groupSize + 1) * sizeof(uint32), nullptr));
			CL_CALL(scanOnComputeGroupsKernel.setArg(1, buffer));
			CL_CALL(scanOnComputeGroupsKernel.setArg(2, (uint32)(elementCount / size)));

			size_t globalWorkOffset = 0;
			size_t globalWorkSize = size / 2;
			size_t localWorkSize = groupSize / 2;
			CL_CALL(clEnqueueNDRangeKernel(queue(), scanOnComputeGroupsKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));			
		}

		for (uintMem i = 1; i != elementCount / groupSize; i *= groupSize)
		{			
			CL_CALL(addToComputeGroupArraysKernel.setArg(0, buffer));
			CL_CALL(addToComputeGroupArraysKernel.setArg(1, (uint32)i));			

			size_t globalWorkOffset = 0;
			size_t globalWorkSize = elementCount / i - groupSize - groupSize + 1;
			size_t localWorkSize = groupSize - 1;
			CL_CALL(clEnqueueNDRangeKernel(queue(), addToComputeGroupArraysKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));
		}
			
	}		
}