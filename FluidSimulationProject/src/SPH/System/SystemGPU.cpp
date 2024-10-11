#include "pch.h"
#include "SystemGPU.h"
#include "gl/glew.h"
#include "GL/glext.h"

namespace SPH
{	
	ParticleBufferSet::ParticleBufferSet()
	{		
	}	

	ParticleBufferSetInterop::ParticleBufferSetInterop(OpenCLContext& CLContext, const Array<DynamicParticle>& dynamicParticles)
	{
		cl_int ret;

		dynamicParticleBufferGL.Allocate(dynamicParticles.Ptr(), sizeof(DynamicParticle) * dynamicParticles.Count());		
		dynamicParticleBufferCL = cl::BufferGL(CLContext.context, CL_MEM_READ_WRITE, dynamicParticleBufferGL.GetHandle(), &ret);
		CL_CHECK();

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
		};

		clEnqueueReleaseGLObjects(queue(), _countof(acquireObjects), acquireObjects, 0, nullptr, &writeFinishedEvent());
	}
	 	
	ParticleBufferSetNoInterop::ParticleBufferSetNoInterop(OpenCLContext& CLContext, const Array<DynamicParticle>& dynamicParticles) :
		dynamicParticleCount(dynamicParticles.Count())
	{
		cl_int ret;

		dynamicParticleBufferGL.Allocate(dynamicParticles.Ptr(), sizeof(DynamicParticle) * dynamicParticleCount, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapAccess::Write, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapType::PersistentUncoherent);
		dynamicParticleBufferMap = dynamicParticleBufferGL.MapBufferRange(0, sizeof(DynamicParticle) * dynamicParticleCount, Graphics::OpenGLWrapper::ImmutableGraphicsBufferMapOptions::ExplicitFlush);

		dynamicParticleBufferCL = cl::Buffer(CLContext.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(DynamicParticle) * dynamicParticleCount, (void*)dynamicParticles.Ptr(), &ret);
		CL_CHECK();

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
	}
	void ParticleBufferSetNoInterop::StartRender(cl::CommandQueue& queue)
	{ 		
		cl_int ret;
		
		if (writeFinishedEvent() != nullptr)
			CL_CALL(writeFinishedEvent.wait());
		writeFinishedEvent = cl::Event();

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
			break;
		case Blaze::Graphics::OpenGLWrapper::FenceReturnState::ConditionSatisfied:
			Console::Write("waited on fence");
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
	}
	void ParticleBufferSetNoInterop::EndSimulationRead(cl::CommandQueue& queue)
	{
		cl_int ret;
		CL_CALL(queue.enqueueBarrierWithWaitList(nullptr, &readFinishedEvent));
	}
	void ParticleBufferSetNoInterop::EndSimulationWrite(cl::CommandQueue& queue)
	{
		cl_int ret;

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
		scanKernelElementCountPerGroup(0), hashMapBufferGroupSize(0), simulationTime(0), lastTimePerStep_ns(0), profiling(false)
	{
		if (Graphics::OpenGL::GraphicsContext_OpenGL::IsExtensionSupported("GL_ARB_cl_event"))
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension supported. The application implementation could be made better to use this extension. This is just informative");
		else
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension not supported. Even if it was supported it isn't implemented in the application. This is just informative");

		cl_int ret;

		queue = cl::CommandQueue(clContext.context, clContext.device, cl::QueueProperties::Profiling, &ret);
		CL_CHECK();				
		
		CL_CALL(clGetDeviceInfo(clContext.device(), CL_DEVICE_PREFERRED_INTEROP_USER_SYNC, sizeof(cl_bool), &userOpenCLOpenGLSync, nullptr))

		LoadKernels();
	}
	SystemGPU::~SystemGPU()
	{
		Clear();		
	}
	void SystemGPU::Clear()
	{
		queue.finish();				

		staticParticleVertexArray = decltype(staticParticleVertexArray)();
		staticParticleBufferGL = decltype(staticParticleBufferGL)();

		renderBufferSetIndex = 0;
		simulationReadBufferSetIndex = 0;
		simulationWriteBufferSetIndex = 0;
		renderBufferSet = nullptr;
		simulationReadBufferSet = nullptr;
		simulationWriteBufferSet = nullptr;

		for (auto& bufferSetPointer : bufferSetsPointers)
			delete bufferSetPointer;
		bufferSetsPointers.Clear();

		dynamicParticleWriteHashMapBuffer = cl::Buffer();
		dynamicParticleReadHashMapBuffer = cl::Buffer();
		particleMapBuffer = cl::Buffer();
		staticParticleBuffer = cl::Buffer();
		staticHashMapBuffer = cl::Buffer();		
		simulationParametersBuffer = cl::Buffer();		

		dynamicParticleCount = 0;
		staticParticleCount = 0;
		dynamicParticleHashMapSize = 0;
		staticParticleHashMapSize = 0;	

		hashMapBufferGroupSize = 0;

		stepCount = 0;
		
		lastTimePerStep_ns = 0;
		simulationTime = 0;					
	}	
	void SystemGPU::Update(float deltaTime, uint simulationSteps)
	{		
		cl_int ret;							

		auto Measure = [](cl::Event& start, cl::Event& end) -> uint64 {

			start.wait();
			end.wait();
			cl_int ret;

			uint64 startTime = start.getProfilingInfo<CL_PROFILING_COMMAND_END>(&ret);
			CL_CHECK(0);
			uint64 endTime = end.getProfilingInfo<CL_PROFILING_COMMAND_END>(&ret);
			CL_CHECK(0);

			return endTime - startTime;

			};

		cl::Event updateStartEvent;
		cl::Event updateEndEvent;
			
		if (profiling)
		{
			updateStartEvent = cl::Event();
			updateEndEvent = cl::Event();

			//To fix OpenCL bug
			byte* pattern = 0;
			CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleWriteHashMapBuffer(), &pattern, sizeof(byte), 0, sizeof(uint32) * dynamicParticleHashMapSize, 0, nullptr, nullptr));

			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &updateStartEvent));
		}

		for (uint i = 0; i < simulationSteps; ++i)
		{
			if (simulationWriteBufferSet->readFinishedEvent() != nullptr)			
				simulationWriteBufferSet->readFinishedEvent.wait();							


#ifdef PROFILE_GPU
			cl::Event prepareAndSyncStart;
			cl::Event prepareAndSyncEnd;
			cl::Event updateParticlePressureStart;
			cl::Event updateParticlePressureEnd;
			cl::Event updateParticleDynamicStart;
			cl::Event updateParticleDynamicEnd;
			cl::Event postSyncSumStart;
			cl::Event postSyncSumEnd;
			cl::Event partialSumStart;
			cl::Event partialSumEnd;
			cl::Event computeParticleMapStart;
			cl::Event computeParticleMapEnd;

			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &prepareAndSyncStart));
