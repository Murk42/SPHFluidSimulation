#include "pch.h"
#include "SPH/System/SystemGPU.h"
#include "SPH/Core/ParticleBufferManager.h"
#include "SPH/Kernels/Kernels.h"
#include "OpenCLDebug.h"

//#define DEBUG_BUFFERS_GPU

namespace SPH
{
	static String PadLeft(StringView s, uintMem count)
	{
		return String(count > s.Count() ? count - s.Count() : 0) + s;
	}
	static void PrintPerformanceProfile(ArrayView<Measurement> measurements, WriteStream& stream)
	{
		for (auto& measurement : measurements)
		{
			uint64 timeToSubmit = measurement.submitTime - measurement.queuedTime;
			uint64 timeToStart = measurement.startTime - measurement.submitTime;
			uint64 executionTime = measurement.endTime - measurement.startTime;
			String out = PadLeft(measurement.name, 40) + " - " + PadLeft(StringParsing::Convert(timeToSubmit), 9) + " " + PadLeft(StringParsing::Convert(timeToStart), 9) + " " + PadLeft(StringParsing::Convert(executionTime), 9) + "\n";

			stream.Write(out.Ptr(), out.Count());
		}

		stream.Write("\n", 1);
	}

	static cl_program BuildOpenCLProgram(cl_context clContext, cl_device_id clDevice, ArrayView<String> sources, const Map<String, String>& values)
	{
		cl_int ret = 0;
		
		Array<const char*> sourcePointers;
		Array<uintMem> sourceLengths;		

		for (uintMem i = 0; i < sources.Count(); ++i)
		{
			sourceLengths.AddBack(sources[i].Count());
			sourcePointers.AddBack(sources[i].Ptr());
		}

		auto program = clCreateProgramWithSource(clContext, sources.Count(), sourcePointers.Ptr(), sourceLengths.Ptr(), &ret);
		CL_CHECK(program);

		String options = "-cl-std=CL2.0 -cl-kernel-arg-info";

		for (auto& pair : values)
			if (pair.value.Empty())
				options += " -D " + pair.key;
			else
				options += " -D " + pair.key + "=" + pair.value;
		
		if ((ret = clBuildProgram(program, 1, &clDevice, options.Ptr(), nullptr, nullptr)) == CL_BUILD_PROGRAM_FAILURE)
		{
			uintMem logLength = 0;
			CL_CALL(clGetProgramBuildInfo(program, clDevice, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logLength), nullptr);
			String log{ logLength };
			CL_CALL(clGetProgramBuildInfo(program, clDevice, CL_PROGRAM_BUILD_LOG, logLength, log.Ptr(), nullptr), nullptr);						

			if (!log.Empty())
				Debug::Logger::LogDebug("Client", "Build log: \n" + log);
		}
		else
			CL_CHECK(program);

		return program;
	}
	
