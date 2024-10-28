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
		hashMapBufferGroupSize(0), simulationTime(0), profiling(false),
		nonUniformWorkGroupSizeSupported(true)
	{
		if (Graphics::OpenGL::GraphicsContext_OpenGL::IsExtensionSupported("GL_ARB_cl_event"))
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension supported. The application implementation could be made better to use this extension. This is just informative");
		else
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension not supported. Even if it was supported it isn't implemented in the application. This is just informative");

		cl_int ret;
		CL_CALL(clGetDeviceInfo(clContext.device(), CL_DEVICE_PREFERRED_INTEROP_USER_SYNC, sizeof(cl_bool), &userOpenCLOpenGLSync, nullptr))

		LoadKernels();

		nonUniformWorkGroupSizeSupported = clContext.device.getInfo<CL_DEVICE_NON_UNIFORM_WORK_GROUP_SUPPORT>();		
	}
	SystemGPU::~SystemGPU()
	{
		Clear();
	}
	void SystemGPU::Clear()
	{

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

		reorderElapsedTime = 0.0f;
		reorderTimeInterval = 2.0;
		detailedProfiling = false;
		openCLChoosesGroupSize = true;
		useMaxGroupSize = false;

		simulationTime = 0;
	}
	void SystemGPU::Update(float deltaTime, uint simulationSteps)
	{
		if (this->particleBufferSet == nullptr)
		{
			Debug::Logger::LogWarning("Client", "Updating a uninitialized SPHSystem");
			return;
		}

		auto Measure = [](cl::Event& start, cl::Event& end) -> double {

			start.wait();
			end.wait();
			cl_int ret;

			uint64 startTime = start.getProfilingInfo<CL_PROFILING_COMMAND_START>(&ret);
			CL_CHECK(0);
			uint64 endTime = end.getProfilingInfo<CL_PROFILING_COMMAND_END>(&ret);
			CL_CHECK(0);

			return double(endTime - startTime) / 1000000000;
			};
		auto InsertMeasurement = [&](StringView name, cl::Event& start, cl::Event& end)
			{
				auto it = systemProfilingData.implementationSpecific.Insert<float>(name, 0).iterator;
				*it.GetValue<float>() += Measure(start, end) / simulationSteps;
			};		

		cl_int ret;

		if (detailedProfiling)
			for (auto it = systemProfilingData.implementationSpecific.FirstIterator(); it != systemProfilingData.implementationSpecific.BehindIterator(); ++it)
				if (float* value = it.GetValue<float>())
					*value = 0;
		
		cl::Event updateStartEvent;
		cl::Event updateEndEvent;

		for (uint i = 0; i < simulationSteps; ++i)
		{
			auto& inputParticleBufferHandle = this->particleBufferSet->GetReadBufferHandle();
			auto* outputParticleBufferHandle = &this->particleBufferSet->GetWriteBufferHandle();			

			cl::Event readStartEvent, writeStartEvent;
			inputParticleBufferHandle.StartRead(&readStartEvent());
			outputParticleBufferHandle->StartWrite(&writeStartEvent());

			cl::Event clearHashMapFinishedEvent;			
			EnqueueClearHashMap( { writeStartEvent() != nullptr ? &writeStartEvent() : nullptr, (uintMem)(writeStartEvent() != nullptr ? 1 : 0)}, &clearHashMapFinishedEvent());

			cl::Event updatePressureFinishedEvent;
			uintMem updateParticlesWaitEventCount = 0;
			cl_event updateParticlesWaitEvents[2]{ };
			if (readStartEvent() != nullptr) updateParticlesWaitEvents[updateParticlesWaitEventCount++] = readStartEvent();
			if (writeStartEvent() != nullptr) updateParticlesWaitEvents[updateParticlesWaitEventCount++] = writeStartEvent();
			EnqueueUpdateParticlesPressureKernel(inputParticleBufferHandle.GetReadBuffer(), outputParticleBufferHandle->GetWriteBuffer(), { updateParticlesWaitEvents, updateParticlesWaitEventCount }, &updatePressureFinishedEvent());

			cl::Event updateDynamicsFinishedEvent;
			EnqueueUpdateParticlesDynamicsKernel(inputParticleBufferHandle.GetReadBuffer(), outputParticleBufferHandle->GetWriteBuffer(), deltaTime, { &updatePressureFinishedEvent(), 1 }, &updateDynamicsFinishedEvent());

#ifdef DEBUG_BUFFERS_GPU
			DebugParticles(outputParticleBufferHandle->GetWriteBuffer());
			DebugPrePrefixSumHashes(outputParticleBufferHandle->GetWriteBuffer(), dynamicParticleWriteHashMapBuffer);
#endif

			cl_event partialSumWaitEvents[]{ clearHashMapFinishedEvent(), updateDynamicsFinishedEvent() };
			cl::Event partialSumFinishedEvent;
			EnqueuePartialSumKernels(dynamicParticleWriteHashMapBuffer, dynamicParticleHashMapSize, hashMapBufferGroupSize, partialSumWaitEvents, &partialSumFinishedEvent());
						
			cl::Event computeParticleMapFinishedEvent;
			if (reorderElapsedTime > reorderTimeInterval)
			{
				reorderElapsedTime -= reorderTimeInterval;

				particleBufferSet->Advance();
				auto* intermediateBuffer = &particleBufferSet->GetWriteBufferHandle();

				cl::Event startIntermediateEvent;
				intermediateBuffer->StartWrite(&startIntermediateEvent());

				uintMem computeParticleMapWaitEventCount = 0;
				cl_event computeParticleMapWaitEvents[2]{ }; 
				if (startIntermediateEvent() != nullptr) computeParticleMapWaitEvents[computeParticleMapWaitEventCount++] = startIntermediateEvent();
				if (partialSumFinishedEvent() != nullptr) computeParticleMapWaitEvents[computeParticleMapWaitEventCount++] = partialSumFinishedEvent();
				EnqueueComputeParticleMapKernel(outputParticleBufferHandle->GetWriteBuffer(), &intermediateBuffer->GetWriteBuffer(), dynamicParticleCount, dynamicParticleWriteHashMapBuffer, particleMapBuffer, { computeParticleMapWaitEvents, computeParticleMapWaitEventCount }, &computeParticleMapFinishedEvent());

				outputParticleBufferHandle->FinishWrite({ &computeParticleMapFinishedEvent(), 1 }, false);

				std::swap(intermediateBuffer, outputParticleBufferHandle);
			}
			else
				EnqueueComputeParticleMapKernel(outputParticleBufferHandle->GetWriteBuffer(), nullptr, dynamicParticleCount, dynamicParticleWriteHashMapBuffer, particleMapBuffer, { &partialSumFinishedEvent(), 1 }, &computeParticleMapFinishedEvent());


#ifdef DEBUG_BUFFERS_GPU
			DebugHashes(outputParticleBufferHandle->GetWriteBuffer(), dynamicParticleWriteHashMapBuffer);
#endif			
			inputParticleBufferHandle.FinishRead({ &updateDynamicsFinishedEvent(), 1});
			outputParticleBufferHandle->FinishWrite({ &computeParticleMapFinishedEvent(), 1 }, i == simulationSteps - 1);
			this->particleBufferSet->Advance();


			std::swap(dynamicParticleReadHashMapBuffer, dynamicParticleWriteHashMapBuffer);

			reorderElapsedTime += deltaTime;

			if (profiling && detailedProfiling)
			{
				InsertMeasurement("updateParticlePressure", updatePressureFinishedEvent, updatePressureFinishedEvent);
				InsertMeasurement("updateParticleDynamics", updateDynamicsFinishedEvent, updateDynamicsFinishedEvent);
				InsertMeasurement("prefixSum", partialSumFinishedEvent, partialSumFinishedEvent);				
				InsertMeasurement("computeParticleMap", computeParticleMapFinishedEvent, computeParticleMapFinishedEvent);
			}

			if (i == 0)
				updateStartEvent = updatePressureFinishedEvent;
			if (i == simulationSteps - 1)
				updateEndEvent = computeParticleMapFinishedEvent;
		}

		if (profiling)
		{
			systemProfilingData.timePerStep_s = (double)Measure(updateStartEvent, updateEndEvent) / simulationSteps;
		}

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

		SPHProgram = BuildOpenCLProgram(clContext, Array<Path>{ "assets/kernels/CL_CPP_SPHDeclarations.h", "assets/kernels/CL_CPP_SPHFunctions.h", "assets/kernels/SPH.cl" }, { {"CL_COMPILER"} });

		computeParticleHashesKernel = cl::Kernel(SPHProgram, "computeParticleHashes", &ret);
		CL_CHECK();
		computeParticleMapKernel = cl::Kernel(SPHProgram, "computeParticleMap", &ret);
		CL_CHECK();
		updateParticlesPressureKernel = cl::Kernel(SPHProgram, "updateParticlesPressure", &ret);
		CL_CHECK();
		updateParticlesDynamicsKernel = cl::Kernel(SPHProgram, "updateParticlesDynamics", &ret);
		CL_CHECK();
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


			/*
				These statements have to hold

					(1) dynamicParticleHashMapSize = hashMapBufferGroupSize ^ layerCount
					(2) dynamicParticleHashMapSize > dynamicParticleHashMapSizeTarget
					(3) hashMapBufferGroupSize < hashMapGroupSizeCapacity

				layerCount should be as small as possible. layerCount, hashMapBufferGroupSize and dynamicParticleHashMapSize must be integers

					(4) (from 1) log_hashMapBufferGroupSize(dynamicParticleHashMapSize) = layerCount
					(5) (from 2 and 4) log_hashMapBufferGroupSize(dynamicParticleHashMapSizeTarget) < layerCount
					(6) (from 5 and 3) log_hashMapGroupSizeCapacity(dynamicParticleHashMapSizeTarget) < layerCount
					(7) (from 6) layerCount = ceil(log(dynamicParticleHashMapSizeTarget) / log(hashMapGroupSizeCapacity))
			*/
			uintMem hashMapGroupSizeCapacity = std::min(maxWorkGroupSize1 * 2, maxWorkGroupSize2);
			uintMem dynamicParticleHashMapSizeTarget = hashesPerDynamicParticle * dynamicParticleCount;

			uintMem layerCount = std::ceil(std::log(dynamicParticleHashMapSizeTarget) / std::log(hashMapGroupSizeCapacity));
			hashMapBufferGroupSize = 1Ui64 << (uintMem)std::ceil(std::log2(std::pow<float>(dynamicParticleHashMapSizeTarget, 1.0f / layerCount)));
			hashMapBufferGroupSize = std::min(hashMapBufferGroupSize, hashMapGroupSizeCapacity);
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
	}
	void SystemGPU::InitializeInternal(const SystemInitParameters& initParams)
	{
		initParams.ParseImplementationSpecifics((StringView)"OpenCL",
			SystemInitParameters::ImplementationSpecificsEntry{ "reorderTimeInterval", reorderTimeInterval },
			SystemInitParameters::ImplementationSpecificsEntry{ "detailedProfiling", detailedProfiling },
			SystemInitParameters::ImplementationSpecificsEntry{ "openCLChoosesGroupSize", openCLChoosesGroupSize },
			SystemInitParameters::ImplementationSpecificsEntry{ "useMaxGroupSize", useMaxGroupSize }
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

		if (openCLChoosesGroupSize)
		{
			computeParticleHashesKernelPreferredGroupSize = computeParticleHashesKernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(clContext.device, &ret);
			CL_CHECK()
				scanOnComputeGroupsKernelPreferredGroupSize = scanOnComputeGroupsKernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(clContext.device, &ret);
			CL_CHECK()
				addToComputeGroupArraysKernelPreferredGroupSize = addToComputeGroupArraysKernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(clContext.device, &ret);
			CL_CHECK()
				computeParticleMapKernelPreferredGroupSize = computeParticleMapKernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(clContext.device, &ret);
			CL_CHECK()
				updateParticlesPressureKernelPreferredGroupSize = updateParticlesPressureKernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(clContext.device, &ret);
			CL_CHECK()
				updateParticlesDynamicsKernelPreferredGroupSize = updateParticlesDynamicsKernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(clContext.device, &ret);
			CL_CHECK()
		}
		else
		{
			computeParticleHashesKernelPreferredGroupSize = computeParticleHashesKernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(clContext.device, &ret);
			CL_CHECK()
				scanOnComputeGroupsKernelPreferredGroupSize = scanOnComputeGroupsKernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(clContext.device, &ret);
			CL_CHECK()
				addToComputeGroupArraysKernelPreferredGroupSize = addToComputeGroupArraysKernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(clContext.device, &ret);
			CL_CHECK()
				computeParticleMapKernelPreferredGroupSize = computeParticleMapKernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(clContext.device, &ret);
			CL_CHECK()
				updateParticlesPressureKernelPreferredGroupSize = updateParticlesPressureKernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(clContext.device, &ret);
			CL_CHECK()
				updateParticlesDynamicsKernelPreferredGroupSize = updateParticlesDynamicsKernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(clContext.device, &ret);
			CL_CHECK()
		}

		{
			cl::Event clearHashMapFinishedEvents[4];

			uint32 pattern0 = 0;
			uint32 patternCount = dynamicParticleCount;
			CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleReadHashMapBuffer(), &pattern0, sizeof(uint32), 0, dynamicParticleHashMapSize * sizeof(uint32), 0, nullptr, &clearHashMapFinishedEvents[0]()));
			CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleReadHashMapBuffer(), &patternCount, sizeof(uint32), dynamicParticleHashMapSize * sizeof(uint32), sizeof(uint32), 0, nullptr, &clearHashMapFinishedEvents[1]()));
			CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleWriteHashMapBuffer(), &pattern0, sizeof(uint32), 0, dynamicParticleHashMapSize * sizeof(uint32), 0, nullptr, &clearHashMapFinishedEvents[2]()));
			CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleWriteHashMapBuffer(), &patternCount, sizeof(uint32), dynamicParticleHashMapSize * sizeof(uint32), sizeof(uint32), 0, nullptr, &clearHashMapFinishedEvents[3]()));

			auto& particlesWriteBufferHandle = this->particleBufferSet->GetWriteBufferHandle();
			cl::Event startWriteEvent;
			particlesWriteBufferHandle.StartWrite(&startWriteEvent());

			cl::Event computeParticleHashesFinishedEvent;
			uintMem computeParticleHashesWaitEventCount = 0;
			cl_event computeParticleHashesWaitEvents[5]{};
			if (clearHashMapFinishedEvents[0]() != nullptr) computeParticleHashesWaitEvents[computeParticleHashesWaitEventCount++] = clearHashMapFinishedEvents[0]();
			if (clearHashMapFinishedEvents[1]() != nullptr) computeParticleHashesWaitEvents[computeParticleHashesWaitEventCount++] = clearHashMapFinishedEvents[1]();
			if (clearHashMapFinishedEvents[2]() != nullptr) computeParticleHashesWaitEvents[computeParticleHashesWaitEventCount++] = clearHashMapFinishedEvents[2]();
			if (clearHashMapFinishedEvents[3]() != nullptr) computeParticleHashesWaitEvents[computeParticleHashesWaitEventCount++] = clearHashMapFinishedEvents[3]();
			if (computeParticleHashesFinishedEvent() != nullptr) computeParticleHashesWaitEvents[computeParticleHashesWaitEventCount++] = computeParticleHashesFinishedEvent();
			EnqueueComputeParticleHashesKernel(particlesWriteBufferHandle.GetWriteBuffer(), dynamicParticleCount, dynamicParticleReadHashMapBuffer, dynamicParticleHashMapSize, initParams.particleBehaviourParameters.maxInteractionDistance, { computeParticleHashesWaitEvents , computeParticleHashesWaitEventCount }, &computeParticleHashesFinishedEvent());			

