#include "pch.h"
#include "SPH/SimulationEngines/SimulationEngineGPUKernels.h"
#include "SPH/OpenCL/OpenCLDebug.h"

#include "SPH/Kernels/Kernels.h"

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
	static inline size_t RoundToMultiple(size_t value, size_t multiple)
	{
		return (value + multiple - 1) / multiple * multiple;
	}

	SimulationEngineGPUKernels::SimulationEngineGPUKernels(cl_context clContext, cl_device_id clDevice)
		: clContext(clContext), clDevice(clDevice)
	{
		uint minorVersion, majorVersion;
		GetDeviceVersion(clDevice, majorVersion, minorVersion);

		if (majorVersion == 3)
		{
			uint nonUniformWorkGroupSupport;
			CL_CALL(clGetDeviceInfo(clDevice, CL_DEVICE_NON_UNIFORM_WORK_GROUP_SUPPORT, sizeof(nonUniformWorkGroupSupport), &nonUniformWorkGroupSupport, nullptr));
			supportsNonUniformWorkGroups = static_cast<bool>(nonUniformWorkGroupSupport);
		}
		else
			supportsNonUniformWorkGroups = true;

		Load();
	}
	SimulationEngineGPUKernels::~SimulationEngineGPUKernels()
	{
		clReleaseProgram(program);

		clReleaseKernel(inclusiveScanUpPassKernel);
		clReleaseKernel(inclusiveScanDownPassKernel);
		clReleaseKernel(prepareStaticParticlesHashMapKernel);
		clReleaseKernel(reorderStaticParticlesAndFinishHashMapKernel);
		clReleaseKernel(computeDynamicParticlesHashAndPrepareHashMapKernel);
		clReleaseKernel(reorderDynamicParticlesAndFinishHashMapKernel);
		clReleaseKernel(fillDynamicParticleMapAndFinishHashMapKernel);
		clReleaseKernel(updateParticlesPressureKernel);
		clReleaseKernel(updateParticlesDynamicsKernel);
	}
	void SimulationEngineGPUKernels::EnqueueInclusiveScanKernels(cl_command_queue clCommandQueue, cl_mem hashMap, uintMem hashMapSize, uintMem dynamicParticlesHashMapGroupSize, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		cl::Event waitEvent;
		cl::Event signalEvent;

		for (uintMem size = hashMapSize, layerI = 0; size != 1; size /= dynamicParticlesHashMapGroupSize, ++layerI)
		{
			uintMem tempMemorySize = (dynamicParticlesHashMapGroupSize + 1) * sizeof(uint32);
			uint32 scale = (uint32)(hashMapSize / size);
			uint64 targetGlobalWorkSize = size / 2;

			CL_CALL(clSetKernelArg(inclusiveScanUpPassKernel, 0, tempMemorySize, nullptr));
			CL_CALL(clSetKernelArg(inclusiveScanUpPassKernel, 1, sizeof(cl_mem), &hashMap));
			CL_CALL(clSetKernelArg(inclusiveScanUpPassKernel, 2, sizeof(uint32), &scale));
			CL_CALL(clSetKernelArg(inclusiveScanUpPassKernel, 3, sizeof(uint64), &targetGlobalWorkSize));

			size_t localWorkSize = dynamicParticlesHashMapGroupSize / 2;
			size_t globalWorkOffset = 0;
			size_t globalWorkSize = supportsNonUniformWorkGroups ? targetGlobalWorkSize : RoundToMultiple(targetGlobalWorkSize, localWorkSize);
			CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, inclusiveScanUpPassKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, size == hashMapSize ? waitEvents.Count() : 1, size == hashMapSize ? waitEvents.Ptr() : &waitEvent(), &signalEvent()));

			std::swap(signalEvent(), waitEvent());
			signalEvent = cl::Event();
		}

		for (uintMem topArraySize = dynamicParticlesHashMapGroupSize; topArraySize != hashMapSize; topArraySize *= dynamicParticlesHashMapGroupSize)
		{
			uint32 scale = hashMapSize / dynamicParticlesHashMapGroupSize / topArraySize;
			uint64 targetGlobalWorkSize = (topArraySize - 1) * (dynamicParticlesHashMapGroupSize - 1);

			CL_CALL(clSetKernelArg(inclusiveScanDownPassKernel, 0, sizeof(uintMem), &hashMap));
			CL_CALL(clSetKernelArg(inclusiveScanDownPassKernel, 1, sizeof(uint32), &scale));
			CL_CALL(clSetKernelArg(inclusiveScanDownPassKernel, 2, sizeof(uint64), &targetGlobalWorkSize));

			size_t localWorkSize = dynamicParticlesHashMapGroupSize - 1;
			size_t globalWorkOffset = 0;
			size_t globalWorkSize = supportsNonUniformWorkGroups ? targetGlobalWorkSize : RoundToMultiple(targetGlobalWorkSize, localWorkSize);
			CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, inclusiveScanDownPassKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 1, &waitEvent(), &signalEvent()));

			std::swap(signalEvent(), waitEvent());
			signalEvent = cl::Event();
		}

		*finishedEvent = waitEvent();
		waitEvent() = nullptr;
	}
	void SimulationEngineGPUKernels::EnqueuePrepareStaticParticlesHashMapKernel(cl_command_queue clCommandQueue, cl_mem hashMap, uintMem hashMapSize, cl_mem inParticles, uintMem particleCount, float maxInteractionDistance, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		uint64 threadIDNull = 0;
		uint64 targetGlobalWorkSize = particleCount;
		CL_CALL(clSetKernelArg(prepareStaticParticlesHashMapKernel, 0, sizeof(uint64), &threadIDNull));
		CL_CALL(clSetKernelArg(prepareStaticParticlesHashMapKernel, 1, sizeof(cl_mem), &hashMap));
		CL_CALL(clSetKernelArg(prepareStaticParticlesHashMapKernel, 2, sizeof(uintMem), &hashMapSize));
		CL_CALL(clSetKernelArg(prepareStaticParticlesHashMapKernel, 3, sizeof(cl_mem), &inParticles));
		CL_CALL(clSetKernelArg(prepareStaticParticlesHashMapKernel, 4, sizeof(float), &maxInteractionDistance));
		CL_CALL(clSetKernelArg(prepareStaticParticlesHashMapKernel, 5, sizeof(uint64), &targetGlobalWorkSize));

		size_t localWorkSize = prepareStaticParticlesHashMapKernelWorkGroupSize;
		size_t globalWorkOffset = 0;
		size_t globalWorkSize = supportsNonUniformWorkGroups ? targetGlobalWorkSize : RoundToMultiple(targetGlobalWorkSize, localWorkSize);
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, prepareStaticParticlesHashMapKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SimulationEngineGPUKernels::EnqueueReorderStaticParticlesAndFinishHashMapKernel(cl_command_queue clCommandQueue, cl_mem hashMap, uintMem hashMapSize, cl_mem inParticles, cl_mem outParticles, uintMem particleCount, float maxInteractionDistance, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		uint64 threadIDNull = 0;
		uint64 targetGlobalWorkSize = particleCount;
		CL_CALL(clSetKernelArg(reorderStaticParticlesAndFinishHashMapKernel, 0, sizeof(uint64), &threadIDNull));
		CL_CALL(clSetKernelArg(reorderStaticParticlesAndFinishHashMapKernel, 1, sizeof(cl_mem), &hashMap));
		CL_CALL(clSetKernelArg(reorderStaticParticlesAndFinishHashMapKernel, 2, sizeof(uintMem), &hashMapSize));
		CL_CALL(clSetKernelArg(reorderStaticParticlesAndFinishHashMapKernel, 3, sizeof(cl_mem), &inParticles));
		CL_CALL(clSetKernelArg(reorderStaticParticlesAndFinishHashMapKernel, 4, sizeof(cl_mem), &outParticles));
		CL_CALL(clSetKernelArg(reorderStaticParticlesAndFinishHashMapKernel, 5, sizeof(float), &maxInteractionDistance));
		CL_CALL(clSetKernelArg(reorderStaticParticlesAndFinishHashMapKernel, 6, sizeof(uint64), &targetGlobalWorkSize));

		size_t localWorkSize = reorderStaticParticlesAndFinishHashMapKernelWorkGroupSize;
		size_t globalWorkOffset = 0;
		size_t globalWorkSize = supportsNonUniformWorkGroups ? targetGlobalWorkSize : RoundToMultiple(targetGlobalWorkSize, localWorkSize);
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, reorderStaticParticlesAndFinishHashMapKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SimulationEngineGPUKernels::EnqueueComputeDynamicParticlesHashAndPrepareHashMapKernel(cl_command_queue clCommandQueue, cl_mem hashMap, uintMem hashMapSize, cl_mem particles, uintMem particleCount, float maxInteractionDistance, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		uint64 threadIDNull = 0;
		uint64 targetGlobalWorkSize = particleCount;
		CL_CALL(clSetKernelArg(computeDynamicParticlesHashAndPrepareHashMapKernel, 0, sizeof(uint64), &threadIDNull));
		CL_CALL(clSetKernelArg(computeDynamicParticlesHashAndPrepareHashMapKernel, 1, sizeof(cl_mem), &hashMap));
		CL_CALL(clSetKernelArg(computeDynamicParticlesHashAndPrepareHashMapKernel, 2, sizeof(uintMem), &hashMapSize));
		CL_CALL(clSetKernelArg(computeDynamicParticlesHashAndPrepareHashMapKernel, 3, sizeof(cl_mem), &particles));
		CL_CALL(clSetKernelArg(computeDynamicParticlesHashAndPrepareHashMapKernel, 4, sizeof(float), &maxInteractionDistance));
		CL_CALL(clSetKernelArg(computeDynamicParticlesHashAndPrepareHashMapKernel, 5, sizeof(uint64), &targetGlobalWorkSize));

		size_t localWorkSize = computeDynamicParticlesHashAndPrepareHashMapKernelWorkGroupSize;
		size_t globalWorkOffset = 0;
		size_t globalWorkSize = supportsNonUniformWorkGroups ? targetGlobalWorkSize : RoundToMultiple(targetGlobalWorkSize, localWorkSize);
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, computeDynamicParticlesHashAndPrepareHashMapKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SimulationEngineGPUKernels::EnqueueReorderDynamicParticlesAndFinishHashMapKernel(cl_command_queue clCommandQueue, cl_mem particleMap, cl_mem hashMap, cl_mem inParticles, cl_mem outParticles, uintMem particleCount, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		uint64 threadIDNull = 0;
		uint64 targetGlobalWorkSize = particleCount;
		CL_CALL(clSetKernelArg(reorderDynamicParticlesAndFinishHashMapKernel, 0, sizeof(uint64), &threadIDNull));
		CL_CALL(clSetKernelArg(reorderDynamicParticlesAndFinishHashMapKernel, 1, sizeof(cl_mem), &particleMap));
		CL_CALL(clSetKernelArg(reorderDynamicParticlesAndFinishHashMapKernel, 2, sizeof(cl_mem), &hashMap));
		CL_CALL(clSetKernelArg(reorderDynamicParticlesAndFinishHashMapKernel, 3, sizeof(cl_mem), &inParticles));
		CL_CALL(clSetKernelArg(reorderDynamicParticlesAndFinishHashMapKernel, 4, sizeof(cl_mem), &outParticles));
		CL_CALL(clSetKernelArg(reorderDynamicParticlesAndFinishHashMapKernel, 5, sizeof(uint64), &targetGlobalWorkSize));

		size_t localWorkSize = reorderDynamicParticlesAndFinishHashMapKernelWorkGroupSize;;
		size_t globalWorkOffset = 0;
		size_t globalWorkSize = supportsNonUniformWorkGroups ? targetGlobalWorkSize : RoundToMultiple(targetGlobalWorkSize, localWorkSize);
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, reorderDynamicParticlesAndFinishHashMapKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SimulationEngineGPUKernels::EnqueueFillDynamicParticleMapAndFinishHashMapKernel(cl_command_queue clCommandQueue, cl_mem particleMap, cl_mem hashMap, cl_mem particles, uintMem particleCount, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		cl_int ret = 0;

		uint64 threadIDNull = 0;
		uint64 targetGlobalWorkSize = particleCount;
		CL_CALL(clSetKernelArg(fillDynamicParticleMapAndFinishHashMapKernel, 0, sizeof(uint64), &threadIDNull));
		CL_CALL(clSetKernelArg(fillDynamicParticleMapAndFinishHashMapKernel, 1, sizeof(cl_mem), &particleMap));
		CL_CALL(clSetKernelArg(fillDynamicParticleMapAndFinishHashMapKernel, 2, sizeof(cl_mem), &hashMap));
		CL_CALL(clSetKernelArg(fillDynamicParticleMapAndFinishHashMapKernel, 3, sizeof(cl_mem), &particles));
		CL_CALL(clSetKernelArg(fillDynamicParticleMapAndFinishHashMapKernel, 4, sizeof(uint64), &targetGlobalWorkSize));

		size_t localWorkSize = fillDynamicParticleMapAndFinishHashMapKernelWorkGroupSize;;
		size_t globalWorkOffset = 0;
		size_t globalWorkSize = supportsNonUniformWorkGroups ? targetGlobalWorkSize : RoundToMultiple(targetGlobalWorkSize, localWorkSize);
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, fillDynamicParticleMapAndFinishHashMapKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SimulationEngineGPUKernels::EnqueueUpdateParticlesPressureKernel(cl_command_queue clCommandQueue, cl_mem dynamicParticlesHashMap, uintMem dynamicParticlesHashMapSize, cl_mem staticParticlesHashMap, uintMem staticParticlesHashMapSize, cl_mem particleMap, cl_mem particleReadBuffer, cl_mem particleWriteBuffer, cl_mem staticParticlesBuffer, uintMem dynamicParticlesCount, uintMem staticParticlesCount, cl_mem particleBehaviourParameters, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		const uint64 threadIDNull = 0;
		uint64 targetGlobalWorkSize = dynamicParticlesCount;

		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 0, sizeof(uint64), &threadIDNull));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 1, sizeof(uintMem), &dynamicParticlesCount));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 2, sizeof(uintMem), &dynamicParticlesHashMapSize));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 3, sizeof(uintMem), &staticParticlesCount));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 4, sizeof(uintMem), &staticParticlesHashMapSize));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 5, sizeof(cl_mem), &particleReadBuffer));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 6, sizeof(cl_mem), &particleWriteBuffer));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 7, sizeof(cl_mem), &dynamicParticlesHashMap));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 8, sizeof(cl_mem), &particleMap));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 9, sizeof(cl_mem), &staticParticlesBuffer));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 10, sizeof(cl_mem), &staticParticlesHashMap));
		CL_CALL(clSetKernelArg(updateParticlesPressureKernel, 11, sizeof(cl_mem), &particleBehaviourParameters));

		size_t localWorkSize = updateParticlesPressureKernelWorkGroupSize;
		size_t globalWorkOffset = 0;
		size_t globalWorkSize = supportsNonUniformWorkGroups ? targetGlobalWorkSize : RoundToMultiple(targetGlobalWorkSize, localWorkSize);
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, updateParticlesPressureKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SimulationEngineGPUKernels::EnqueueUpdateParticlesDynamicsKernel(cl_command_queue clCommandQueue, cl_mem dynamicParticlesHashMap, uintMem dynamicParticlesHashMapSize, cl_mem staticParticlesHashMap, uintMem staticParticlesHashMapSize, cl_mem particleMap, cl_mem particleReadBuffer, cl_mem particleWriteBuffer, cl_mem staticParticlesBuffer, uintMem dynamicParticlesCount, uintMem staticParticlesCount, cl_mem particleBehaviourParameters, float deltaTime, uint64 triangleCount, cl_mem triangles, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		const uint64 threadIDNull = 0;
		uint64 targetGlobalWorkSize = dynamicParticlesCount;

		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 0, sizeof(uint64), &threadIDNull));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 1, sizeof(uintMem), &dynamicParticlesCount));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 2, sizeof(uintMem), &dynamicParticlesHashMapSize));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 3, sizeof(uintMem), &staticParticlesCount));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 4, sizeof(uintMem), &staticParticlesHashMapSize));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 5, sizeof(cl_mem), &particleReadBuffer));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 6, sizeof(cl_mem), &particleWriteBuffer));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 7, sizeof(cl_mem), &dynamicParticlesHashMap));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 8, sizeof(cl_mem), &particleMap));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 9, sizeof(cl_mem), &staticParticlesBuffer));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 10, sizeof(cl_mem), &staticParticlesHashMap));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 11, sizeof(float), &deltaTime));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 12, sizeof(cl_mem), &particleBehaviourParameters));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 13, sizeof(uint64), &triangleCount));
		CL_CALL(clSetKernelArg(updateParticlesDynamicsKernel, 14, sizeof(cl_mem), triangles == NULL ? nullptr : &triangles));

		size_t localWorkSize = updateParticlesDynamicsKernelWorkGroupSize;
		size_t globalWorkOffset = 0;
		size_t globalWorkSize = supportsNonUniformWorkGroups ? targetGlobalWorkSize : RoundToMultiple(targetGlobalWorkSize, localWorkSize);
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, updateParticlesDynamicsKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SimulationEngineGPUKernels::DetermineHashGroupSize(uintMem targetHashMapSize, uintMem& dynamicParticlesHashMapGroupSize, uintMem& hashMapSize) const
	{
		uintMem maxWorkGroupSize1 = 0;
		uintMem preferredWorkGroupSize1 = 0;
		uintMem maxWorkGroupSize2 = 0;
		uintMem preferredWorkGroupSize2 = 0;
		CL_CALL(clGetKernelWorkGroupInfo(inclusiveScanUpPassKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &maxWorkGroupSize1, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(inclusiveScanUpPassKernel, clDevice, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(uintMem), &preferredWorkGroupSize1, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(inclusiveScanDownPassKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &maxWorkGroupSize2, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(inclusiveScanDownPassKernel, clDevice, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(uintMem), &preferredWorkGroupSize2, nullptr));

		/*
			These statements have to hold

				(1) hashMapSize = dynamicParticlesHashMapGroupSize ^ layerCount
				(2) hashMapSize > targetHashMapSize
				(3) dynamicParticlesHashMapGroupSize < hashMapGroupSizeCapacity

			layerCount should be as small as possible. layerCount, dynamicParticlesHashMapGroupSize and hashMapSize must be integers

				(4) (from 1) log_hashMapBufferGroupSize(hashMapSize) = layerCount
				(5) (from 2 and 4) log_hashMapBufferGroupSize(targetHashMapSize) < layerCount
				(6) (from 5 and 3) log_hashMapGroupSizeCapacity(targetHashMapSize) < layerCount
				(7) (from 6) layerCount = ceil(log(targetHashMapSize) / log(hashMapGroupSizeCapacity))
		*/
		uintMem hashMapGroupSizeCapacity = std::min(maxWorkGroupSize1 * 2, maxWorkGroupSize2);

		uintMem layerCount = std::ceil(std::log(targetHashMapSize) / std::log(hashMapGroupSizeCapacity));
		dynamicParticlesHashMapGroupSize = 1Ui64 << (uintMem)std::ceil(std::log2(std::pow<float>(targetHashMapSize, 1.0f / layerCount)));
		dynamicParticlesHashMapGroupSize = std::min(dynamicParticlesHashMapGroupSize, hashMapGroupSizeCapacity);
		hashMapSize = std::pow(dynamicParticlesHashMapGroupSize, layerCount);
	}
	void SimulationEngineGPUKernels::Load()
	{
		program = BuildOpenCLProgram(clContext, clDevice, {
			Kernels::compatibilityHeader,
			Kernels::SPHKernelSource,
			}, { {"CL_COMPILER"} });

		CL_CHECK_RET(inclusiveScanUpPassKernel = clCreateKernel(program, "InclusiveScanUpPass", &ret));
		CL_CHECK_RET(inclusiveScanDownPassKernel = clCreateKernel(program, "InclusiveScanDownPass", &ret));
		CL_CHECK_RET(prepareStaticParticlesHashMapKernel = clCreateKernel(program, "PrepareStaticParticlesHashMap", &ret));
		CL_CHECK_RET(reorderStaticParticlesAndFinishHashMapKernel = clCreateKernel(program, "ReorderStaticParticlesAndFinishHashMap", &ret));
		CL_CHECK_RET(computeDynamicParticlesHashAndPrepareHashMapKernel = clCreateKernel(program, "ComputeDynamicParticlesHashAndPrepareHashMap", &ret));
		CL_CHECK_RET(reorderDynamicParticlesAndFinishHashMapKernel = clCreateKernel(program, "ReorderDynamicParticlesAndFinishHashMap", &ret));
		CL_CHECK_RET(fillDynamicParticleMapAndFinishHashMapKernel = clCreateKernel(program, "FillDynamicParticleMapAndFinishHashMap", &ret));
		CL_CHECK_RET(updateParticlesPressureKernel = clCreateKernel(program, "UpdateParticlePressure", &ret));
		CL_CHECK_RET(updateParticlesDynamicsKernel = clCreateKernel(program, "UpdateParticleDynamics", &ret));

		CL_CALL(clGetKernelWorkGroupInfo(inclusiveScanUpPassKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &inclusiveScanUpPassKernelWorkGroupSize, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(inclusiveScanDownPassKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &inclusiveScanDownPassKernelWorkGroupSize, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(prepareStaticParticlesHashMapKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &prepareStaticParticlesHashMapKernelWorkGroupSize, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(reorderStaticParticlesAndFinishHashMapKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &reorderStaticParticlesAndFinishHashMapKernelWorkGroupSize, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(computeDynamicParticlesHashAndPrepareHashMapKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &computeDynamicParticlesHashAndPrepareHashMapKernelWorkGroupSize, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(reorderDynamicParticlesAndFinishHashMapKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &reorderDynamicParticlesAndFinishHashMapKernelWorkGroupSize, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(fillDynamicParticleMapAndFinishHashMapKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &fillDynamicParticleMapAndFinishHashMapKernelWorkGroupSize, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(updateParticlesPressureKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &updateParticlesPressureKernelWorkGroupSize, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(updateParticlesDynamicsKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &updateParticlesDynamicsKernelWorkGroupSize, nullptr));

		File infoFile{ "OpenCLInfo.txt", FileAccessPermission::Write };

		PrintDeviceInfo(clDevice, infoFile);
		PrintKernelInfo(inclusiveScanUpPassKernel, clDevice, infoFile);
		PrintKernelInfo(inclusiveScanDownPassKernel, clDevice, infoFile);
		PrintKernelInfo(prepareStaticParticlesHashMapKernel, clDevice, infoFile);
		PrintKernelInfo(reorderStaticParticlesAndFinishHashMapKernel, clDevice, infoFile);
		PrintKernelInfo(computeDynamicParticlesHashAndPrepareHashMapKernel, clDevice, infoFile);
		PrintKernelInfo(reorderDynamicParticlesAndFinishHashMapKernel, clDevice, infoFile);
		PrintKernelInfo(fillDynamicParticleMapAndFinishHashMapKernel, clDevice, infoFile);
		PrintKernelInfo(updateParticlesPressureKernel, clDevice, infoFile);
		PrintKernelInfo(updateParticlesDynamicsKernel, clDevice, infoFile);
	}
}