#endif

			byte* pattern = 0;
			CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleWriteHashMapBuffer(), &pattern, sizeof(byte), 0, sizeof(uint32) * dynamicParticleHashMapSize, 0, nullptr, nullptr));

			simulationReadBufferSet->StartSimulationRead(queue);
			simulationWriteBufferSet->StartSimulationWrite(queue);			

#ifdef PROFILE_GPU
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &prepareAndSyncEnd));
			float prepareAndSyncTime = 0.000000001f * Measure(prepareAndSyncStart, prepareAndSyncEnd);

			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &updateParticlePressureStart));
#endif
			EnqueueUpdateParticlesPressureKernel();
#ifdef PROFILE_GPU
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &updateParticlePressureEnd));
			float updateParticlePressureTime = 0.000000001f * Measure(updateParticlePressureStart, updateParticlePressureEnd);

			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &updateParticleDynamicStart));
#endif
			EnqueueUpdateParticlesDynamicsKernel(deltaTime);
#ifdef PROFILE_GPU
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &updateParticleDynamicEnd));			
			float updateParticleDynamicTime = 0.000000001f * Measure(updateParticleDynamicStart, updateParticleDynamicEnd);
#endif

#ifdef DEBUG_BUFFERS_GPU
			DebugParticles(*simulationWriteBufferSet);
			DebugPrePrefixSumHashes(*simulationWriteBufferSet, dynamicParticleWriteHashMapBuffer);
#endif

			//bool reorderParticles = stepCount % 20 == 19;
			bool reorderParticles = false;

#ifdef PROFILE_GPU
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &partialSumStart));
#endif
			EnqueuePartialSumKernels(dynamicParticleWriteHashMapBuffer, dynamicParticleHashMapSize, hashMapBufferGroupSize, reorderParticles ? 1 : 0);
#ifdef PROFILE_GPU
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &partialSumEnd));
			float partialSumTime = 0.000000001f * Measure(partialSumStart, partialSumEnd);

			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &computeParticleMapStart));
