#include "pch.h"
#include "SystemGPU.h"
#include "SPH/Graphics/SystemGPURenderer.h"
#include "gl/glew.h"
#include "GL/glext.h"

namespace SPH
{	
	ParticleBufferSet::ParticleBufferSet(const Array<StaticParticle>& staticParticles)
	{		
	}	

	ParticleBufferSetInterop::ParticleBufferSetInterop(SystemGPU& system, const Array<DynamicParticle>& dynamicParticles, const Array<StaticParticle>& staticParticles) :
		ParticleBufferSet(staticParticles), system(system)
	{
		cl_int ret;

		dynamicParticleBufferGL.Allocate(dynamicParticles.Ptr(), sizeof(DynamicParticle) * dynamicParticles.Count());		
		dynamicParticleBufferCL = cl::BufferGL(system.clContext.context, CL_MEM_READ_WRITE, dynamicParticleBufferGL.GetHandle(), &ret);
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
	 	
	ParticleBufferSetNoInterop::ParticleBufferSetNoInterop(SystemGPU& system, const Array<DynamicParticle>& dynamicParticles, const Array<StaticParticle>& staticParticles) :
		ParticleBufferSet(staticParticles), system(system), dynamicParticleCount(dynamicParticles.Count()), staticParticleCount(staticParticles.Count())
	{
		cl_int ret;

		dynamicParticleBufferGL.Allocate(dynamicParticles.Ptr(), sizeof(DynamicParticle) * dynamicParticleCount, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentUncoherent);
		dynamicParticleBufferMap = dynamicParticleBufferGL.MapBufferRange(0, sizeof(DynamicParticle) * dynamicParticleCount, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush);

		dynamicParticleBufferCL = cl::Buffer(system.clContext.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(DynamicParticle) * dynamicParticleCount, (void*)dynamicParticles.Ptr(), &ret);
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
		
		if (writeFinishedEvent() != nullptr)
			CL_CALL(writeFinishedEvent.wait());
		writeFinishedEvent = cl::Event();

#ifdef VISUALIZE_NEIGHBOURS
		dynamicParticleColorBufferGL.FlushBufferRange(0, sizeof(float) * dynamicParticleCount);
		staticParticleColorBufferGL.FlushBufferRange(0, sizeof(float) * staticParticleCount);
#endif
		dynamicParticleBufferGL.FlushBufferRange(0, sizeof(DynamicParticle) * dynamicParticleCount);
		readFinishedFence.SetFence();
	}
	void ParticleBufferSetNoInterop::EndRender(cl::CommandQueue& queue)
	{				
	}
	void ParticleBufferSetNoInterop::StartSimulationRead(cl::CommandQueue& queue)
	{
		cl_int ret;

		if (writeFinishedEvent())
			CL_CALL(writeFinishedEvent.wait())
		writeFinishedEvent = cl::Event();
	}
	void ParticleBufferSetNoInterop::StartSimulationWrite(cl::CommandQueue& queue)
	{
		cl_int ret;		
		
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
		readFinishedFence.Clear();

#ifdef VISUALIZE_NEIGHBOURS			
		const byte pattern = (byte)0;
		CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleColorBufferCL(), &pattern, 1, 0, sizeof(float) * dynamicParticleCount, 0, nullptr, nullptr));
		CL_CALL(clEnqueueFillBuffer(queue(), staticParticleColorBufferCL(), &pattern, 1, 0, sizeof(float) * staticParticleCount, 0, nullptr, nullptr));
#endif
	}
	void ParticleBufferSetNoInterop::EndSimulationRead(cl::CommandQueue& queue)
	{
		cl_int ret;
		CL_CALL(queue.enqueueBarrierWithWaitList(nullptr, &readFinishedEvent));
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

	SystemGPU::SystemGPU(OpenCLContext& clContext, Graphics::OpenGL::GraphicsContext_OpenGL& glContext) :
		clContext(clContext), glContext(glContext),
		userOpenCLOpenGLSync(false),
		renderBufferSetIndex(0), renderBufferSet(nullptr),
		simulationReadBufferSetIndex(0), simulationReadBufferSet(nullptr),
		simulationWriteBufferSetIndex(0), simulationWriteBufferSet(nullptr),
		dynamicParticleCount(0), staticParticleCount(0), dynamicParticleHashMapSize(0), staticParticleHashMapSize(0),
		scanKernelElementCountPerGroup(0),		
		particleMoveElapsedTime(0)
	{
		if (Graphics::OpenGL::GraphicsContext_OpenGL::IsExtensionSupported("GL_ARB_cl_event"))
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension supported. The application implementation could be made better to use this extension. This is just informative");
		else
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension not supported. Even if it was supported it isn't implemented in the application. This is just informative");

		cl_int ret;

		queue = cl::CommandQueue(clContext.context, clContext.device, cl::QueueProperties::Profiling, &ret);
		CL_CHECK();				
		
		CL_CALL(clGetDeviceInfo(clContext.device(), CL_DEVICE_PREFERRED_INTEROP_USER_SYNC, sizeof(cl_bool), &userOpenCLOpenGLSync, nullptr))
	}
	SystemGPU::~SystemGPU()
	{
		Clear();		
	}
	void SystemGPU::Clear()
	{
		initParams = { };
		bufferSetsPointers.Clear();
		for (auto& bufferSetPointer : bufferSetsPointers)
			delete bufferSetPointer;

		partialSumProgram = cl::Program();
		SPHProgram = cl::Program();

		computeParticleHashesKernel = cl::Kernel();
		scanOnComputeGroupsKernel = cl::Kernel();
		addToComputeGroupArraysKernel = cl::Kernel();
		computeParticleMapKernel = cl::Kernel();
		updateParticlesPressureKernel = cl::Kernel();
		updateParticlesDynamicsKernel = cl::Kernel();

		dynamicParticleWriteHashMapBuffer = cl::Buffer();
		dynamicParticleReadHashMapBuffer = cl::Buffer();
		particleMapBuffer = cl::Buffer();
		staticParticleBuffer = cl::Buffer();
		staticHashMapBuffer = cl::Buffer();

		renderBufferSetIndex = 0;
		simulationReadBufferSetIndex = 0;
		simulationWriteBufferSetIndex = 0;
		renderBufferSet = nullptr;
		simulationReadBufferSet = nullptr;
		simulationWriteBufferSet = nullptr;

		dynamicParticleCount = 0;
		staticParticleCount = 0;
		dynamicParticleHashMapSize = 0;

		scanKernelElementCountPerGroup = 0;		

		particleMoveElapsedTime = 0;
	}
	void SystemGPU::Initialize(const SystemInitParameters& initParams)
	{					
		Clear();

		this->initParams = initParams;

		LoadKernels();			
				
		Array<DynamicParticle> dynamicParticles;
		Array<StaticParticle> staticParticles;		

		GenerateParticles(dynamicParticles, staticParticles);

		staticParticleHashMapSize = initParams.hashesPerStaticParticle * staticParticleCount;
		Array<uint32> staticParticleHashMap = CalculateStaticParticleHashes(staticParticles, staticParticleHashMapSize, initParams.particleBehaviourParameters.maxInteractionDistance);				

		//find the best combination to be > than the target dynamicParticleHashMapSize but still be in the form of dynamicParticleHashMapSize = pow(scanKernelElementCountPerGroup, n) where n is a natural number
		{
			scanKernelElementCountPerGroup = std::min(
				scanOnComputeGroupsKernel.getWorkGroupInfo< CL_KERNEL_WORK_GROUP_SIZE>(clContext.device),
				addToComputeGroupArraysKernel.getWorkGroupInfo< CL_KERNEL_WORK_GROUP_SIZE>(clContext.device)
			) * 2;
			uintMem dynamicParticleHashMapSizeTarget = initParams.hashesPerDynamicParticle * dynamicParticleCount;
			uint layerCount = std::ceil(std::log(dynamicParticleHashMapSizeTarget) / std::log(scanKernelElementCountPerGroup));
			scanKernelElementCountPerGroup = 1Ui64 << (uint)std::ceil(std::log2(std::pow<float>(dynamicParticleHashMapSizeTarget, 1.0f / layerCount)));
			dynamicParticleHashMapSize = std::pow(scanKernelElementCountPerGroup, layerCount);
		}



#ifdef DEBUG_ARRAYS
		debugParticlesArray.Resize(dynamicParticleCount);		
		debugHashMapArray.Resize(dynamicHashMapSize + 1);
		debugParticleMapArray.Resize(dynamicParticleCount);
#endif		

		simulationParameters.behaviour = initParams.particleBehaviourParameters;
		simulationParameters.dynamicParticleCount = dynamicParticleCount;
		simulationParameters.dynamicParticleHashMapSize = dynamicParticleHashMapSize;
		simulationParameters.staticParticleCount = staticParticleCount;
		simulationParameters.staticParticleHashMapSize = staticParticleHashMapSize;
		simulationParameters.smoothingKernelConstant = SmoothingKernelConstant(initParams.particleBehaviourParameters.maxInteractionDistance);
		simulationParameters.selfDensity = initParams.particleBehaviourParameters.particleMass * SmoothingKernelD0(0, initParams.particleBehaviourParameters.maxInteractionDistance) * simulationParameters.smoothingKernelConstant;

		CreateBuffers(dynamicParticles, staticParticles, dynamicParticleHashMapSize, staticParticleHashMap);

		renderBufferSetIndex = 0;
		simulationReadBufferSetIndex = 0;
		simulationWriteBufferSetIndex = 1;
		renderBufferSet = bufferSetsPointers[renderBufferSetIndex];	
		simulationReadBufferSet = bufferSetsPointers[simulationReadBufferSetIndex];
		simulationWriteBufferSet = bufferSetsPointers[simulationWriteBufferSetIndex];

		CalculateDynamicParticleHashes(bufferSetsPointers[0], dynamicParticleCount, dynamicParticleHashMapSize, initParams.particleBehaviourParameters.maxInteractionDistance);
	}
	void SystemGPU::Update(float deltaTime)
	{
		cl_int ret;								
		
		if (simulationWriteBufferSet->readFinishedEvent() != nullptr)			
		{
			auto status = simulationWriteBufferSet->readFinishedEvent.getInfo<CL_EVENT_COMMAND_EXECUTION_STATUS>();
			if (status != CL_COMPLETE)
				return;
		}

		simulationReadBufferSet->StartSimulationRead(queue);
		simulationWriteBufferSet->StartSimulationWrite(queue);				

		EnqueueUpdateParticlesPressureKernel();
		EnqueueUpdateParticlesDynamicsKernel(deltaTime);

#ifdef DEBUG_ARRAYS
		DebugParticles(*simulationReadBufferSet);
#endif

		simulationReadBufferSet->EndSimulationRead(queue);
		simulationWriteBufferSet->EndSimulationWrite(queue);				

		std::swap(dynamicParticleReadHashMapBuffer, dynamicParticleWriteHashMapBuffer);		

		byte* pattern = 0;
		CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleReadHashMapBuffer(), &pattern, sizeof(byte), 0, sizeof(uint32) * dynamicParticleHashMapSize, 0, nullptr, nullptr));
					
		EnqueuePartialSumKernels(dynamicParticleWriteHashMapBuffer, dynamicParticleHashMapSize, scanKernelElementCountPerGroup);
		EnqueueComputeParticleMapKernel(simulationWriteBufferSet, dynamicParticleCount, dynamicParticleWriteHashMapBuffer, particleMapBuffer, nullptr);

