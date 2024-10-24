 #include "pch.h"
#include "SystemGPU.h"
#include "gl/glew.h"
#include "GL/glext.h"
#include "SPH/ParticleBufferSet/GPUParticleBufferSet.h"

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

	SystemGPU::SystemGPU(OpenCLContext& clContext, cl::CommandQueue& queue, Graphics::OpenGL::GraphicsContext_OpenGL& glContext) :
		clContext(clContext), queue(queue), glContext(glContext),
		userOpenCLOpenGLSync(false),		
		dynamicParticleCount(0), staticParticleCount(0), dynamicParticleHashMapSize(0), staticParticleHashMapSize(0),
		hashMapBufferGroupSize(0), simulationTime(0), lastTimePerStep_s(0), profiling(false)
	{
		if (Graphics::OpenGL::GraphicsContext_OpenGL::IsExtensionSupported("GL_ARB_cl_event"))
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension supported. The application implementation could be made better to use this extension. This is just informative");
		else
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension not supported. Even if it was supported it isn't implemented in the application. This is just informative");

		cl_int ret;				
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
		reorderStepCount = 20;
		
		lastTimePerStep_s = 0;
		simulationTime = 0;					
	}	
	void SystemGPU::Update(float deltaTime, uint simulationSteps)
	{		
		if (this->particleBufferSet == nullptr)
		{
			Debug::Logger::LogWarning("Client", "Updating a uninitialized SPHSystem");
			return;
		}

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

			auto& particleReadBufferHandle = this->particleBufferSet->GetReadBufferHandle();
			auto& particleWriteBufferHandle = this->particleBufferSet->GetWriteBufferHandle();

#ifdef PROFILE_GPU
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &prepareAndSyncEnd));
			float prepareAndSyncTime = 0.000000001f * Measure(prepareAndSyncStart, prepareAndSyncEnd);

			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &updateParticlePressureStart));
#endif
			EnqueueUpdateParticlesPressureKernel(particleReadBufferHandle.GetReadBuffer(), particleWriteBufferHandle.GetWriteBuffer());
#ifdef PROFILE_GPU
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &updateParticlePressureEnd));
			float updateParticlePressureTime = 0.000000001f * Measure(updateParticlePressureStart, updateParticlePressureEnd);

			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &updateParticleDynamicStart));
#endif
			EnqueueUpdateParticlesDynamicsKernel(particleReadBufferHandle.GetReadBuffer(), particleWriteBufferHandle.GetWriteBuffer(), deltaTime);
#ifdef PROFILE_GPU
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &updateParticleDynamicEnd));			
			float updateParticleDynamicTime = 0.000000001f * Measure(updateParticleDynamicStart, updateParticleDynamicEnd);
#endif

#ifdef DEBUG_BUFFERS_GPU
			DebugParticles(particleBufferSets[simulationWriteBufferSetIndex].GPUData);
			DebugPrePrefixSumHashes(particleBufferSets[simulationWriteBufferSetIndex].GPUData, dynamicParticleWriteHashMapBuffer);
#endif


#ifdef PROFILE_GPU
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &partialSumStart));
#endif
			EnqueuePartialSumKernels(dynamicParticleWriteHashMapBuffer, dynamicParticleHashMapSize, hashMapBufferGroupSize, 0);
#ifdef PROFILE_GPU
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &partialSumEnd));
			float partialSumTime = 0.000000001f * Measure(partialSumStart, partialSumEnd);

			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &computeParticleMapStart));
#endif
			bool reorderParticles = reorderStepCount != 0 && (stepCount % reorderStepCount) == 0;
			
			EnqueueComputeParticleMapKernel(particleWriteBufferHandle.GetWriteBuffer(), nullptr, dynamicParticleCount, dynamicParticleWriteHashMapBuffer, particleMapBuffer, (profiling ? &updateEndEvent() : nullptr));
			//EnqueueComputeParticleMapKernel(*particleWriteBuffer, (reorderParticles ? &dynamicParticleIntermediateBuffer : nullptr), dynamicParticleCount, dynamicParticleWriteHashMapBuffer, particleMapBuffer, (profiling ? &updateEndEvent() : nullptr));
			//if (reorderParticles)
			//	particleBufferSet->ReorderParticles();
			//	std::swap(particleBufferSets[simulationWriteBufferSetIndex].GPUData.GetDynamicParticleBufferCL(), dynamicParticleIntermediateBuffer);

#ifdef PROFILE_GPU
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &computeParticleMapEnd));
			float computeParticleMapTime = 0.000000001f * Measure(computeParticleMapStart, computeParticleMapEnd);

			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &postSyncSumStart));