#endif
			if (!reorderParticles)			
				EnqueueComputeParticleMapKernel(simulationWriteBufferSet, dynamicParticleCount, dynamicParticleWriteHashMapBuffer, particleMapBuffer, (profiling ? &updateEndEvent() : nullptr));			
			else
			{
				CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleWriteHashMapBuffer(), &pattern, sizeof(byte), 0, sizeof(uint32), 0, nullptr, nullptr));
				EnqueueReorderParticles(simulationWriteBufferSet->GetDynamicParticleBufferCL(), dynamicParticleIntermediateBuffer, particleMapBuffer, (profiling ? &updateEndEvent() : nullptr));
				std::swap(simulationWriteBufferSet->GetDynamicParticleBufferCL(), dynamicParticleIntermediateBuffer);
			}
#ifdef PROFILE_GPU
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &computeParticleMapEnd));
			float computeParticleMapTime = 0.000000001f * Measure(computeParticleMapStart, computeParticleMapEnd);

			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &postSyncSumStart));
#endif
			simulationReadBufferSet->EndSimulationRead(queue);
			simulationWriteBufferSet->EndSimulationWrite(queue);
#ifdef PROFILE_GPU
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &postSyncSumEnd));
			float postSyncSumTime = 0.000000001f * Measure(postSyncSumEnd, postSyncSumEnd);
#endif

#ifdef DEBUG_BUFFERS_GPU
			DebugHashes(*simulationWriteBufferSet, dynamicParticleWriteHashMapBuffer);
#endif

#ifdef PROFILE_GPU
			Console::WriteLine(
				StringParsing::Convert(prepareAndSyncTime) + " " +
				StringParsing::Convert(updateParticlePressureTime) + " " +
				StringParsing::Convert(updateParticleDynamicTime) + " " +
				StringParsing::Convert(partialSumTime) + " " +
				StringParsing::Convert(computeParticleMapTime) + " " +
				StringParsing::Convert(postSyncSumTime)
			);