#ifdef DEBUG_ARRAYS
		DebugHashes(*simulationWriteBufferSet);
#endif

		IncrementSimulationBufferSets();
	}	
	void SystemGPU::StartRender(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
	{		
		renderBufferSetIndex = simulationReadBufferSetIndex;
		renderBufferSet = bufferSetsPointers[renderBufferSetIndex];
		renderBufferSet->StartRender(queue);		
	}
	Graphics::OpenGLWrapper::VertexArray& SystemGPU::GetDynamicParticlesVertexArray()
	{
		return renderBufferSet->dynamicParticleVertexArray;
	}
	Graphics::OpenGLWrapper::VertexArray& SystemGPU::GetStaticParticlesVertexArray()
	{
		return staticParticleVertexArray;
	}
	void SystemGPU::EndRender()
	{
		renderBufferSet->EndRender(queue);		
		IncrementRenderBufferSet();		
	}		
	void SystemGPU::IncrementRenderBufferSet()
	{		
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

		SPHProgram = BuildOpenCLProgram(clContext, Array<Path>{ "kernels/CL_CPP_SPHDeclarations.h", "kernels/CL_CPP_SPHFunctions.h", "kernels/SPH.cl" }, { {"CL_COMPILER"}});

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
	void SystemGPU::CreateBuffers(const Array<DynamicParticle>& dynamicParticles, const Array<StaticParticle>& staticParticles, uintMem dynamicParticleHashMapSize, const Array<uint32>& staticParticleHashMap)
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
			delete bufferSetPointer;
				
		auto AllocateBufferSet = [&]<typename T>() {						
			bufferSetsPointers.Resize(bufferCount);
			for (auto& bufferSetPointer : bufferSetsPointers)			
				bufferSetPointer = new T(*this, dynamicParticles, staticParticles);
		};				

		if (clContext.supportedCLGLInterop)
			AllocateBufferSet.template operator()<ParticleBufferSetInterop>();
		else
			AllocateBufferSet.template operator()<ParticleBufferSetNoInterop>();				

		staticParticleBufferGL.Allocate(staticParticles.Ptr(), sizeof(StaticParticle) * staticParticles.Count());

		staticParticleVertexArray.EnableVertexAttribute(0);
		staticParticleVertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(StaticParticle, position));
		staticParticleVertexArray.SetVertexAttributeBuffer(0, &staticParticleBufferGL, sizeof(StaticParticle), 0);
		staticParticleVertexArray.SetVertexAttributeDivisor(0, 1);

		if (!dynamicParticles.Empty())
		{
			dynamicParticleWriteHashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * (dynamicParticleHashMapSize + 1), nullptr, &ret);
			CL_CHECK();
			dynamicParticleReadHashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * (dynamicParticleHashMapSize + 1), nullptr, &ret);
			CL_CHECK();
			particleMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * dynamicParticleCount, nullptr, &ret);
			CL_CHECK();
		}
		if (!staticParticles.Empty())
		{
			staticHashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(uint32) * staticParticleHashMap.Count(), (void*)staticParticleHashMap.Ptr(), &ret);
			CL_CHECK();
			staticParticleBuffer = cl::Buffer(clContext.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(StaticParticle) * staticParticles.Count(), (void*)staticParticles.Ptr(), &ret);
			CL_CHECK();
		}		

		simulationParametersBuffer = cl::Buffer(clContext.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(ParticleSimulationParameters), &simulationParameters, &ret);
	}
	void SystemGPU::CalculateDynamicParticleHashes(ParticleBufferSet* bufferSet, uintMem dynamicParticleCount, uintMem dynamicParticleHashMapSize, float maxInteractionDistance)
	{
		uint32 pattern0 = 0;
		clEnqueueFillBuffer(queue(), dynamicParticleWriteHashMapBuffer(), &pattern0, sizeof(uint32), 0, dynamicParticleHashMapSize * sizeof(uint32), 0, nullptr, nullptr);
		clEnqueueFillBuffer(queue(), dynamicParticleReadHashMapBuffer(), &pattern0, sizeof(uint32), 0, dynamicParticleHashMapSize * sizeof(uint32), 0, nullptr, nullptr);

		uint32 patternCount = dynamicParticleCount;
		clEnqueueFillBuffer(queue(), dynamicParticleWriteHashMapBuffer(), &patternCount, sizeof(uint32), dynamicParticleHashMapSize * sizeof(uint32), sizeof(uint32), 0, nullptr, nullptr);
		clEnqueueFillBuffer(queue(), dynamicParticleReadHashMapBuffer(), &patternCount, sizeof(uint32), dynamicParticleHashMapSize * sizeof(uint32), sizeof(uint32), 0, nullptr, nullptr);

		bufferSet->StartSimulationWrite(queue);
		EnqueueComputeParticleHashesKernel(bufferSet, dynamicParticleCount, dynamicParticleWriteHashMapBuffer, dynamicParticleHashMapSize, maxInteractionDistance, nullptr);

#ifdef DEBUG_ARRAYS
		DebugPrePrefixSumHashes(*bufferSet);
#endif

		EnqueuePartialSumKernels(dynamicParticleWriteHashMapBuffer, dynamicParticleHashMapSize, scanKernelElementCountPerGroup);
		EnqueueComputeParticleMapKernel(bufferSet, dynamicParticleCount, dynamicParticleWriteHashMapBuffer, particleMapBuffer, nullptr);
		bufferSet->EndSimulationWrite(queue);

#ifdef DEBUG_ARRAYS
		DebugParticles(*bufferSet);
		DebugHashes(*bufferSet);
#endif
	}
	Array<uint32> SystemGPU::CalculateStaticParticleHashes(Array<StaticParticle>& staticParticles, uintMem staticHashMapSize, float maxInteractionDistance)
	{		
		if (staticHashMapSize == 0)
			return { };

		auto GetStaticParticleHash = [gridSize = maxInteractionDistance, mod = staticHashMapSize](const StaticParticle& particle) {
			return GetHash(Vec3i(particle.position / gridSize)) % mod;
			};				
		
		Array<uint32> staticParticleHashMap;		
		staticParticleHashMap.Resize(staticHashMapSize + 1);		

		staticParticles = GenerateHashMapAndReorderParticles(staticParticles, staticParticleHashMap, GetStaticParticleHash);		

		return staticParticleHashMap;		
	}
	void SystemGPU::EnqueueComputeParticleHashesKernel(ParticleBufferSet* bufferSet, uintMem dynamicParticleCount, cl::Buffer& dynamicParticleWriteHashMapBuffer, uintMem dynamicParticleHashMapSize, float maxInteractionDistance, cl_event* finishedEvent)
	{
		cl_int ret;

		CL_CALL(computeParticleHashesKernel.setArg(0, bufferSet->GetDynamicParticleBufferCL()));
		CL_CALL(computeParticleHashesKernel.setArg(1, dynamicParticleWriteHashMapBuffer));		
		CL_CALL(computeParticleHashesKernel.setArg(2, maxInteractionDistance));
		CL_CALL(computeParticleHashesKernel.setArg(3, (uint32)dynamicParticleHashMapSize));
		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = 1;
		CL_CALL(clEnqueueNDRangeKernel(queue(), computeParticleHashesKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, finishedEvent));
	}
	void SystemGPU::EnqueueComputeParticleMapKernel(ParticleBufferSet* bufferSet, uintMem dynamicParticleCount, cl::Buffer& dynamicParticleWriteHashMapBuffer, cl::Buffer& particleMapBuffer, cl_event* finishedEvent)
	{
		cl_int ret;

		CL_CALL(computeParticleMapKernel.setArg(0, bufferSet->GetDynamicParticleBufferCL()));		
		CL_CALL(computeParticleMapKernel.setArg(1, dynamicParticleWriteHashMapBuffer));		
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
		CL_CALL(updateParticlesPressureKernel.setArg(2, dynamicParticleWriteHashMapBuffer));		
		CL_CALL(updateParticlesPressureKernel.setArg(3, particleMapBuffer));						
		CL_CALL(updateParticlesPressureKernel.setArg(4, staticParticleBuffer));		
		CL_CALL(updateParticlesPressureKernel.setArg(5, staticHashMapBuffer));
		CL_CALL(updateParticlesPressureKernel.setArg(6, simulationParametersBuffer));
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
		CL_CALL(updateParticlesDynamicsKernel.setArg(2, dynamicParticleWriteHashMapBuffer));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(3, dynamicParticleReadHashMapBuffer));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(4, particleMapBuffer));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(5, staticParticleBuffer));
		CL_CALL(updateParticlesDynamicsKernel.setArg(6, staticHashMapBuffer));
		CL_CALL(updateParticlesDynamicsKernel.setArg(7, deltaTime));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(8, simulationParametersBuffer));

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
#ifdef DEBUG_ARRAYS
	void SystemGPU::DebugParticles(ParticleBufferSet& bufferSet)
	{		
		queue.finish();
		queue.enqueueReadBuffer(bufferSet.GetDynamicParticleBufferCL(), CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), nullptr, nullptr);
		
		System::DebugParticles<DynamicParticle>(debugParticlesArray, initParams.particleBehaviourParameters.maxInteractionDistance, dynamicHashMapSize);
	}
	void SystemGPU::DebugPrePrefixSumHashes(ParticleBufferSet& bufferSet)
	{						
		queue.finish();
		queue.enqueueReadBuffer(hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicHashMapSize + 1), debugHashMapArray.Ptr(), nullptr, nullptr);
		queue.enqueueReadBuffer(bufferSet.GetDynamicParticleBufferCL(), CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), nullptr, nullptr);
		
		System::DebugPrePrefixSumHashes<DynamicParticle>(debugParticlesArray, debugHashMapArray);
	}
	void SystemGPU::DebugHashes(ParticleBufferSet& bufferSet)
	{		
		queue.finish();
		queue.enqueueReadBuffer(hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicHashMapSize + 1), debugHashMapArray.Ptr(), nullptr, nullptr);
		queue.enqueueReadBuffer(particleMapBuffer, CL_TRUE, 0, sizeof(uint32) * dynamicParticleCount, debugParticleMapArray.Ptr(), nullptr, nullptr);
		queue.enqueueReadBuffer(bufferSet.GetDynamicParticleBufferCL(), CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), nullptr, nullptr);

		System::DebugHashAndParticleMap<DynamicParticle>(debugParticlesArray, debugHashMapArray, debugParticleMapArray);
	}
#endif
}