#endif
			particleReadBufferHandle.FinishRead();
			particleWriteBufferHandle.FinishWrite();
			this->particleBufferSet->Advance();

#ifdef PROFILE_GPU
			CL_CALL(queue.enqueueMarkerWithWaitList(nullptr, &postSyncSumEnd));
			float postSyncSumTime = 0.000000001f * Measure(postSyncSumEnd, postSyncSumEnd);
#endif

#ifdef DEBUG_BUFFERS_GPU
			DebugHashes(particleBufferSets[simulationWriteBufferSetIndex].GPUData, dynamicParticleWriteHashMapBuffer);
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

			++stepCount;
		}

		if (profiling)		
			lastTimePerStep_s = (double)Measure(updateStartEvent, updateEndEvent) / simulationSteps / 1000000000;
		
		simulationTime += deltaTime * simulationSteps;
	}	
	void SystemGPU::LoadKernels()
	{
		cl_int ret;

		partialSumProgram = BuildOpenCLProgram(clContext, Array<Path>{ "assets/kernels/partialSum.cl" }, {});

		scanOnComputeGroupsKernel = cl::Kernel(partialSumProgram, "scanOnComputeGroups", &ret);
		CL_CHECK();
		addToComputeGroupArraysKernel = cl::Kernel(partialSumProgram, "addToComputeGroupArrays", &ret);
		CL_CHECK();

		SPHProgram = BuildOpenCLProgram(clContext, Array<Path>{ "assets/kernels/CL_CPP_SPHDeclarations.h", "assets/kernels/CL_CPP_SPHFunctions.h", "assets/kernels/SPH.cl" }, { {"CL_COMPILER"}});

		computeParticleHashesKernel = cl::Kernel(SPHProgram, "computeParticleHashes", &ret);
		CL_CHECK();
		computeParticleMapKernel = cl::Kernel(SPHProgram, "computeParticleMap", &ret);
		CL_CHECK();
		updateParticlesPressureKernel = cl::Kernel(SPHProgram, "updateParticlesPressure", &ret);
		CL_CHECK();
		updateParticlesDynamicsKernel = cl::Kernel(SPHProgram, "updateParticlesDynamics", &ret);
		CL_CHECK();		

		computeParticleHashesKernelPreferredGroupSize = computeParticleHashesKernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(clContext.device);
		scanOnComputeGroupsKernelPreferredGroupSize = scanOnComputeGroupsKernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(clContext.device);
		addToComputeGroupArraysKernelPreferredGroupSize = addToComputeGroupArraysKernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(clContext.device);
		computeParticleMapKernelPreferredGroupSize = computeParticleMapKernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(clContext.device);
		updateParticlesPressureKernelPreferredGroupSize = updateParticlesPressureKernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(clContext.device);
		updateParticlesDynamicsKernelPreferredGroupSize = updateParticlesDynamicsKernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(clContext.device);		
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
		
		staticParticleBuffer = cl::Buffer(clContext.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(StaticParticle) * staticParticles.Count(), (void*)staticParticles.Ptr(), &ret);
		CL_CHECK();

		staticHashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(uint32) * staticParticleHashMap.Count(), (void*)staticParticleHashMap.Ptr(), &ret);
		CL_CHECK();
	}
	void SystemGPU::CreateDynamicParticlesBuffers(ParticleBufferSet& particleBufferSet, uintMem hashesPerDynamicParticle, float maxInteractionDistance)
	{		
		this->particleBufferSet = &dynamic_cast<GPUParticleBufferSet&>(particleBufferSet);

		dynamicParticleCount = this->particleBufferSet->GetDynamicParticleCount();

		//find the best combination to be > than the target dynamicParticleHashMapSize but still be in the form of dynamicParticleHashMapSize = pow(scanKernelElementCountPerGroup, n) where n is a natural number
		{
			uintMem maxWorkGroupSize1 = scanOnComputeGroupsKernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(clContext.device);
			uintMem preferredWorkGroupSize1 = scanOnComputeGroupsKernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(clContext.device);
			uintMem maxWorkGroupSize2 = addToComputeGroupArraysKernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(clContext.device);
			uintMem preferredWorkGroupSize2 = addToComputeGroupArraysKernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(clContext.device);

			uintMem dynamicParticleHashMapSizeTarget = hashesPerDynamicParticle * dynamicParticleCount;
			uint layerCount = std::ceil(std::log(dynamicParticleHashMapSizeTarget) / std::log(std::min(maxWorkGroupSize1, maxWorkGroupSize2) * 2));
			hashMapBufferGroupSize = 1Ui64 << (uint)std::floor(std::log2(std::pow<float>(dynamicParticleHashMapSizeTarget, 1.0f / layerCount)));
			dynamicParticleHashMapSize = std::pow(hashMapBufferGroupSize, layerCount);
		}

#ifdef DEBUG_BUFFERS_GPU
		debugMaxInteractionDistance = maxInteractionDistance;
		debugParticlesArray.Resize(dynamicParticleCount);
		debugHashMapArray.Resize(dynamicParticleHashMapSize + 1);
		debugParticleMapArray.Resize(dynamicParticleCount);
#endif		

		cl_int ret;		

		dynamicParticleWriteHashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * (dynamicParticleHashMapSize + 1), nullptr, &ret);
		CL_CHECK();
		dynamicParticleReadHashMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * (dynamicParticleHashMapSize + 1), nullptr, &ret);
		CL_CHECK();
		particleMapBuffer = cl::Buffer(clContext.context, CL_MEM_READ_WRITE, sizeof(uint32) * dynamicParticleCount, nullptr, &ret);
		CL_CHECK();		

		uint32 pattern0 = 0;
		uint32 patternCount = dynamicParticleCount;
		CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleReadHashMapBuffer(), &pattern0, sizeof(uint32), 0, dynamicParticleHashMapSize * sizeof(uint32), 0, nullptr, nullptr));
		CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleReadHashMapBuffer(), &patternCount, sizeof(uint32), dynamicParticleHashMapSize * sizeof(uint32), sizeof(uint32), 0, nullptr, nullptr));
		CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleWriteHashMapBuffer(), &pattern0, sizeof(uint32), 0, dynamicParticleHashMapSize * sizeof(uint32), 0, nullptr, nullptr));
		CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleWriteHashMapBuffer(), &patternCount, sizeof(uint32), dynamicParticleHashMapSize * sizeof(uint32), sizeof(uint32), 0, nullptr, nullptr));
		
		{
			auto& particlesWriteBufferHandle = this->particleBufferSet->GetWriteBufferHandle();
			EnqueueComputeParticleHashesKernel(particlesWriteBufferHandle.GetWriteBuffer(), dynamicParticleCount, dynamicParticleReadHashMapBuffer, dynamicParticleHashMapSize, maxInteractionDistance, nullptr);

#ifdef DEBUG_BUFFERS_GPU
			DebugParticles(particleBufferSets[0].GPUData);
			DebugPrePrefixSumHashes(particleBufferSets[0].GPUData, dynamicParticleReadHashMapBuffer);
#endif

			EnqueuePartialSumKernels(dynamicParticleReadHashMapBuffer, dynamicParticleHashMapSize, hashMapBufferGroupSize, 0);
			EnqueueComputeParticleMapKernel(particlesWriteBufferHandle.GetWriteBuffer(), nullptr, dynamicParticleCount, dynamicParticleReadHashMapBuffer, particleMapBuffer, nullptr);

#ifdef DEBUG_BUFFERS_GPU				
			DebugHashes(particleBufferSets[0].GPUData, dynamicParticleReadHashMapBuffer);
#endif		

			particlesWriteBufferHandle.FinishWrite();			
			this->particleBufferSet->Advance();
		}
	}
	void SystemGPU::InitializeInternal(const SystemInitParameters& initParams)
	{		
		initParams.ParseImplementationSpecifics((StringView)"OpenCL",
			SystemInitParameters::ImplementationSpecificsEntry{"reorderStepCount", reorderStepCount}
			);		

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
	void SystemGPU::EnqueueComputeParticleHashesKernel(cl::Buffer& particles, uintMem dynamicParticleCount, cl::Buffer& dynamicParticleWriteHashMapBuffer, uintMem dynamicParticleHashMapSize, float maxInteractionDistance, cl_event* finishedEvent)
	{
		cl_int ret;

		CL_CALL(computeParticleHashesKernel.setArg(0, particles));
		CL_CALL(computeParticleHashesKernel.setArg(1, dynamicParticleWriteHashMapBuffer));		
		CL_CALL(computeParticleHashesKernel.setArg(2, maxInteractionDistance));
		CL_CALL(computeParticleHashesKernel.setArg(3, (uint32)dynamicParticleHashMapSize));
		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = computeParticleHashesKernelPreferredGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(queue(), computeParticleHashesKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, finishedEvent));
	}
	void SystemGPU::EnqueueComputeParticleMapKernel(cl::Buffer& particles, cl::Buffer* orderedParticles, uintMem dynamicParticleCount, cl::Buffer& dynamicParticleWriteHashMapBuffer, cl::Buffer& particleMapBuffer, cl_event* finishedEvent)
	{
		cl_int ret;

		CL_CALL(computeParticleMapKernel.setArg(0, particles));		
		CL_CALL(computeParticleMapKernel.setArg(1, orderedParticles));
		CL_CALL(computeParticleMapKernel.setArg(2, dynamicParticleWriteHashMapBuffer));		
		CL_CALL(computeParticleMapKernel.setArg(3, particleMapBuffer));
		CL_CALL(computeParticleMapKernel.setArg(4, (orderedParticles != nullptr ? 1 : 0)));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = computeParticleMapKernelPreferredGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(queue(), computeParticleMapKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, finishedEvent));
	}
	void SystemGPU::EnqueueUpdateParticlesPressureKernel(const cl::Buffer& particleReadBuffer, cl::Buffer& particleWriteBuffer)
	{
		cl_int ret;

		CL_CALL(updateParticlesPressureKernel.setArg(0, particleReadBuffer));
		CL_CALL(updateParticlesPressureKernel.setArg(1, particleWriteBuffer));
		CL_CALL(updateParticlesPressureKernel.setArg(2, dynamicParticleReadHashMapBuffer));		
		CL_CALL(updateParticlesPressureKernel.setArg(3, particleMapBuffer));						
		CL_CALL(updateParticlesPressureKernel.setArg(4, staticParticleBuffer));		
		CL_CALL(updateParticlesPressureKernel.setArg(5, staticHashMapBuffer));
		CL_CALL(updateParticlesPressureKernel.setArg(6, simulationParametersBuffer));


		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = updateParticlesPressureKernelPreferredGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(queue(), updateParticlesPressureKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 0, nullptr, nullptr));
	}
	void SystemGPU::EnqueueUpdateParticlesDynamicsKernel(const cl::Buffer& particleReadBuffer, cl::Buffer& particleWriteBuffer, float deltaTime)
	{
		bool moveParticles = false;		

		cl_int ret;

		CL_CALL(updateParticlesDynamicsKernel.setArg(0, particleReadBuffer));
		CL_CALL(updateParticlesDynamicsKernel.setArg(1, particleWriteBuffer));
		CL_CALL(updateParticlesDynamicsKernel.setArg(2, dynamicParticleReadHashMapBuffer));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(3, dynamicParticleWriteHashMapBuffer));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(4, particleMapBuffer));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(5, staticParticleBuffer));
		CL_CALL(updateParticlesDynamicsKernel.setArg(6, staticHashMapBuffer));
		CL_CALL(updateParticlesDynamicsKernel.setArg(7, deltaTime));		
		CL_CALL(updateParticlesDynamicsKernel.setArg(8, simulationParametersBuffer));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = updateParticlesDynamicsKernelPreferredGroupSize;
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
#ifdef DEBUG_BUFFERS_GPU
	void SystemGPU::DebugParticles(GPUParticleBufferSet& bufferSet)
	{		
		queue.finish();
		queue.enqueueReadBuffer(bufferSet.GetDynamicParticleBufferCL(), CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), nullptr, nullptr);
		
		System::DebugParticles<DynamicParticle>(debugParticlesArray, debugMaxInteractionDistance, dynamicParticleHashMapSize);
	}
	void SystemGPU::DebugPrePrefixSumHashes(GPUParticleBufferSet& bufferSet, cl::Buffer& hashMapBuffer)
	{						
		queue.finish();
		queue.enqueueReadBuffer(hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticleHashMapSize + 1), debugHashMapArray.Ptr(), nullptr, nullptr);
		queue.enqueueReadBuffer(bufferSet.GetDynamicParticleBufferCL(), CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), nullptr, nullptr);
		
		System::DebugPrePrefixSumHashes<DynamicParticle>(debugParticlesArray, debugHashMapArray);
	}
	void SystemGPU::DebugHashes(GPUParticleBufferSet& bufferSet, cl::Buffer& hashMapBuffer)
	{		
		queue.finish();
		queue.enqueueReadBuffer(hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticleHashMapSize + 1), debugHashMapArray.Ptr(), nullptr, nullptr);
		queue.enqueueReadBuffer(particleMapBuffer, CL_TRUE, 0, sizeof(uint32) * dynamicParticleCount, debugParticleMapArray.Ptr(), nullptr, nullptr);
		queue.enqueueReadBuffer(bufferSet.GetDynamicParticleBufferCL(), CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), nullptr, nullptr);

		System::DebugHashAndParticleMap<DynamicParticle, uint32>(debugParticlesArray, debugHashMapArray, debugParticleMapArray);
	}
#endif
}