#endif

			std::swap(dynamicParticleReadHashMapBuffer, dynamicParticleWriteHashMapBuffer);

			simulationReadBufferSetIndex = (simulationReadBufferSetIndex + 1) % bufferSetsPointers.Count();
			simulationWriteBufferSetIndex = (simulationWriteBufferSetIndex + 1) % bufferSetsPointers.Count();
			simulationReadBufferSet = bufferSetsPointers[simulationReadBufferSetIndex];
			simulationWriteBufferSet = bufferSetsPointers[simulationWriteBufferSetIndex];			

			++stepCount;
		}

		if (profiling)		
			lastTimePerStep_ns = Measure(updateStartEvent, updateEndEvent) / simulationSteps;		
		
		simulationTime += deltaTime * simulationSteps;
	}	
	void SystemGPU::StartRender()
	{		
		if (bufferSetsPointers.Empty())
			return;

		renderBufferSetIndex = simulationReadBufferSetIndex;		
		renderBufferSet = bufferSetsPointers[renderBufferSetIndex];
		renderBufferSet->StartRender(queue);		
	}
	Graphics::OpenGLWrapper::VertexArray* SystemGPU::GetDynamicParticlesVertexArray()
	{
		if (bufferSetsPointers.Empty())
			return nullptr;

		return &renderBufferSet->dynamicParticleVertexArray;
	}
	Graphics::OpenGLWrapper::VertexArray* SystemGPU::GetStaticParticlesVertexArray()
	{
		return &staticParticleVertexArray;
	}
	void SystemGPU::EndRender()
	{
		if (bufferSetsPointers.Empty())
			return;

		renderBufferSet->EndRender(queue);				
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
		reorderParticlesKernel = cl::Kernel(SPHProgram, "reorderParticles", &ret);
		CL_CHECK();

		scanKernelElementCountPerGroup = std::min(
			scanOnComputeGroupsKernel.getWorkGroupInfo< CL_KERNEL_WORK_GROUP_SIZE>(clContext.device),
			addToComputeGroupArraysKernel.getWorkGroupInfo< CL_KERNEL_WORK_GROUP_SIZE>(clContext.device)
		) * 2;
	}
	void SystemGPU::CreateStaticParticlesBuffers(Array<StaticParticle>& staticParticles, uintMem hashesPerStaticParticle, float maxInteractionDistance)
	{		
		cl_int ret;

		staticParticleCount = staticParticles.Count();
		staticParticleHashMapSize = hashesPerStaticParticle * staticParticleCount;

		Array<uint32> staticParticleHashMap;
		staticParticleHashMap.Resize(staticParticleHashMapSize + 1);

		auto GetStaticParticleHash = [maxInteractionDistance = maxInteractionDistance, mod = staticParticleHashMapSize](const StaticParticle& particle) {
			return GetHash(GetCell(particle.position, maxInteractionDistance)) % mod;
			};

		staticParticles = GenerateHashMapAndReorderParticles(staticParticles, staticParticleHashMap, GetStaticParticleHash);

#ifdef DEBUG_BUFFERS_GPU
		System::DebugParticles<StaticParticle>(staticParticles, maxInteractionDistance, staticParticleHashMapSize);
		System::DebugHashAndParticleMap<StaticParticle, uint32>(staticParticles, staticParticleHashMap, {}, GetStaticParticleHash);
#endif

		staticParticleBufferGL.Allocate(staticParticles.Ptr(), sizeof(StaticParticle) * staticParticles.Count());
		
		staticParticleVertexArray.EnableVertexAttribute(0);
		staticParticleVertexArray.SetVertexAttributeFormat(0, Graphics::OpenGLWrapper::VertexAttributeType::Float, 3, false, offsetof(StaticParticle, position));
		staticParticleVertexArray.SetVertexAttributeBuffer(0, &staticParticleBufferGL, sizeof(StaticParticle), 0);
		staticParticleVertexArray.SetVertexAttributeDivisor(0, 1);

		staticParticleBuffer = cl::Buffer(clContext.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(StaticParticle) * staticParticles.Count(), (void*)staticParticles.Ptr(), &ret);
		CL_CHECK();

		staticHashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(uint32) * staticParticleHashMap.Count(), (void*)staticParticleHashMap.Ptr(), &ret);
		CL_CHECK();
	}
	void SystemGPU::CreateDynamicParticlesBuffers(Array<DynamicParticle>& dynamicParticles, uintMem bufferCount, uintMem hashesPerDynamicParticle, float maxInteractionDistance)
	{
		dynamicParticleCount = dynamicParticles.Count();
		//find the best combination to be > than the target dynamicParticleHashMapSize but still be in the form of dynamicParticleHashMapSize = pow(scanKernelElementCountPerGroup, n) where n is a natural number
		{
			uintMem dynamicParticleHashMapSizeTarget = hashesPerDynamicParticle * dynamicParticleCount;
			uint layerCount = std::ceil(std::log(dynamicParticleHashMapSizeTarget) / std::log(scanKernelElementCountPerGroup));
			hashMapBufferGroupSize = 1Ui64 << (uint)std::ceil(std::log2(std::pow<float>(dynamicParticleHashMapSizeTarget, 1.0f / layerCount)));
			dynamicParticleHashMapSize = std::pow(hashMapBufferGroupSize, layerCount);
		}

#ifdef DEBUG_BUFFERS_GPU
		debugMaxInteractionDistance = maxInteractionDistance;
		debugParticlesArray.Resize(dynamicParticleCount);
		debugHashMapArray.Resize(dynamicParticleHashMapSize + 1);
		debugParticleMapArray.Resize(dynamicParticleCount);
#endif		

		if (bufferCount < 2)
		{
			Debug::Logger::LogWarning("Client", "SystemInitParameters bufferCount member was set to " + StringParsing::Convert(bufferCount) + ". Only values above 1 are valid. The value was set to 2");
			bufferCount = 2;
		}		

		for (auto& bufferSetPointer : bufferSetsPointers)
			delete bufferSetPointer;
		bufferSetsPointers.Resize(bufferCount);

		auto AllocateBufferSet = [&]<typename T>() {			
			for (auto& bufferSetPointer : bufferSetsPointers)
				bufferSetPointer = new T(clContext, dynamicParticles);
		};

		if (clContext.supportedCLGLInterop)
			AllocateBufferSet.template operator()<ParticleBufferSetInterop > ();
		else
			AllocateBufferSet.template operator()<ParticleBufferSetNoInterop > ();

		cl_int ret;		

		dynamicParticleWriteHashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * (dynamicParticleHashMapSize + 1), nullptr, &ret);
		CL_CHECK();
		dynamicParticleReadHashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * (dynamicParticleHashMapSize + 1), nullptr, &ret);
		CL_CHECK();
		particleMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * dynamicParticleCount, nullptr, &ret);
		CL_CHECK();
		dynamicParticleIntermediateBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(DynamicParticle) * dynamicParticleCount, nullptr, &ret);
		CL_CHECK();

		uint32 pattern0 = 0;
		uint32 patternCount = dynamicParticleCount;
		CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleReadHashMapBuffer(), &pattern0, sizeof(uint32), 0, dynamicParticleHashMapSize * sizeof(uint32), 0, nullptr, nullptr));
		CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleReadHashMapBuffer(), &patternCount, sizeof(uint32), dynamicParticleHashMapSize * sizeof(uint32), sizeof(uint32), 0, nullptr, nullptr));
		CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleWriteHashMapBuffer(), &pattern0, sizeof(uint32), 0, dynamicParticleHashMapSize * sizeof(uint32), 0, nullptr, nullptr));
		CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleWriteHashMapBuffer(), &patternCount, sizeof(uint32), dynamicParticleHashMapSize * sizeof(uint32), sizeof(uint32), 0, nullptr, nullptr));

		auto bufferSet = bufferSetsPointers[0];

		bufferSet->StartSimulationWrite(queue);
		EnqueueComputeParticleHashesKernel(bufferSet, dynamicParticleCount, dynamicParticleReadHashMapBuffer, dynamicParticleHashMapSize, maxInteractionDistance, nullptr);