	SystemGPU::SystemGPU(cl_context clContext, cl_device_id clDevice, cl_command_queue clCommandQueue, Graphics::OpenGL::GraphicsContext_OpenGL& glContext) :
		clContext(clContext), clDevice(clDevice), clCommandQueue(clCommandQueue), glContext(glContext),
		dynamicParticleHashMapSize(0), staticParticleHashMapSize(0), hashMapBufferGroupSize(0),
		reorderElapsedTime(0.0f), reorderTimeInterval(FLT_MAX),
		simulationTime(0)
	{		
		if (!CheckForExtensions(clDevice, { "cl_khr_global_int32_base_atomics" }))
			Debug::Logger::LogError("Client", "Given OpenCL device doesn't support all needed extensions");

		if (Graphics::OpenGL::GraphicsContext_OpenGL::IsExtensionSupported("GL_ARB_cl_event"))
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension supported. The application implementation could be made better to use this extension. This is just informative");
		else
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension not supported. Even if it was supported it isn't implemented in the application. This is just informative");

		cl_command_queue_properties commandQueueProperties;
		clGetCommandQueueInfo(clCommandQueue, CL_QUEUE_PROPERTIES, sizeof(commandQueueProperties), &commandQueueProperties, nullptr);

		if (!bool(commandQueueProperties | CL_QUEUE_PROFILING_ENABLE))
			Debug::Logger::LogFatal("Library SPH", "A command queue suplied to a GPU system must have the property CL_QUEUE_PROFILING_ENABLE set");


		LoadKernels();

		File infoFile{ "OpenCLInfo.txt", FileAccessPermission::Write };

		PrintDeviceInfo(clDevice, infoFile);
		PrintKernelInfo(computeParticleHashesKernel, clDevice, infoFile);
		PrintKernelInfo(updateParticlesPressureKernel, clDevice, infoFile);
		PrintKernelInfo(updateParticlesDynamicsKernel, clDevice, infoFile);
		PrintKernelInfo(incrementHashMapKernel, clDevice, infoFile);
		PrintKernelInfo(scanOnComputeGroupsKernel, clDevice, infoFile);
		PrintKernelInfo(addToComputeGroupArraysKernel, clDevice, infoFile);
		PrintKernelInfo(computeParticleMapKernel, clDevice, infoFile);		
	}
	SystemGPU::~SystemGPU()
	{		
		clReleaseProgram(partialSumProgram);
		clReleaseProgram(SPHProgram);

		clReleaseKernel(computeParticleHashesKernel);
		clReleaseKernel(scanOnComputeGroupsKernel);
		clReleaseKernel(addToComputeGroupArraysKernel);
		clReleaseKernel(incrementHashMapKernel);
		clReleaseKernel(computeParticleMapKernel);
		clReleaseKernel(updateParticlesPressureKernel);
		clReleaseKernel(updateParticlesDynamicsKernel);

		Clear();
	}
	void SystemGPU::Clear()
	{		
		if (dynamicParticleHashMapBuffer != nullptr)
		{
			clReleaseMemObject(dynamicParticleHashMapBuffer);
			dynamicParticleHashMapBuffer = nullptr;
		}
		if (particleMapBuffer != nullptr)
		{
			clReleaseMemObject(particleMapBuffer);
			particleMapBuffer = nullptr;		
		}
		if (staticHashMapBuffer != nullptr)
		{
			clReleaseMemObject(staticHashMapBuffer);
			staticHashMapBuffer = nullptr;
		}
		if (particleBehaviourParametersBuffer != nullptr)
		{
			clReleaseMemObject(particleBehaviourParametersBuffer);
			particleBehaviourParametersBuffer = nullptr;
		}

		dynamicParticleHashMapSize = 0;
		staticParticleHashMapSize = 0;

		hashMapBufferGroupSize = 0;

		reorderElapsedTime = 0.0f;
		reorderTimeInterval = FLT_MAX;

		simulationTime = 0;
	}
	void SystemGPU::Initialize(const SystemParameters& parameters, ParticleBufferManager& particleBufferManager, Array<DynamicParticle> dynamicParticles, Array<StaticParticle> staticParticles)
	{
		Clear();
		particleBufferManager.Clear();
		this->particleBufferManager = &particleBufferManager;

		parameters.ParseParameter("reorderTimeInterval", reorderTimeInterval);
		
		ParticleBehaviourParameters particleBehaviourParameters = parameters.particleBehaviourParameters;
		particleBehaviourParameters.smoothingKernelConstant = SmoothingKernelConstant(particleBehaviourParameters.maxInteractionDistance);
		particleBehaviourParameters.selfDensity = particleBehaviourParameters.particleMass * SmoothingKernelD0(0, particleBehaviourParameters.maxInteractionDistance) * particleBehaviourParameters.smoothingKernelConstant;
		CL_CHECK_RET(particleBehaviourParametersBuffer = clCreateBuffer(clContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(ParticleBehaviourParameters), &particleBehaviourParameters, &ret));

		if (!staticParticles.Empty())
		{

			staticParticleHashMapSize = staticParticles.Count();

			Array<uint32> staticParticleHashMap(staticParticleHashMapSize + 1);

			staticParticles = GenerateHashMapAndReorderParticles(staticParticles, staticParticleHashMap, parameters.particleBehaviourParameters.maxInteractionDistance);

#ifdef DEBUG_BUFFERS_GPU
			System::DebugParticles<StaticParticle>(staticParticles, maxInteractionDistance, staticParticleHashMapSize);
			System::DebugHashAndParticleMap<StaticParticle, uint32>(staticParticles, staticParticleHashMap, {}, GetStaticParticleHash);
#endif

			if (staticParticleHashMapSize != 0)
				CL_CHECK_RET(staticHashMapBuffer = clCreateBuffer(clContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(uint32) * staticParticleHashMap.Count(), (void*)staticParticleHashMap.Ptr(), &ret))

				particleBufferManager.AllocateStaticParticles(staticParticles.Count(), staticParticles.Ptr());
		}
		
		if (!dynamicParticles.Empty())
		{
			DetermineHashGroupSize(clDevice, scanOnComputeGroupsKernel, addToComputeGroupArraysKernel, dynamicParticles.Count() * 2, hashMapBufferGroupSize, dynamicParticleHashMapSize);			

#ifdef DEBUG_BUFFERS_GPU
			debugMaxInteractionDistance = maxInteractionDistance;
			debugParticlesArray.Resize(dynamicParticleCount);
			debugHashMapArray.Resize(dynamicParticleHashMapSize + 1);
			debugParticleMapArray.Resize(dynamicParticleCount);
#endif		

			CL_CHECK_RET(dynamicParticleHashMapBuffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, sizeof(uint32) * (dynamicParticleHashMapSize + 1), nullptr, &ret));			
			CL_CHECK_RET(particleMapBuffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, sizeof(uint32) * dynamicParticles.Count(), nullptr, &ret));