#ifdef DEBUG_BUFFERS_GPU
			DebugParticles(particlesWriteBufferHandle.GetWriteBuffer());
			DebugPrePrefixSumHashes(particlesWriteBufferHandle.GetWriteBuffer(), dynamicParticleReadHashMapBuffer);
#endif

			cl::Event partialSumFinishedEvent;
			EnqueuePartialSumKernels(dynamicParticleReadHashMapBuffer, dynamicParticleHashMapSize, hashMapBufferGroupSize, { &computeParticleHashesFinishedEvent(), 1}, &partialSumFinishedEvent());

			cl::Event computeParticleMapFinishedEvent;
			EnqueueComputeParticleMapKernel(particlesWriteBufferHandle.GetWriteBuffer(), nullptr, dynamicParticleCount, dynamicParticleReadHashMapBuffer, particleMapBuffer, { &partialSumFinishedEvent() , 1}, &computeParticleMapFinishedEvent());

#ifdef DEBUG_BUFFERS_GPU				
			DebugHashes(particlesWriteBufferHandle.GetWriteBuffer(), dynamicParticleReadHashMapBuffer);
#endif		


			particlesWriteBufferHandle.FinishWrite({ &computeParticleMapFinishedEvent(), 1 }, true);
			this->particleBufferSet->Advance();			
		}
	}
	void SystemGPU::EnqueueComputeParticleHashesKernel(cl::Buffer& particles, uintMem dynamicParticleCount, cl::Buffer& dynamicParticleWriteHashMapBuffer, uintMem dynamicParticleHashMapSize, float maxInteractionDistance, ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{
		cl_int ret;

		CL_CALL(computeParticleHashesKernel.setArg(0, particles));
		CL_CALL(computeParticleHashesKernel.setArg(1, dynamicParticleWriteHashMapBuffer));
		CL_CALL(computeParticleHashesKernel.setArg(2, maxInteractionDistance));
		CL_CALL(computeParticleHashesKernel.setArg(3, (uint32)dynamicParticleHashMapSize));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = computeParticleHashesKernelPreferredGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(queue(), computeParticleHashesKernel(), 1, &globalWorkOffset, &globalWorkSize, openCLChoosesGroupSize ? nullptr : &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPU::EnqueueClearHashMap(ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{
		cl_int ret;
		byte* pattern = 0;
		CL_CALL(clEnqueueFillBuffer(queue(), dynamicParticleWriteHashMapBuffer(), &pattern, sizeof(byte), 0, sizeof(uint32) * dynamicParticleHashMapSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPU::EnqueueUpdateParticlesPressureKernel(const cl::Buffer& particleReadBuffer, cl::Buffer& particleWriteBuffer, ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
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
		CL_CALL(clEnqueueNDRangeKernel(queue(), updateParticlesPressureKernel(), 1, &globalWorkOffset, &globalWorkSize, openCLChoosesGroupSize ? nullptr : &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPU::EnqueueUpdateParticlesDynamicsKernel(const cl::Buffer& particleReadBuffer, cl::Buffer& particleWriteBuffer, float deltaTime, ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
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
		CL_CALL(clEnqueueNDRangeKernel(queue(), updateParticlesDynamicsKernel(), 1, &globalWorkOffset, &globalWorkSize, openCLChoosesGroupSize ? nullptr : &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPU::EnqueuePartialSumKernels(cl::Buffer& buffer, uintMem elementCount, uintMem groupSize, ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{
		cl_int ret;

		cl::Event waitEvent;
		cl::Event signalEvent;

		for (uintMem size = elementCount, layerI = 0; size != 1; size /= groupSize, ++layerI)
		{
			CL_CALL(scanOnComputeGroupsKernel.setArg(0, (groupSize + 1) * sizeof(uint32), nullptr));
			CL_CALL(scanOnComputeGroupsKernel.setArg(1, buffer));
			CL_CALL(scanOnComputeGroupsKernel.setArg(2, (uint32)(elementCount / size)));

			size_t globalWorkOffset = 0;
			size_t globalWorkSize = size / 2;
			size_t localWorkSize = groupSize / 2;
			CL_CALL(clEnqueueNDRangeKernel(queue(), scanOnComputeGroupsKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, size == elementCount ? waitEvents.Count() : 1, size == elementCount ? waitEvents.Ptr() : &waitEvent(), &signalEvent()));

			std::swap(signalEvent(), waitEvent());
			signalEvent = cl::Event();

#if defined (DEBUG_BUFFERS_GPU) && 0
			DebugInterPrefixSumHashes(nullptr, buffer, layerI + 1);
#endif
		}

#if defined (DEBUG_BUFFERS_GPU) && 0
		DebugInterPrefixSumHashes(nullptr, buffer, 0);
#endif

		for (uintMem topArraySize = groupSize; topArraySize != elementCount; topArraySize *= groupSize)
		{
			uintMem scale = elementCount / groupSize / topArraySize;

			CL_CALL(addToComputeGroupArraysKernel.setArg(0, buffer));
			CL_CALL(addToComputeGroupArraysKernel.setArg(1, (uint32)scale));

			size_t globalWorkOffset = 0;
			size_t globalWorkSize = (topArraySize - 1) * (groupSize - 1);
			size_t localWorkSize = groupSize - 1;
			CL_CALL(clEnqueueNDRangeKernel(queue(), addToComputeGroupArraysKernel(), 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 1, &waitEvent(), &signalEvent()));

			std::swap(signalEvent(), waitEvent());
			signalEvent = cl::Event();			

			//CL_CALL(queue.finish());
			//CL_CALL(queue.enqueueReadBuffer(buffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticleHashMapSize + 1), debugHashMapArray.Ptr(), nullptr, nullptr));
			//__debugbreak();
		}

		*finishedEvent = waitEvent();
		waitEvent() = nullptr;
	}
	void SystemGPU::EnqueueComputeParticleMapKernel(cl::Buffer& particles, cl::Buffer* orderedParticles, uintMem dynamicParticleCount, cl::Buffer& dynamicParticleWriteHashMapBuffer, cl::Buffer& particleMapBuffer, ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{
		cl_int ret;

		CL_CALL(computeParticleMapKernel.setArg(0, particles));
		if (orderedParticles == nullptr)
			CL_CALL(computeParticleMapKernel.setArg(1, nullptr))
		else
			CL_CALL(computeParticleMapKernel.setArg(1, *orderedParticles));
		CL_CALL(computeParticleMapKernel.setArg(2, dynamicParticleWriteHashMapBuffer));
		CL_CALL(computeParticleMapKernel.setArg(3, particleMapBuffer));
		CL_CALL(computeParticleMapKernel.setArg(4, (orderedParticles != nullptr ? 1 : 0)));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = computeParticleMapKernelPreferredGroupSize;;
		CL_CALL(clEnqueueNDRangeKernel(queue(), computeParticleMapKernel(), 1, &globalWorkOffset, &globalWorkSize, openCLChoosesGroupSize ? nullptr : &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
#ifdef DEBUG_BUFFERS_GPU
	void SystemGPU::DebugParticles(cl::Buffer& particles)
	{
		cl_int ret;

		CL_CALL(queue.finish());
		CL_CALL(queue.enqueueReadBuffer(particles, CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), nullptr, nullptr))

			System::DebugParticles<DynamicParticle>(debugParticlesArray, debugMaxInteractionDistance, dynamicParticleHashMapSize);
	}
	void SystemGPU::DebugPrePrefixSumHashes(cl::Buffer& particles, cl::Buffer& hashMapBuffer)
	{
		cl_int ret;

		CL_CALL(queue.finish());
		CL_CALL(queue.enqueueReadBuffer(hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticleHashMapSize + 1), debugHashMapArray.Ptr(), nullptr, nullptr));
		CL_CALL(queue.enqueueReadBuffer(particles, CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), nullptr, nullptr));

		System::DebugPrePrefixSumHashes<DynamicParticle>(debugParticlesArray, debugHashMapArray);
	}
	void SystemGPU::DebugInterPrefixSumHashes(cl::Buffer* particles, cl::Buffer& hashMapBuffer, uintMem layerCount)
	{
		cl_int ret;

		CL_CALL(queue.finish());
		CL_CALL(queue.enqueueReadBuffer(hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticleHashMapSize + 1), debugHashMapArray.Ptr(), nullptr, nullptr));

		if (particles != nullptr)
			CL_CALL(queue.enqueueReadBuffer(*particles, CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), nullptr, nullptr));

		System::DebugInterPrefixSumHashes(debugParticlesArray, debugHashMapArray, hashMapBufferGroupSize, layerCount);
	}
	void SystemGPU::DebugHashes(cl::Buffer& particles, cl::Buffer& hashMapBuffer)
	{
		cl_int ret;

		CL_CALL(queue.finish());
		CL_CALL(queue.enqueueReadBuffer(hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticleHashMapSize + 1), debugHashMapArray.Ptr(), nullptr, nullptr));
		CL_CALL(queue.enqueueReadBuffer(particleMapBuffer, CL_TRUE, 0, sizeof(uint32) * dynamicParticleCount, debugParticleMapArray.Ptr(), nullptr, nullptr));
		CL_CALL(queue.enqueueReadBuffer(particles, CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), nullptr, nullptr));

		System::DebugHashAndParticleMap<DynamicParticle, uint32>(debugParticlesArray, debugHashMapArray, debugParticleMapArray);
	}
#endif
}