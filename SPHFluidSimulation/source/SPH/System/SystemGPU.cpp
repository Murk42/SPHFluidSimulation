 #include "pch.h"
#include "SPH/System/SystemGPU.h"
#include "gl/glew.h"
#include "GL/glext.h"
#include "SPH/ParticleBufferSet/GPUParticleBufferSet.h"
#include "SPH/kernels/kernels.h"
#include "OpenCLDebug.h"

#define DEBUG_BUFFERS_GPU

namespace SPH
{
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

		String options;
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
	
	SystemGPU::SystemGPU(cl_context clContext, cl_device_id clDevice, cl_command_queue queue, Graphics::OpenGL::GraphicsContext_OpenGL& glContext) :
		clContext(clContext), clDevice(clDevice), queue(queue), glContext(glContext)		
	{				
		if (!CheckForExtensions(clDevice, GetRequiredOpenCLExtensions()))
			Debug::Logger::LogError("Client", "Given OpenCL device doesn't support all needed extensions");

		if (Graphics::OpenGL::GraphicsContext_OpenGL::IsExtensionSupported("GL_ARB_cl_event"))
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension supported. The application implementation could be made better to use this extension. This is just informative");
		else
			Debug::Logger::LogInfo("Client", "GL_ARB_cl_event extension not supported. Even if it was supported it isn't implemented in the application. This is just informative");

		cl_int ret = 0;
		CL_CALL(clGetDeviceInfo(clDevice, CL_DEVICE_PREFERRED_INTEROP_USER_SYNC, sizeof(cl_bool), &userOpenCLOpenGLSync, nullptr))

		LoadKernels();

		clGetDeviceInfo(clDevice, CL_DEVICE_NON_UNIFORM_WORK_GROUP_SUPPORT, sizeof(bool), &nonUniformWorkGroupSizeSupported, nullptr);		
	}
	SystemGPU::~SystemGPU()
	{		
		clReleaseProgram(partialSumProgram);
		clReleaseProgram(SPHProgram);

		clReleaseKernel(computeParticleHashesKernel);
		clReleaseKernel(scanOnComputeGroupsKernel);
		clReleaseKernel(addToComputeGroupArraysKernel);
		clReleaseKernel(increaseHashMapKernel);
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
		if (simulationParametersBuffer != nullptr)
		{
			clReleaseMemObject(simulationParametersBuffer);
			simulationParametersBuffer = nullptr;
		}

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

		if (dynamicParticleCount == 0)
			return; 

		auto Measure = [](cl::Event& start, cl::Event& end) -> double {

			start.wait();
			end.wait();
			cl_int ret = 0;

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

		cl_int ret = 0;

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

				cl::Event updatePressureFinishedEvent;
				uintMem updateParticlesWaitEventCount = 0;
				cl_event updateParticlesWaitEvents[2]{ };
				if (readStartEvent() != nullptr) updateParticlesWaitEvents[updateParticlesWaitEventCount++] = readStartEvent();
				if (writeStartEvent() != nullptr) updateParticlesWaitEvents[updateParticlesWaitEventCount++] = writeStartEvent();
				EnqueueUpdateParticlesPressureKernel(inputParticleBufferHandle.GetReadBuffer(), outputParticleBufferHandle->GetWriteBuffer(), { updateParticlesWaitEvents, updateParticlesWaitEventCount }, &updatePressureFinishedEvent());

				cl::Event updateDynamicsFinishedEvent;
				EnqueueUpdateParticlesDynamicsKernel(inputParticleBufferHandle.GetReadBuffer(), outputParticleBufferHandle->GetWriteBuffer(), deltaTime, { &updatePressureFinishedEvent(), 1 }, &updateDynamicsFinishedEvent());

				cl::Event clearHashMapFinishedEvent;
				EnqueueClearHashMap({ &updateDynamicsFinishedEvent(), 1 }, &clearHashMapFinishedEvent());

				cl::Event increaseHashMapEventFinished;
				EnqueueIncreaseHashMap(outputParticleBufferHandle->GetWriteBuffer(), { &clearHashMapFinishedEvent(), 1 }, &increaseHashMapEventFinished());

#ifdef DEBUG_BUFFERS_GPU
				DebugParticles(outputParticleBufferHandle->GetWriteBuffer());
				DebugPrePrefixSumHashes(outputParticleBufferHandle->GetWriteBuffer(), dynamicParticleHashMapBuffer);
#endif

				cl::Event partialSumFinishedEvent;
				EnqueuePartialSumKernels({ &increaseHashMapEventFinished(), 1 }, &partialSumFinishedEvent());

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
					EnqueueComputeParticleMapKernel(outputParticleBufferHandle->GetWriteBuffer(), &intermediateBuffer->GetWriteBuffer(), { computeParticleMapWaitEvents, computeParticleMapWaitEventCount }, &computeParticleMapFinishedEvent());

					outputParticleBufferHandle->FinishWrite({ &computeParticleMapFinishedEvent(), 1 }, false);

					std::swap(intermediateBuffer, outputParticleBufferHandle);
				}
				else
					EnqueueComputeParticleMapKernel(outputParticleBufferHandle->GetWriteBuffer(), nullptr, { &partialSumFinishedEvent(), 1 }, &computeParticleMapFinishedEvent());

#ifdef DEBUG_BUFFERS_GPU
				DebugHashes(outputParticleBufferHandle->GetWriteBuffer(), dynamicParticleHashMapBuffer);
#endif			
				inputParticleBufferHandle.FinishRead({ &updateDynamicsFinishedEvent(), 1 });
				outputParticleBufferHandle->FinishWrite({ &computeParticleMapFinishedEvent(), 1 }, i == simulationSteps - 1);
				this->particleBufferSet->Advance();

				reorderElapsedTime += deltaTime;

				if (detailedProfiling)
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

		if (profiling && !detailedProfiling)
		{
			systemProfilingData.timePerStep_s = (double)Measure(updateStartEvent, updateEndEvent) / simulationSteps;
		}

		simulationTime += deltaTime * simulationSteps;
	}
	Set<String> SystemGPU::GetRequiredOpenCLExtensions()
	{
		return Set<String>(); Set<String> requiredOpenCLExtensions{
			"cl_khr_global_int32_base_atomics",
		};
	}
	void SystemGPU::LoadKernels()
	{
		cl_int ret = 0;
		
		partialSumProgram = BuildOpenCLProgram(clContext, clDevice, { String((const char*)partialSumKernelSource, partialSumKernelSourceSize) }, {});

		scanOnComputeGroupsKernel = clCreateKernel(partialSumProgram, "scanOnComputeGroups", &ret);
		CL_CHECK();
		addToComputeGroupArraysKernel = clCreateKernel(partialSumProgram, "addToComputeGroupArrays", &ret);
		CL_CHECK();

		SPHProgram = BuildOpenCLProgram(clContext, clDevice, { 
			String((const char*)CL_CPP_SPHDeclarationsSource, CL_CPP_SPHDeclarationsSourceSize),
			String((const char*)CL_CPP_SPHFuntionsSource, CL_CPP_SPHFuntionsSourceSize),
			String((const char*)SPHKernelSource, SPHKernelSourceSize),
		}, { {"CL_COMPILER"} });

		computeParticleHashesKernel = clCreateKernel(SPHProgram, "computeParticleHashes", &ret);
		CL_CHECK();
		computeParticleMapKernel = clCreateKernel(SPHProgram, "computeParticleMap", &ret);
		CL_CHECK();
		updateParticlesPressureKernel = clCreateKernel(SPHProgram, "updateParticlesPressure", &ret);
		CL_CHECK();
		updateParticlesDynamicsKernel = clCreateKernel(SPHProgram, "updateParticlesDynamics", &ret);
		CL_CHECK();
		increaseHashMapKernel = clCreateKernel(SPHProgram, "increaseHashMap", &ret);
		CL_CHECK();
	}
	void SystemGPU::CreateStaticParticlesBuffers(Array<StaticParticle>& staticParticles, float maxInteractionDistance)
	{
		cl_int ret = 0;

		staticParticleCount = staticParticles.Count();
		staticParticleHashMapSize = 1 * staticParticleCount;

		Array<uint32> staticParticleHashMap;
		staticParticleHashMap.Resize(staticParticleHashMapSize + 1);

		auto GetStaticParticleHash = [maxInteractionDistance = maxInteractionDistance, mod = staticParticleHashMapSize](const StaticParticle& particle) {
			return GetHash(GetCell(particle.position, maxInteractionDistance)) % mod;
			};

		staticParticles = GenerateHashMapAndReorderParticles<StaticParticle>(staticParticles, staticParticleHashMap, GetStaticParticleHash);
		particleBufferSet->SetStaticParticles(staticParticles);

#ifdef DEBUG_BUFFERS_GPU
		System::DebugParticles<StaticParticle>(staticParticles, maxInteractionDistance, staticParticleHashMapSize);
		System::DebugHashAndParticleMap<StaticParticle, uint32>(staticParticles, staticParticleHashMap, {}, GetStaticParticleHash);
#endif
		
		if (staticParticleHashMapSize != 0)
		{
			staticHashMapBuffer = clCreateBuffer(clContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(uint32) * staticParticleHashMap.Count(), (void*)staticParticleHashMap.Ptr(), &ret);
			CL_CHECK();
		}
		else if (staticHashMapBuffer != nullptr)
		{
			clReleaseMemObject(staticHashMapBuffer);
			staticHashMapBuffer = nullptr;
		}
	}
	void SystemGPU::CreateDynamicParticlesBuffers(ParticleBufferSet& particleBufferSet, float maxInteractionDistance)
	{
		this->particleBufferSet = &dynamic_cast<GPUParticleBufferSet&>(particleBufferSet);
		
		dynamicParticleCount = this->particleBufferSet->GetDynamicParticleCount();

		//find the best combination to be > than the target dynamicParticleHashMapSize but still be in the form of dynamicParticleHashMapSize = pow(scanKernelElementCountPerGroup, n) where n is a natural number
		{
			cl_int ret = 0;

			uintMem maxWorkGroupSize1 = 0;
			uintMem preferredWorkGroupSize1 = 0;
			uintMem maxWorkGroupSize2 = 0;
			uintMem preferredWorkGroupSize2 = 0;
			CL_CALL(clGetKernelWorkGroupInfo(scanOnComputeGroupsKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &maxWorkGroupSize1, nullptr));
			CL_CALL(clGetKernelWorkGroupInfo(scanOnComputeGroupsKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &preferredWorkGroupSize1, nullptr));
			CL_CALL(clGetKernelWorkGroupInfo(addToComputeGroupArraysKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &maxWorkGroupSize2, nullptr));
			CL_CALL(clGetKernelWorkGroupInfo(addToComputeGroupArraysKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &preferredWorkGroupSize2, nullptr));


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
			uintMem dynamicParticleHashMapSizeTarget = 2 * dynamicParticleCount;

			if (dynamicParticleHashMapSizeTarget == 0)
			{
				hashMapBufferGroupSize = 0;
				dynamicParticleHashMapSize = 0;
			}
			else
			{
				uintMem layerCount = std::ceil(std::log(dynamicParticleHashMapSizeTarget) / std::log(hashMapGroupSizeCapacity));
				hashMapBufferGroupSize = 1Ui64 << (uintMem)std::ceil(std::log2(std::pow<float>(dynamicParticleHashMapSizeTarget, 1.0f / layerCount)));
				hashMapBufferGroupSize = std::min(hashMapBufferGroupSize, hashMapGroupSizeCapacity);
				dynamicParticleHashMapSize = std::pow(hashMapBufferGroupSize, layerCount);
			}
		}

#ifdef DEBUG_BUFFERS_GPU
		debugMaxInteractionDistance = maxInteractionDistance;
		debugParticlesArray.Resize(dynamicParticleCount);
		debugHashMapArray.Resize(dynamicParticleHashMapSize + 1);
		debugParticleMapArray.Resize(dynamicParticleCount);
#endif		

		cl_int ret = 0;

		if (dynamicParticleHashMapSize != 0)
		{
			dynamicParticleHashMapBuffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, sizeof(uint32) * (dynamicParticleHashMapSize + 1), nullptr, &ret);
			CL_CHECK();
		}
		else if (dynamicParticleHashMapBuffer != nullptr)
		{
			clReleaseMemObject(dynamicParticleHashMapBuffer);
			dynamicParticleHashMapBuffer = nullptr;
		}