			particleBufferManager.AllocateDynamicParticles(dynamicParticles.Count(), dynamicParticles.Ptr());			
			
			CalculateHashAndParticleMap(parameters.particleBehaviourParameters.maxInteractionDistance);
		}
	}
	void SystemGPU::Update(float deltaTime, uint simulationStepCount)
	{
		if (this->particleBufferManager == nullptr)
		{
			Debug::Logger::LogWarning("Client", "Updating a uninitialized SPHSystem");
			return;
		}

		if (particleBufferManager->GetDynamicParticleCount() == 0)
			return; 

		Array<PerformanceProfile<20>> performanceProfiles{ simulationStepCount };
		
		cl::Event staticParticlesReadStartEvent;
		auto staticParticlesLockGuard = particleBufferManager->LockStaticParticlesForRead(&staticParticlesReadStartEvent());
		auto staticParticles = (cl_mem)staticParticlesLockGuard.GetResource();
		
		cl::Event updateEndEvent;		

		for (uint i = 0; i < simulationStepCount; ++i)
		{			
			cl::Event readLockAcquiredEvent;
			auto inputParticlesLockGuard = particleBufferManager->LockDynamicParticlesForRead(&readLockAcquiredEvent);
			cl_mem inputParticles = (cl_mem)inputParticlesLockGuard.GetResource();

			particleBufferManager->Advance();

			cl::Event writeLockAcquiredEvent;
			auto outputParticlesLockGuard = particleBufferManager->LockDynamicParticlesForWrite(&writeLockAcquiredEvent);
			cl_mem outputParticles = (cl_mem)outputParticlesLockGuard.GetResource();


#ifdef DEBUG_BUFFERS_GPU			
			DebugParticles(inputParticles);
#endif										

			cl::Event updatePressureFinishedEvent;
 			EnqueueUpdateParticlesPressureKernel(inputParticles, outputParticles, staticParticles, EventWaitArray<3>{ readLockAcquiredEvent, writeLockAcquiredEvent, staticParticlesReadStartEvent  }, &updatePressureFinishedEvent());
			staticParticlesReadStartEvent = cl::Event();
			readLockAcquiredEvent = cl::Event();
			writeLockAcquiredEvent = cl::Event();

			performanceProfiles[i].AddPendingMeasurement("Update particle pressure kernel", updatePressureFinishedEvent());

			cl::Event updateDynamicsFinishedEvent;
			EnqueueUpdateParticlesDynamicsKernel(inputParticles, outputParticles, staticParticles, deltaTime, { &updatePressureFinishedEvent(), 1 }, &updateDynamicsFinishedEvent());
			updatePressureFinishedEvent = cl::Event();

			performanceProfiles[i].AddPendingMeasurement("Update particle dynamics kernel", updateDynamicsFinishedEvent());

			inputParticlesLockGuard.Unlock({ (void**)&updateDynamicsFinishedEvent(), 1 });

#ifdef DEBUG_BUFFERS_GPU
			DebugParticles(outputParticles);
#endif

			cl::Event clearHashMapFinishedEvent;
			EnqueueClearHashMapCommand({ &updateDynamicsFinishedEvent(), 1 }, &clearHashMapFinishedEvent());
			updateDynamicsFinishedEvent = cl::Event();

			performanceProfiles[i].AddPendingMeasurement("Clear hash map", clearHashMapFinishedEvent());

			cl::Event incrementHashMapEventFinished;
			EnqueueIncrementHashMapKernel(outputParticles, { &clearHashMapFinishedEvent(), 1 }, &incrementHashMapEventFinished());
			clearHashMapFinishedEvent = cl::Event();

			performanceProfiles[i].AddPendingMeasurement("Increment hash map kernel", incrementHashMapEventFinished());

#ifdef DEBUG_BUFFERS_GPU
			DebugParticles(outputParticles);
			DebugPrePrefixSumHashes(outputParticles, dynamicParticleHashMapBuffer);
#endif

			cl::Event partialSumFinishedEvent;
			EnqueuePartialSumKernels({ &incrementHashMapEventFinished(), 1 }, &partialSumFinishedEvent());
			incrementHashMapEventFinished = cl::Event();			

			cl::Event computeParticleMapFinishedEvent;
			if (reorderElapsedTime > reorderTimeInterval)
			{
				reorderElapsedTime -= reorderTimeInterval;

				particleBufferManager->Advance();

				cl::Event startIntermediateEvent;
				auto intermediateParticlesLockGuard = particleBufferManager->LockDynamicParticlesForWrite(&startIntermediateEvent());
				auto intermediateParticles = (cl_mem)intermediateParticlesLockGuard.GetResource();
				
				uintMem computeParticleMapWaitEventCount = 0;
				cl_event computeParticleMapWaitEvents[2]{ };
				if (startIntermediateEvent() != nullptr) computeParticleMapWaitEvents[computeParticleMapWaitEventCount++] = startIntermediateEvent();
				if (partialSumFinishedEvent() != nullptr) computeParticleMapWaitEvents[computeParticleMapWaitEventCount++] = partialSumFinishedEvent();
				EnqueueComputeParticleMapKernel(outputParticles, &intermediateParticles, { computeParticleMapWaitEvents, computeParticleMapWaitEventCount }, &computeParticleMapFinishedEvent());
				startIntermediateEvent = cl::Event();
				partialSumFinishedEvent = cl::Event();


				outputParticlesLockGuard.Unlock({ (void**)&computeParticleMapFinishedEvent(), 1 });				

				std::swap(intermediateParticles, outputParticles);
				std::swap(intermediateParticlesLockGuard, outputParticlesLockGuard);
			}
			else
			{
				EnqueueComputeParticleMapKernel(outputParticles, nullptr, { &partialSumFinishedEvent(), 1 }, &computeParticleMapFinishedEvent());
				partialSumFinishedEvent = cl::Event();
			}

			performanceProfiles[i].AddPendingMeasurement("Compute particle map kernel", computeParticleMapFinishedEvent());

#ifdef DEBUG_BUFFERS_GPU
			DebugHashes(outputParticles, dynamicParticleHashMapBuffer);
#endif						
			outputParticlesLockGuard.Unlock({ (void**) & computeParticleMapFinishedEvent(), 1});			

			reorderElapsedTime += deltaTime;
							
			if (i == simulationStepCount - 1)
				updateEndEvent = std::move(computeParticleMapFinishedEvent);
			else
				computeParticleMapFinishedEvent = cl::Event();
		}

		//TODO make the unlock acquire the event
		staticParticlesLockGuard.Unlock({ (void**)&updateEndEvent, 1 });
		updateEndEvent = cl::Event();

		simulationTime += deltaTime * simulationStepCount;


		if (Keyboard::GetFrameKeyState(Keyboard::Key::P).pressed)
		{
			ConsoleOutputStream stream;
			for (auto& profile : performanceProfiles)
				PrintPerformanceProfile(profile.GetMeasurements(), stream);
		}
	}
	void SystemGPU::LoadKernels()
	{
		partialSumProgram = BuildOpenCLProgram(clContext, clDevice, { 
			String(partialSumKernelSourceBytes, partialSumKernelSourceSize) 
			}, {});

		CL_CHECK_RET(scanOnComputeGroupsKernel     = clCreateKernel(partialSumProgram, "scanOnComputeGroups", &ret));
		CL_CHECK_RET(addToComputeGroupArraysKernel = clCreateKernel(partialSumProgram, "addToComputeGroupArrays", &ret));

		SPHProgram = BuildOpenCLProgram(clContext, clDevice, { 
			String(compatibilityHeaderOpenCLBytes, compatibiliyHeaderOpenCLSize),
			String(SPHKernelSourceBytes, SPHKernelSourceSize),			
			}, { {"CL_COMPILER"} });

		CL_CHECK_RET(computeParticleHashesKernel   = clCreateKernel(SPHProgram, "computeParticleHashes", &ret));
		CL_CHECK_RET(computeParticleMapKernel      = clCreateKernel(SPHProgram, "ComputeParticleMap", &ret));
		CL_CHECK_RET(updateParticlesPressureKernel = clCreateKernel(SPHProgram, "UpdateParticlePressure", &ret));
		CL_CHECK_RET(updateParticlesDynamicsKernel = clCreateKernel(SPHProgram, "UpdateParticleDynamics", &ret));
		CL_CHECK_RET(incrementHashMapKernel         = clCreateKernel(SPHProgram, "incrementHashMap", &ret));

		CL_CALL(clGetKernelWorkGroupInfo(computeParticleHashesKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &computeParticleHashesKernelPreferredGroupSize, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(updateParticlesPressureKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &updateParticlesPressureKernelPreferredGroupSize, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(updateParticlesDynamicsKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &updateParticlesDynamicsKernelPreferredGroupSize, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(incrementHashMapKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &incrementHashMapKernelPreferredGroupSize, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(scanOnComputeGroupsKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &scanOnComputeGroupsKernelPreferredGroupSize, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(addToComputeGroupArraysKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &addToComputeGroupArraysKernelPreferredGroupSize, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(computeParticleMapKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &computeParticleMapKernelPreferredGroupSize, nullptr));		
	}
	void SystemGPU::DetermineHashGroupSize(cl_device_id clDevice, cl_kernel scanGroupKernel, cl_kernel addGroupKernel, uintMem targetHashMapSize, uintMem& hashMapBufferGroupSize, uintMem& hashMapSize)
	{
		uintMem maxWorkGroupSize1 = 0;
		uintMem preferredWorkGroupSize1 = 0;
		uintMem maxWorkGroupSize2 = 0;
		uintMem preferredWorkGroupSize2 = 0;
		CL_CALL(clGetKernelWorkGroupInfo(scanGroupKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &maxWorkGroupSize1, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(scanGroupKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &preferredWorkGroupSize1, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(addGroupKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &maxWorkGroupSize2, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(addGroupKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &preferredWorkGroupSize2, nullptr));

		/*
			These statements have to hold

				(1) hashMapSize = hashMapBufferGroupSize ^ layerCount
				(2) hashMapSize > targetHashMapSize
				(3) hashMapBufferGroupSize < hashMapGroupSizeCapacity

			layerCount should be as small as possible. layerCount, hashMapBufferGroupSize and hashMapSize must be integers

				(4) (from 1) log_hashMapBufferGroupSize(hashMapSize) = layerCount
				(5) (from 2 and 4) log_hashMapBufferGroupSize(targetHashMapSize) < layerCount
				(6) (from 5 and 3) log_hashMapGroupSizeCapacity(targetHashMapSize) < layerCount
				(7) (from 6) layerCount = ceil(log(targetHashMapSize) / log(hashMapGroupSizeCapacity))
		*/
		uintMem hashMapGroupSizeCapacity = std::min(maxWorkGroupSize1 * 2, maxWorkGroupSize2);		
		
		uintMem layerCount = std::ceil(std::log(targetHashMapSize) / std::log(hashMapGroupSizeCapacity));
		hashMapBufferGroupSize = 1Ui64 << (uintMem)std::ceil(std::log2(std::pow<float>(targetHashMapSize, 1.0f / layerCount)));
		hashMapBufferGroupSize = std::min(hashMapBufferGroupSize, hashMapGroupSizeCapacity);
		hashMapSize = std::pow(hashMapBufferGroupSize, layerCount);
	}		
	void SystemGPU::CalculateHashAndParticleMap(float maxInteractionDistance)
	{
		EventWaitArray<3> computeParticleHashesWaitEvents;

		uint32 pattern0 = 0;
		uint32 patternCount = particleBufferManager->GetDynamicParticleCount();
		CL_CALL(clEnqueueFillBuffer(clCommandQueue, dynamicParticleHashMapBuffer, &pattern0, sizeof(uint32), 0, dynamicParticleHashMapSize * sizeof(uint32), 0, nullptr, computeParticleHashesWaitEvents));
		CL_CALL(clEnqueueFillBuffer(clCommandQueue, dynamicParticleHashMapBuffer, &patternCount, sizeof(uint32), dynamicParticleHashMapSize * sizeof(uint32), sizeof(uint32), 0, nullptr, computeParticleHashesWaitEvents));

		auto particlesLockGuard = particleBufferManager->LockDynamicParticlesForWrite(computeParticleHashesWaitEvents);
		auto particles = (cl_mem)particlesLockGuard.GetResource();

		cl_event computeParticleHashesFinishedEvent;
		EnqueueComputeParticleHashesKernel(particles, maxInteractionDistance, computeParticleHashesWaitEvents, &computeParticleHashesFinishedEvent);
		computeParticleHashesWaitEvents.Release();

#ifdef DEBUG_BUFFERS_GPU
		DebugParticles(particles);
		DebugPrePrefixSumHashes(particles, dynamicParticleHashMapBuffer);
#endif

		cl_event partialSumFinishedEvent;
		EnqueuePartialSumKernels({ &computeParticleHashesFinishedEvent, 1 }, &partialSumFinishedEvent);
		clReleaseEvent(computeParticleHashesFinishedEvent);

		cl_event computeParticleMapFinishedEvent;
		EnqueueComputeParticleMapKernel(particles, nullptr, { &partialSumFinishedEvent , 1 }, &computeParticleMapFinishedEvent);
		clReleaseEvent(partialSumFinishedEvent);

#ifdef DEBUG_BUFFERS_GPU				
		DebugHashes(particles, dynamicParticleHashMapBuffer);
#endif		

		particlesLockGuard.Unlock({ (void**)&computeParticleMapFinishedEvent, 1 });
	}
	void SystemGPU::EnqueueComputeParticleHashesKernel(cl_mem particles, float maxInteractionDistance, ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{
		cl_int ret = 0;		
		
		CL_CALL(clSetKernelArg(computeParticleHashesKernel, 0, sizeof(cl_mem), &particles));
		CL_CALL(clSetKernelArg(computeParticleHashesKernel, 1, sizeof(cl_mem), &dynamicParticleHashMapBuffer));
		CL_CALL(clSetKernelArg(computeParticleHashesKernel, 2, sizeof(float), &maxInteractionDistance));
		CL_CALL(clSetKernelArg(computeParticleHashesKernel, 3, sizeof(uint64), &dynamicParticleHashMapSize));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = particleBufferManager->GetDynamicParticleCount();
		size_t localWorkSize = computeParticleHashesKernelPreferredGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, computeParticleHashesKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPU::EnqueueClearHashMapCommand(ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{		
		byte* pattern = 0;
		CL_CALL(clEnqueueFillBuffer(clCommandQueue, dynamicParticleHashMapBuffer, &pattern, sizeof(byte), 0, sizeof(uint32) * dynamicParticleHashMapSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPU::EnqueueUpdateParticlesPressureKernel(cl_mem particleReadBuffer, cl_mem particleWriteBuffer, cl_mem staticParticlesBuffer, ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{
		const uint64 dynamicParticleCount = particleBufferManager->GetDynamicParticleCount();
		const uint64 staticParticleCount = particleBufferManager->GetStaticParticleCount();
		const uint64 threadIDNull = 0;

		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 0, sizeof(uint64), &threadIDNull));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 1, sizeof(uint64), &dynamicParticleCount));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 2, sizeof(uint64), &dynamicParticleHashMapSize));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 3, sizeof(uint64), &staticParticleCount));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 4, sizeof(uint64), &staticParticleHashMapSize));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 5, sizeof(cl_mem), &particleReadBuffer));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 6, sizeof(cl_mem), &particleWriteBuffer));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 7, sizeof(cl_mem), &dynamicParticleHashMapBuffer));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 8, sizeof(cl_mem), &particleMapBuffer));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 9, sizeof(cl_mem), &staticParticlesBuffer));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 10, sizeof(cl_mem), &staticHashMapBuffer));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 11, sizeof(cl_mem), &particleBehaviourParametersBuffer));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = updateParticlesPressureKernelPreferredGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, updateParticlesPressureKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPU::EnqueueUpdateParticlesDynamicsKernel(cl_mem particleReadBuffer, cl_mem particleWriteBuffer, cl_mem staticParticlesBuffer, float deltaTime, ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{
		const uint64 dynamicParticleCount = particleBufferManager->GetDynamicParticleCount();
		const uint64 staticParticleCount = particleBufferManager->GetStaticParticleCount();
		const uint64 threadIDNull = 0;

		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 0, sizeof(uint64), &threadIDNull));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 1, sizeof(uint64), &dynamicParticleCount));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 2, sizeof(uint64), &dynamicParticleHashMapSize));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 3, sizeof(uint64), &staticParticleCount));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 4, sizeof(uint64), &staticParticleHashMapSize));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 5, sizeof(cl_mem), &particleReadBuffer));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 6, sizeof(cl_mem), &particleWriteBuffer));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 7, sizeof(cl_mem), &dynamicParticleHashMapBuffer));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 8, sizeof(cl_mem), &particleMapBuffer));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 9, sizeof(cl_mem), &staticParticlesBuffer));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 10, sizeof(cl_mem), &staticHashMapBuffer));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 11, sizeof(float), &deltaTime));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 12, sizeof(cl_mem), &particleBehaviourParametersBuffer));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = updateParticlesDynamicsKernelPreferredGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, updateParticlesDynamicsKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPU::EnqueueIncrementHashMapKernel(cl_mem particleBuffer, ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{
		cl_int ret = 0;

		CL_CALL(clSetKernelArg(incrementHashMapKernel, 0, sizeof(cl_mem), &particleBuffer));
		CL_CALL(clSetKernelArg(incrementHashMapKernel, 1, sizeof(cl_mem), &dynamicParticleHashMapBuffer));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = particleBufferManager->GetDynamicParticleCount();
		size_t localWorkSize = incrementHashMapKernelPreferredGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, incrementHashMapKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPU::EnqueuePartialSumKernels(ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{
		cl_int ret = 0;

		cl::Event waitEvent;
		cl::Event signalEvent;

		for (uintMem size = dynamicParticleHashMapSize, layerI = 0; size != 1; size /= hashMapBufferGroupSize, ++layerI)
		{
			uintMem tempMemorySize = (hashMapBufferGroupSize + 1) * sizeof(uint32);
			uint32 scale = (uint32)(dynamicParticleHashMapSize / size);
			CL_CALL(clSetKernelArg(scanOnComputeGroupsKernel, 0, tempMemorySize, nullptr));
			CL_CALL(clSetKernelArg(scanOnComputeGroupsKernel, 1, sizeof(cl_mem), &dynamicParticleHashMapBuffer));
			CL_CALL(clSetKernelArg(scanOnComputeGroupsKernel, 2, sizeof(uint32), &scale));

			size_t globalWorkOffset = 0;
			size_t globalWorkSize = size / 2;
			size_t localWorkSize = hashMapBufferGroupSize / 2;
			CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, scanOnComputeGroupsKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, size == dynamicParticleHashMapSize ? waitEvents.Count() : 1, size == dynamicParticleHashMapSize ? waitEvents.Ptr() : &waitEvent(), &signalEvent()));

			std::swap(signalEvent(), waitEvent());
			signalEvent = cl::Event();

#if defined (DEBUG_BUFFERS_GPU) && 0
			DebugInterPrefixSumHashes(nullptr, buffer, layerI + 1);
#endif
		}

#if defined (DEBUG_BUFFERS_GPU) && 0
		DebugInterPrefixSumHashes(nullptr, buffer, 0);
#endif

		for (uintMem topArraySize = hashMapBufferGroupSize; topArraySize != dynamicParticleHashMapSize; topArraySize *= hashMapBufferGroupSize)
		{
			uint32 scale = dynamicParticleHashMapSize / hashMapBufferGroupSize / topArraySize;

			CL_CALL(clSetKernelArg(addToComputeGroupArraysKernel, 0, sizeof(uintMem), &dynamicParticleHashMapBuffer));
			CL_CALL(clSetKernelArg(addToComputeGroupArraysKernel, 1, sizeof(scale), &scale));

			size_t globalWorkOffset = 0;
			size_t globalWorkSize = (topArraySize - 1) * (hashMapBufferGroupSize - 1);
			size_t localWorkSize = hashMapBufferGroupSize - 1;
			CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, addToComputeGroupArraysKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 1, &waitEvent(), &signalEvent()));

			std::swap(signalEvent(), waitEvent());
			signalEvent = cl::Event();			

			//CL_CALL(queue.finish());
			//CL_CALL(queue.enqueueReadBuffer(buffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticleHashMapSize + 1), debugHashMapArray.Ptr(), nullptr, nullptr));
			//__debugbreak();
		}

		*finishedEvent = waitEvent();
		waitEvent() = nullptr;
	}
	void SystemGPU::EnqueueComputeParticleMapKernel(cl_mem particles, const cl_mem* orderedParticles, ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{
		cl_int ret = 0;

		uint32 reorderParticles = (orderedParticles != nullptr ? 1 : 0);

		uint64 threadIDNull = 0;
		CL_CALL(clSetKernelArg(computeParticleMapKernel, 0, sizeof(uint64), &threadIDNull));
		CL_CALL(clSetKernelArg(computeParticleMapKernel, 1, sizeof(cl_mem), &particles));		
		CL_CALL(clSetKernelArg(computeParticleMapKernel, 2, sizeof(cl_mem), orderedParticles));
		CL_CALL(clSetKernelArg(computeParticleMapKernel, 3, sizeof(cl_mem), &dynamicParticleHashMapBuffer));
		CL_CALL(clSetKernelArg(computeParticleMapKernel, 4, sizeof(cl_mem), &particleMapBuffer));
		CL_CALL(clSetKernelArg(computeParticleMapKernel, 5, sizeof(uint32), &reorderParticles));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = particleBufferManager->GetDynamicParticleCount();
		size_t localWorkSize = computeParticleMapKernelPreferredGroupSize;;
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, computeParticleMapKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
#ifdef DEBUG_BUFFERS_GPU
	void SystemGPU::DebugParticles(cl_mem particles)
	{
		cl_int ret = 0;

		CL_CALL(clFinish(clCommandQueue));
		
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, particles, CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), 0, nullptr, nullptr))

		System::DebugParticles<DynamicParticle>(debugParticlesArray, debugMaxInteractionDistance, dynamicParticleHashMapSize);
	}
	void SystemGPU::DebugPrePrefixSumHashes(cl_mem particles, cl_mem hashMapBuffer)
	{
		cl_int ret = 0;

		CL_CALL(clFinish(clCommandQueue));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticleHashMapSize + 1), debugHashMapArray.Ptr(), 0, nullptr, nullptr));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, particles, CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), 0, nullptr, nullptr));

		System::DebugPrePrefixSumHashes<DynamicParticle>(debugParticlesArray, debugHashMapArray);
	}
	void SystemGPU::DebugInterPrefixSumHashes(cl_mem* particles, cl_mem hashMapBuffer, uintMem layerCount)
	{
		cl_int ret = 0;

		CL_CALL(clFinish(clCommandQueue));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticleHashMapSize + 1), debugHashMapArray.Ptr(), 0, nullptr, nullptr));

		if (particles != nullptr)
			CL_CALL(clEnqueueReadBuffer(clCommandQueue, *particles, CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), 0, nullptr, nullptr));

		System::DebugInterPrefixSumHashes(debugParticlesArray, debugHashMapArray, hashMapBufferGroupSize, layerCount);
	}
	void SystemGPU::DebugHashes(cl_mem particles, cl_mem hashMapBuffer)
	{
		cl_int ret = 0;

		CL_CALL(clFinish(clCommandQueue));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticleHashMapSize + 1), debugHashMapArray.Ptr(), 0, nullptr, nullptr));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, particleMapBuffer, CL_TRUE, 0, sizeof(uint32) * dynamicParticleCount, debugParticleMapArray.Ptr(), 0, nullptr, nullptr));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, particles, CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), 0, nullptr, nullptr));

		System::DebugHashAndParticleMap<DynamicParticle, uint32>(debugParticlesArray, debugHashMapArray, debugParticleMapArray);
	}
#else
	void SystemGPU::DebugParticles(cl_mem particles) { }
	void SystemGPU::DebugPrePrefixSumHashes(cl_mem particles, cl_mem hashMapBuffer) { }
	void SystemGPU::DebugInterPrefixSumHashes(cl_mem* particles, cl_mem hashMapBuffer, uintMem layerCount) { }
	void SystemGPU::DebugHashes(cl_mem particles, cl_mem hashMapBuffer) { }
#endif
}