#ifdef DEBUG_BUFFERS_GPU
		DebugParticles(*bufferSet);
		DebugPrePrefixSumHashes(*bufferSet, dynamicParticleReadHashMapBuffer);
#endif

		EnqueuePartialSumKernels(dynamicParticleReadHashMapBuffer, dynamicParticleHashMapSize, hashMapBufferGroupSize, 0);
		EnqueueComputeParticleMapKernel(bufferSet, dynamicParticleCount, dynamicParticleReadHashMapBuffer, particleMapBuffer, nullptr);
		bufferSet->EndSimulationWrite(queue);

#ifdef DEBUG_BUFFERS_GPU				
		DebugHashes(*bufferSet, dynamicParticleReadHashMapBuffer);
#endif
	}
	void SystemGPU::InitializeInternal(const SystemInitParameters& initParams)
	{
		renderBufferSetIndex = 0;
		simulationReadBufferSetIndex = 0;
		simulationWriteBufferSetIndex = 1;
		renderBufferSet = bufferSetsPointers[renderBufferSetIndex];
		simulationReadBufferSet = bufferSetsPointers[simulationReadBufferSetIndex];
		simulationWriteBufferSet = bufferSetsPointers[simulationWriteBufferSetIndex];

		cl_int ret;
		ParticleSimulationParameters simulationParameters;
		simulationParameters.behaviour = initParams.particleBehaviourParameters;
		simulationParameters.bounds = initParams.particleBoundParameters;
		simulationParameters.dynamicParticleCount = dynamicParticleCount;
		simulationParameters.dynamicParticleHashMapSize = dynamicParticleHashMapSize;
		simulationParameters.staticParticleCount = staticParticleCount;
		simulationParameters.staticParticleHashMapSize = staticParticleHashMapSize;
		simulationParameters.smoothingKernelConstant = SmoothingKernelConstant(initParams.particleBehaviourParameters.maxInteractionDistance);
		simulationParameters.selfDensity = initParams.particleBehaviourParameters.particleMass * SmoothingKernelD0(0, initParams.particleBehaviourParameters.maxInteractionDistance) * simulationParameters.smoothingKernelConstant;
		simulationParametersBuffer = cl::Buffer(clContext.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(ParticleSimulationParameters), &simulationParameters, &ret);
		CL_CHECK();
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
	void SystemGPU::EnqueueUpdateParticlesPressureKernel() 
	{
		cl_int ret;

		CL_CALL(updateParticlesPressureKernel.setArg(0, simulationReadBufferSet->GetDynamicParticleBufferCL()));
		CL_CALL(updateParticlesPressureKernel.setArg(1, simulationWriteBufferSet->GetDynamicParticleBufferCL()));
		CL_CALL(updateParticlesPressureKernel.setArg(2, dynamicParticleReadHashMapBuffer));		
		CL_CALL(updateParticlesPressureKernel.setArg(3, particleMapBuffer));						
		CL_CALL(updateParticlesPressureKernel.setArg(4, staticParticleBuffer));		
		CL_CALL(updateParticlesPressureKernel.setArg(5, staticHashMapBuffer));
		CL_CALL(updateParticlesPressureKernel.setArg(6, simulationParametersBuffer));


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
		CL_CALL(updateParticlesDynamicsKernel.setArg(2, dynamicParticleReadHashMapBuffer));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(3, dynamicParticleWriteHashMapBuffer));		
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
	void SystemGPU::EnqueuePartialSumKernels(cl::Buffer& buffer, uintMem elementCount, uintMem groupSize, uintMem offset)
	{		
		cl_int ret;					

		for (uintMem size = elementCount; size != 1; size /= groupSize)
		{									
			CL_CALL(scanOnComputeGroupsKernel.setArg(0, (groupSize + 1) * sizeof(uint32), nullptr));
			CL_CALL(scanOnComputeGroupsKernel.setArg(1, buffer));
			CL_CALL(scanOnComputeGroupsKernel.setArg(2, (uint32)(elementCount / size)));
			CL_CALL(scanOnComputeGroupsKernel.setArg(3, (uint32)offset));

			size_t globalWorkOffset = 0;
			size_t globalWorkSize = size / 2;
			size_t localWorkSize = groupSize / 2;
			CL_CALL(clEnqueueNDRangeKernel(queue(), scanOnComputeGroupsKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));			
		}

		for (uintMem i = 1; i != elementCount / groupSize; i *= groupSize)
		{			
			CL_CALL(addToComputeGroupArraysKernel.setArg(0, buffer));
			CL_CALL(addToComputeGroupArraysKernel.setArg(1, (uint32)i));			
			CL_CALL(addToComputeGroupArraysKernel.setArg(2, (uint32)offset));

			size_t globalWorkOffset = 0;
			size_t globalWorkSize = elementCount / i - groupSize - groupSize + 1;
			size_t localWorkSize = groupSize - 1;
			CL_CALL(clEnqueueNDRangeKernel(queue(), addToComputeGroupArraysKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));
		}
			
	}
	void SystemGPU::EnqueueReorderParticles(cl::Buffer& inArray, cl::Buffer& outArray, cl::Buffer& particleMap, cl_event* finishedEvent)
	{
		cl_int ret;

		CL_CALL(reorderParticlesKernel.setArg(0, inArray));
		CL_CALL(reorderParticlesKernel.setArg(1, outArray));
		CL_CALL(reorderParticlesKernel.setArg(2, particleMap));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = 1;
		CL_CALL(clEnqueueNDRangeKernel(queue(), reorderParticlesKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, finishedEvent));
	}
#ifdef DEBUG_BUFFERS_GPU
	void SystemGPU::DebugParticles(ParticleBufferSet& bufferSet)
	{		
		queue.finish();
		queue.enqueueReadBuffer(bufferSet.GetDynamicParticleBufferCL(), CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), nullptr, nullptr);
		
		System::DebugParticles<DynamicParticle>(debugParticlesArray, debugMaxInteractionDistance, dynamicParticleHashMapSize);
	}
	void SystemGPU::DebugPrePrefixSumHashes(ParticleBufferSet& bufferSet, cl::Buffer& hashMapBuffer)
	{						
		queue.finish();
		queue.enqueueReadBuffer(hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticleHashMapSize + 1), debugHashMapArray.Ptr(), nullptr, nullptr);
		queue.enqueueReadBuffer(bufferSet.GetDynamicParticleBufferCL(), CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), nullptr, nullptr);
		
		System::DebugPrePrefixSumHashes<DynamicParticle>(debugParticlesArray, debugHashMapArray);
	}
	void SystemGPU::DebugHashes(ParticleBufferSet& bufferSet, cl::Buffer& hashMapBuffer)
	{		
		queue.finish();
		queue.enqueueReadBuffer(hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticleHashMapSize + 1), debugHashMapArray.Ptr(), nullptr, nullptr);
		queue.enqueueReadBuffer(particleMapBuffer, CL_TRUE, 0, sizeof(uint32) * dynamicParticleCount, debugParticleMapArray.Ptr(), nullptr, nullptr);
		queue.enqueueReadBuffer(bufferSet.GetDynamicParticleBufferCL(), CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), nullptr, nullptr);

		System::DebugHashAndParticleMap<DynamicParticle, uint32>(debugParticlesArray, debugHashMapArray, debugParticleMapArray);
	}
#endif
}