		if (dynamicParticleCount != 0)
		{
			particleMapBuffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, sizeof(uint32) * dynamicParticleCount, nullptr, &ret);
			CL_CHECK();
		}
		else if(particleMapBuffer != nullptr)
		{
			clReleaseMemObject(particleMapBuffer);
			particleMapBuffer = nullptr;
		}
	}
	void SystemGPU::InitializeInternal(const SystemParameters& initParams)
	{		
		initParams.ParseParameter("reorderTimeInterval", reorderTimeInterval);
		initParams.ParseParameter("detailedProfiling", detailedProfiling);
		initParams.ParseParameter("openCLChoosesGroupSize", openCLChoosesGroupSize);
		initParams.ParseParameter("useMaxGroupSize", useMaxGroupSize);

		cl_int ret = 0;
		ParticleSimulationParameters simulationParameters;
		simulationParameters.behaviour = initParams.particleBehaviourParameters;		
		simulationParameters.dynamicParticleCount = dynamicParticleCount;
		simulationParameters.dynamicParticleHashMapSize = dynamicParticleHashMapSize;
		simulationParameters.staticParticleCount = staticParticleCount;
		simulationParameters.staticParticleHashMapSize = staticParticleHashMapSize;
		simulationParameters.smoothingKernelConstant = SmoothingKernelConstant(initParams.particleBehaviourParameters.maxInteractionDistance);
		simulationParameters.selfDensity = initParams.particleBehaviourParameters.particleMass * SmoothingKernelD0(0, initParams.particleBehaviourParameters.maxInteractionDistance) * simulationParameters.smoothingKernelConstant;
		simulationParametersBuffer = clCreateBuffer(clContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(ParticleSimulationParameters), &simulationParameters, &ret);
		CL_CHECK();

		if (openCLChoosesGroupSize)
		{
			CL_CALL(clGetKernelWorkGroupInfo(computeParticleHashesKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &computeParticleHashesKernelPreferredGroupSize, nullptr));
			CL_CALL(clGetKernelWorkGroupInfo(updateParticlesPressureKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &updateParticlesPressureKernelPreferredGroupSize, nullptr));
			CL_CALL(clGetKernelWorkGroupInfo(updateParticlesDynamicsKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &updateParticlesDynamicsKernelPreferredGroupSize, nullptr));
			CL_CALL(clGetKernelWorkGroupInfo(increaseHashMapKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &increaseHashMapKernelPreferredGroupSize, nullptr));
			CL_CALL(clGetKernelWorkGroupInfo(scanOnComputeGroupsKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &scanOnComputeGroupsKernelPreferredGroupSize, nullptr));
			CL_CALL(clGetKernelWorkGroupInfo(addToComputeGroupArraysKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &addToComputeGroupArraysKernelPreferredGroupSize, nullptr));
			CL_CALL(clGetKernelWorkGroupInfo(computeParticleMapKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &computeParticleMapKernelPreferredGroupSize, nullptr));			
		}
		else
		{
			CL_CALL(clGetKernelWorkGroupInfo(computeParticleHashesKernel, clDevice, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(uintMem), &computeParticleHashesKernelPreferredGroupSize, nullptr));
			CL_CALL(clGetKernelWorkGroupInfo(updateParticlesPressureKernel, clDevice, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(uintMem), &updateParticlesPressureKernelPreferredGroupSize, nullptr));
			CL_CALL(clGetKernelWorkGroupInfo(updateParticlesDynamicsKernel, clDevice, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(uintMem), &updateParticlesDynamicsKernelPreferredGroupSize, nullptr));
			CL_CALL(clGetKernelWorkGroupInfo(increaseHashMapKernel, clDevice, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(uintMem), &increaseHashMapKernelPreferredGroupSize, nullptr));
			CL_CALL(clGetKernelWorkGroupInfo(scanOnComputeGroupsKernel, clDevice, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(uintMem), &scanOnComputeGroupsKernelPreferredGroupSize, nullptr));
			CL_CALL(clGetKernelWorkGroupInfo(addToComputeGroupArraysKernel, clDevice, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(uintMem), &addToComputeGroupArraysKernelPreferredGroupSize, nullptr));
			CL_CALL(clGetKernelWorkGroupInfo(computeParticleMapKernel, clDevice, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(uintMem), &computeParticleMapKernelPreferredGroupSize, nullptr));			
		}

		if (dynamicParticleCount != 0)
		{
			cl::Event clearHashMapFinishedEvents[2];

			uint32 pattern0 = 0;
			uint32 patternCount = dynamicParticleCount;
			CL_CALL(clEnqueueFillBuffer(queue, dynamicParticleHashMapBuffer, &pattern0, sizeof(uint32), 0, dynamicParticleHashMapSize * sizeof(uint32), 0, nullptr, &clearHashMapFinishedEvents[0]()));
			CL_CALL(clEnqueueFillBuffer(queue, dynamicParticleHashMapBuffer, &patternCount, sizeof(uint32), dynamicParticleHashMapSize * sizeof(uint32), sizeof(uint32), 0, nullptr, &clearHashMapFinishedEvents[1]()));			

			auto& particlesBufferHandle = this->particleBufferSet->GetReadWriteBufferHandle();
			cl::Event startWriteEvent;
			particlesBufferHandle.StartWrite(&startWriteEvent());

			cl::Event computeParticleHashesFinishedEvent;
			uintMem computeParticleHashesWaitEventCount = 0;
			cl_event computeParticleHashesWaitEvents[4]{};
			if (clearHashMapFinishedEvents[0]() != nullptr) computeParticleHashesWaitEvents[computeParticleHashesWaitEventCount++] = clearHashMapFinishedEvents[0]();
			if (clearHashMapFinishedEvents[1]() != nullptr) computeParticleHashesWaitEvents[computeParticleHashesWaitEventCount++] = clearHashMapFinishedEvents[1]();			
			if (startWriteEvent() != nullptr) computeParticleHashesWaitEvents[computeParticleHashesWaitEventCount++] = startWriteEvent();
			EnqueueComputeParticleHashesKernel(particlesBufferHandle.GetWriteBuffer(), initParams.particleBehaviourParameters.maxInteractionDistance, { computeParticleHashesWaitEvents , computeParticleHashesWaitEventCount }, &computeParticleHashesFinishedEvent());

#ifdef DEBUG_BUFFERS_GPU
			DebugParticles(particlesBufferHandle.GetWriteBuffer());
			DebugPrePrefixSumHashes(particlesBufferHandle.GetWriteBuffer(), dynamicParticleHashMapBuffer);
#endif

			cl::Event partialSumFinishedEvent;
			EnqueuePartialSumKernels({ &computeParticleHashesFinishedEvent(), 1}, &partialSumFinishedEvent());

			cl::Event computeParticleMapFinishedEvent;
			EnqueueComputeParticleMapKernel(particlesBufferHandle.GetWriteBuffer(), nullptr, { &partialSumFinishedEvent() , 1}, &computeParticleMapFinishedEvent());

#ifdef DEBUG_BUFFERS_GPU				
			DebugHashes(particlesBufferHandle.GetWriteBuffer(), dynamicParticleHashMapBuffer);
#endif		


			particlesBufferHandle.FinishWrite({ &computeParticleMapFinishedEvent(), 1 }, true);	
		}		
	}
	void SystemGPU::EnqueueComputeParticleHashesKernel(cl_mem particles, float maxInteractionDistance, ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{
		cl_int ret = 0;

		uint32 dynamicParticleHashMapSize = this->dynamicParticleHashMapSize;
		CL_CALL(clSetKernelArg(computeParticleHashesKernel, 0, sizeof(cl_mem), &particles));
		CL_CALL(clSetKernelArg(computeParticleHashesKernel, 1, sizeof(cl_mem), &dynamicParticleHashMapBuffer));
		CL_CALL(clSetKernelArg(computeParticleHashesKernel, 2, sizeof(maxInteractionDistance), &maxInteractionDistance));
		CL_CALL(clSetKernelArg(computeParticleHashesKernel, 3, sizeof(dynamicParticleHashMapSize), &dynamicParticleHashMapSize));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = computeParticleHashesKernelPreferredGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(queue, computeParticleHashesKernel, 1, &globalWorkOffset, &globalWorkSize, openCLChoosesGroupSize ? nullptr : &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPU::EnqueueClearHashMap(ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{
		cl_int ret = 0;
		byte* pattern = 0;
		CL_CALL(clEnqueueFillBuffer(queue, dynamicParticleHashMapBuffer, &pattern, sizeof(byte), 0, sizeof(uint32) * dynamicParticleHashMapSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPU::EnqueueUpdateParticlesPressureKernel(cl_mem particleReadBuffer, cl_mem particleWriteBuffer, ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{
		cl_int ret = 0;

		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 0, sizeof(cl_mem), &particleReadBuffer));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 1, sizeof(cl_mem), &particleWriteBuffer));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 2, sizeof(cl_mem), &dynamicParticleHashMapBuffer));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 3, sizeof(cl_mem), &particleMapBuffer));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 4, sizeof(cl_mem), &particleBufferSet->GetStaticParticleBuffer()));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 5, sizeof(cl_mem), &staticHashMapBuffer));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 6, sizeof(cl_mem), &simulationParametersBuffer));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = updateParticlesPressureKernelPreferredGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(queue, updateParticlesPressureKernel, 1, &globalWorkOffset, &globalWorkSize, openCLChoosesGroupSize ? nullptr : &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPU::EnqueueUpdateParticlesDynamicsKernel(cl_mem particleReadBuffer, cl_mem particleWriteBuffer, float deltaTime, ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{
		bool moveParticles = false;

		cl_int ret = 0;

		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 0, sizeof(cl_mem), &particleReadBuffer));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 1, sizeof(cl_mem), &particleWriteBuffer));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 2, sizeof(cl_mem), &dynamicParticleHashMapBuffer));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 3, sizeof(cl_mem), &particleMapBuffer));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 4, sizeof(cl_mem), &particleBufferSet->GetStaticParticleBuffer()));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 5, sizeof(cl_mem), &staticHashMapBuffer));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 6, sizeof(deltaTime), &deltaTime));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 7, sizeof(cl_mem), &simulationParametersBuffer));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = updateParticlesDynamicsKernelPreferredGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(queue, updateParticlesDynamicsKernel, 1, &globalWorkOffset, &globalWorkSize, openCLChoosesGroupSize ? nullptr : &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPU::EnqueueIncreaseHashMap(cl_mem particleBuffer, ArrayView<cl_event> waitEvents, cl_event* finishedEvent)
	{
		cl_int ret = 0;

		CL_CALL(clSetKernelArg(increaseHashMapKernel, 0, sizeof(cl_mem), &particleBuffer));
		CL_CALL(clSetKernelArg(increaseHashMapKernel, 1, sizeof(cl_mem), &dynamicParticleHashMapBuffer));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = increaseHashMapKernelPreferredGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(queue, increaseHashMapKernel, 1, &globalWorkOffset, &globalWorkSize, openCLChoosesGroupSize ? nullptr : &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
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
			CL_CALL(clSetKernelArg(scanOnComputeGroupsKernel, 2, sizeof(scale), &scale));

			size_t globalWorkOffset = 0;
			size_t globalWorkSize = size / 2;
			size_t localWorkSize = hashMapBufferGroupSize / 2;
			CL_CALL(clEnqueueNDRangeKernel(queue, scanOnComputeGroupsKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, size == dynamicParticleHashMapSize ? waitEvents.Count() : 1, size == dynamicParticleHashMapSize ? waitEvents.Ptr() : &waitEvent(), &signalEvent()));

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
			CL_CALL(clEnqueueNDRangeKernel(queue, addToComputeGroupArraysKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 1, &waitEvent(), &signalEvent()));

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

		CL_CALL(clSetKernelArg(computeParticleMapKernel, 0, sizeof(cl_mem), &particles));		
		CL_CALL(clSetKernelArg(computeParticleMapKernel, 1, sizeof(cl_mem), orderedParticles));
		CL_CALL(clSetKernelArg(computeParticleMapKernel, 2, sizeof(cl_mem), &dynamicParticleHashMapBuffer));
		CL_CALL(clSetKernelArg(computeParticleMapKernel, 3, sizeof(cl_mem), &particleMapBuffer));
		CL_CALL(clSetKernelArg(computeParticleMapKernel, 4, sizeof(uint32), &reorderParticles));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticleCount;
		size_t localWorkSize = computeParticleMapKernelPreferredGroupSize;;
		CL_CALL(clEnqueueNDRangeKernel(queue, computeParticleMapKernel, 1, &globalWorkOffset, &globalWorkSize, openCLChoosesGroupSize ? nullptr : &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
#ifdef DEBUG_BUFFERS_GPU
	void SystemGPU::DebugParticles(cl_mem particles)
	{
		cl_int ret = 0;

		CL_CALL(clFinish(queue));
		
		CL_CALL(clEnqueueReadBuffer(queue, particles, CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), 0, nullptr, nullptr))

		System::DebugParticles<DynamicParticle>(debugParticlesArray, debugMaxInteractionDistance, dynamicParticleHashMapSize);
	}
	void SystemGPU::DebugPrePrefixSumHashes(cl_mem particles, cl_mem hashMapBuffer)
	{
		cl_int ret = 0;

		CL_CALL(clFinish(queue));
		CL_CALL(clEnqueueReadBuffer(queue, hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticleHashMapSize + 1), debugHashMapArray.Ptr(), 0, nullptr, nullptr));
		CL_CALL(clEnqueueReadBuffer(queue, particles, CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), 0, nullptr, nullptr));

		System::DebugPrePrefixSumHashes<DynamicParticle>(debugParticlesArray, debugHashMapArray);
	}
	void SystemGPU::DebugInterPrefixSumHashes(cl_mem* particles, cl_mem hashMapBuffer, uintMem layerCount)
	{
		cl_int ret = 0;

		CL_CALL(clFinish(queue));
		CL_CALL(clEnqueueReadBuffer(queue, hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticleHashMapSize + 1), debugHashMapArray.Ptr(), 0, nullptr, nullptr));

		if (particles != nullptr)
			CL_CALL(clEnqueueReadBuffer(queue, *particles, CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), 0, nullptr, nullptr));

		System::DebugInterPrefixSumHashes(debugParticlesArray, debugHashMapArray, hashMapBufferGroupSize, layerCount);
	}
	void SystemGPU::DebugHashes(cl_mem particles, cl_mem hashMapBuffer)
	{
		cl_int ret = 0;

		CL_CALL(clFinish(queue));
		CL_CALL(clEnqueueReadBuffer(queue, hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticleHashMapSize + 1), debugHashMapArray.Ptr(), 0, nullptr, nullptr));
		CL_CALL(clEnqueueReadBuffer(queue, particleMapBuffer, CL_TRUE, 0, sizeof(uint32) * dynamicParticleCount, debugParticleMapArray.Ptr(), 0, nullptr, nullptr));
		CL_CALL(clEnqueueReadBuffer(queue, particles, CL_TRUE, 0, sizeof(DynamicParticle) * dynamicParticleCount, debugParticlesArray.Ptr(), 0, nullptr, nullptr));

		System::DebugHashAndParticleMap<DynamicParticle, uint32>(debugParticlesArray, debugHashMapArray, debugParticleMapArray);
	}
#else
	void SystemGPU::DebugParticles(cl_mem particles) { }
	void SystemGPU::DebugPrePrefixSumHashes(cl_mem particles, cl_mem hashMapBuffer) { }
	void SystemGPU::DebugInterPrefixSumHashes(cl_mem* particles, cl_mem hashMapBuffer, uintMem layerCount) { }
	void SystemGPU::DebugHashes(cl_mem particles, cl_mem hashMapBuffer) { }
#endif
}