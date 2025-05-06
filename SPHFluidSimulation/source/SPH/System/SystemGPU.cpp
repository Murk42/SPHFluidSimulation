#include "pch.h"
#include "SPH/Core/Scene.h"
#include "SPH/Core/ParticleBufferManager.h"
#include "SPH/System/SystemGPU.h"
#include "SPH/Kernels/Kernels.h"
#include "OpenCLDebug.h"

//struct KernelEnqueueData
//{
//	cl_command_queue queue;
//	size_t globalWorkOffset;
//	size_t globalWorkSize;
//	size_t localWorkSize;
//	ArrayView<cl_event> waitEvents;
//	cl_event* signalEvent;
//};
//#define KERNEL_ARG(name, type) type name
//
//#define KERNEL(name, ...) \
//class name##_KernelType   \
//{                         \
//public:                   \
//	name##_KernelType(cl_program program)\
//	{\
//		CL_CHECK_RET(kernel = clCreateKernel(program, #name, &ret));\
//	}\
//	~name##_KernelType()\
//	{\
//		CL_CALL(clReleaseKernel(kernel));\
//	}\
//	cl_int operator()(const KernelEnqueueData& data, __VA_ARGS__)\
//	{\
//	#__VA_ARGS__\
//	}\
//private:\
//	cl_kernel kernel;\
//} name;
//
//KERNEL(updateParticlesPressureKernel, uintMem hashMap, uintMem markoCar);

#define DEBUG_BUFFERS_GPU

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

	SystemGPUKernels::SystemGPUKernels(cl_context clContext, cl_device_id clDevice)
		: clContext(clContext), clDevice(clDevice)
	{
		Load();
	}
	SystemGPUKernels::~SystemGPUKernels()
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
	void SystemGPUKernels::EnqueueInclusiveScanKernels(cl_command_queue clCommandQueue, cl_mem hashMap, uintMem hashMapSize, uintMem dynamicParticlesHashMapGroupSize, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		cl::Event waitEvent;
		cl::Event signalEvent;

		for (uintMem size = hashMapSize, layerI = 0; size != 1; size /= dynamicParticlesHashMapGroupSize, ++layerI)
		{
			uintMem tempMemorySize = (dynamicParticlesHashMapGroupSize + 1) * sizeof(uint32);
			uint32 scale = (uint32)(hashMapSize / size);
			CL_CALL(clSetKernelArg(inclusiveScanUpPassKernel, 0, tempMemorySize, nullptr));
			CL_CALL(clSetKernelArg(inclusiveScanUpPassKernel, 1, sizeof(cl_mem), &hashMap));
			CL_CALL(clSetKernelArg(inclusiveScanUpPassKernel, 2, sizeof(uint32), &scale));

			size_t globalWorkOffset = 0;
			size_t globalWorkSize = size / 2;
			size_t localWorkSize = dynamicParticlesHashMapGroupSize / 2;
			CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, inclusiveScanUpPassKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, size == hashMapSize ? waitEvents.Count() : 1, size == hashMapSize ? waitEvents.Ptr() : &waitEvent(), &signalEvent()));

			std::swap(signalEvent(), waitEvent());
			signalEvent = cl::Event();
		}

		for (uintMem topArraySize = dynamicParticlesHashMapGroupSize; topArraySize != hashMapSize; topArraySize *= dynamicParticlesHashMapGroupSize)
		{
			uint32 scale = hashMapSize / dynamicParticlesHashMapGroupSize / topArraySize;

			CL_CALL(clSetKernelArg(inclusiveScanDownPassKernel, 0, sizeof(uintMem), &hashMap));
			CL_CALL(clSetKernelArg(inclusiveScanDownPassKernel, 1, sizeof(scale), &scale));

			size_t globalWorkOffset = 0;
			size_t globalWorkSize = (topArraySize - 1) * (dynamicParticlesHashMapGroupSize - 1);
			size_t localWorkSize = dynamicParticlesHashMapGroupSize - 1;
			CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, inclusiveScanDownPassKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, 1, &waitEvent(), &signalEvent()));

			std::swap(signalEvent(), waitEvent());
			signalEvent = cl::Event();
		}

		*finishedEvent = waitEvent();
		waitEvent() = nullptr;
	}	
	void SystemGPUKernels::EnqueuePrepareStaticParticlesHashMapKernel(cl_command_queue clCommandQueue, cl_mem hashMap, uintMem hashMapSize, cl_mem inParticles, uintMem particleCount, float maxInteractionDistance, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		uint64 threadIDNull = 0;
		CL_CALL(clSetKernelArg(prepareStaticParticlesHashMapKernel, 0, sizeof(uint64), &threadIDNull));
		CL_CALL(clSetKernelArg(prepareStaticParticlesHashMapKernel, 1, sizeof(cl_mem), &hashMap));
		CL_CALL(clSetKernelArg(prepareStaticParticlesHashMapKernel, 2, sizeof(uintMem), &hashMapSize));
		CL_CALL(clSetKernelArg(prepareStaticParticlesHashMapKernel, 3, sizeof(cl_mem), &inParticles));
		CL_CALL(clSetKernelArg(prepareStaticParticlesHashMapKernel, 4, sizeof(float), &maxInteractionDistance));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = particleCount;
		size_t localWorkSize = prepareStaticParticlesHashMapKernelWorkGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, prepareStaticParticlesHashMapKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPUKernels::EnqueueReorderStaticParticlesAndFinishHashMapKernel(cl_command_queue clCommandQueue, cl_mem hashMap, uintMem hashMapSize, cl_mem inParticles, cl_mem outParticles, uintMem particleCount, float maxInteractionDistance, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		uint64 threadIDNull = 0;
		CL_CALL(clSetKernelArg(reorderStaticParticlesAndFinishHashMapKernel, 0, sizeof(uint64), &threadIDNull));
		CL_CALL(clSetKernelArg(reorderStaticParticlesAndFinishHashMapKernel, 1, sizeof(cl_mem), &hashMap));
		CL_CALL(clSetKernelArg(reorderStaticParticlesAndFinishHashMapKernel, 2, sizeof(uintMem), &hashMapSize));
		CL_CALL(clSetKernelArg(reorderStaticParticlesAndFinishHashMapKernel, 3, sizeof(cl_mem), &inParticles));
		CL_CALL(clSetKernelArg(reorderStaticParticlesAndFinishHashMapKernel, 4, sizeof(cl_mem), &outParticles));
		CL_CALL(clSetKernelArg(reorderStaticParticlesAndFinishHashMapKernel, 5, sizeof(float), &maxInteractionDistance));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = particleCount;
		size_t localWorkSize = reorderStaticParticlesAndFinishHashMapKernelWorkGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, reorderStaticParticlesAndFinishHashMapKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPUKernels::EnqueueComputeDynamicParticlesHashAndPrepareHashMapKernel(cl_command_queue clCommandQueue, cl_mem hashMap, uintMem hashMapSize, cl_mem particles, uintMem particleCount, float maxInteractionDistance, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		uint64 threadIDNull = 0;
		CL_CALL(clSetKernelArg(computeDynamicParticlesHashAndPrepareHashMapKernel, 0, sizeof(uint64), &threadIDNull));
		CL_CALL(clSetKernelArg(computeDynamicParticlesHashAndPrepareHashMapKernel, 1, sizeof(cl_mem), &hashMap));
		CL_CALL(clSetKernelArg(computeDynamicParticlesHashAndPrepareHashMapKernel, 2, sizeof(uintMem), &hashMapSize));
		CL_CALL(clSetKernelArg(computeDynamicParticlesHashAndPrepareHashMapKernel, 3, sizeof(cl_mem), &particles));
		CL_CALL(clSetKernelArg(computeDynamicParticlesHashAndPrepareHashMapKernel, 4, sizeof(float), &maxInteractionDistance));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = particleCount;
		size_t localWorkSize = computeDynamicParticlesHashAndPrepareHashMapKernelWorkGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, computeDynamicParticlesHashAndPrepareHashMapKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPUKernels::EnqueueReorderDynamicParticlesAndFinishHashMapKernel(cl_command_queue clCommandQueue, cl_mem particleMap, cl_mem hashMap, cl_mem inParticles, cl_mem outParticles, uintMem particleCount, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		uint64 threadIDNull = 0;
		CL_CALL(clSetKernelArg(reorderDynamicParticlesAndFinishHashMapKernel, 0, sizeof(uint64), &threadIDNull));
		CL_CALL(clSetKernelArg(reorderDynamicParticlesAndFinishHashMapKernel, 1, sizeof(cl_mem), &particleMap));
		CL_CALL(clSetKernelArg(reorderDynamicParticlesAndFinishHashMapKernel, 2, sizeof(cl_mem), &hashMap));
		CL_CALL(clSetKernelArg(reorderDynamicParticlesAndFinishHashMapKernel, 3, sizeof(cl_mem), &inParticles));
		CL_CALL(clSetKernelArg(reorderDynamicParticlesAndFinishHashMapKernel, 4, sizeof(cl_mem), &outParticles));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = particleCount;
		size_t localWorkSize = reorderDynamicParticlesAndFinishHashMapKernelWorkGroupSize;;
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, reorderDynamicParticlesAndFinishHashMapKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPUKernels::EnqueueFillDynamicParticleMapAndFinishHashMapKernel(cl_command_queue clCommandQueue, cl_mem particleMap, cl_mem hashMap, cl_mem particles, uintMem particleCount, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		cl_int ret = 0;

		uint64 threadIDNull = 0;
		CL_CALL(clSetKernelArg(fillDynamicParticleMapAndFinishHashMapKernel, 0, sizeof(uint64), &threadIDNull));
		CL_CALL(clSetKernelArg(fillDynamicParticleMapAndFinishHashMapKernel, 1, sizeof(cl_mem), &particleMap));
		CL_CALL(clSetKernelArg(fillDynamicParticleMapAndFinishHashMapKernel, 2, sizeof(cl_mem), &hashMap));
		CL_CALL(clSetKernelArg(fillDynamicParticleMapAndFinishHashMapKernel, 3, sizeof(cl_mem), &particles));

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = particleCount;
		size_t localWorkSize = fillDynamicParticleMapAndFinishHashMapKernelWorkGroupSize;;
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, fillDynamicParticleMapAndFinishHashMapKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPUKernels::EnqueueUpdateParticlesPressureKernel(cl_command_queue clCommandQueue, cl_mem dynamicParticlesHashMap, uintMem dynamicParticlesHashMapSize, cl_mem staticParticlesHashMap, uintMem staticParticlesHashMapSize, cl_mem particleMap, cl_mem particleReadBuffer, cl_mem particleWriteBuffer, cl_mem staticParticlesBuffer, uintMem dynamicParticlesCount, uintMem staticParticlesCount, cl_mem particleBehaviourParameters, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		const uint64 threadIDNull = 0;
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

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticlesCount;
		size_t localWorkSize = updateParticlesPressureKernelWorkGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, updateParticlesPressureKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPUKernels::EnqueueUpdateParticlesDynamicsKernel(cl_command_queue clCommandQueue, cl_mem dynamicParticlesHashMap, uintMem dynamicParticlesHashMapSize, cl_mem staticParticlesHashMap, uintMem staticParticlesHashMapSize, cl_mem particleMap, cl_mem particleReadBuffer, cl_mem particleWriteBuffer, cl_mem staticParticlesBuffer, uintMem dynamicParticlesCount, uintMem staticParticlesCount, cl_mem particleBehaviourParameters, float deltaTime, ArrayView<cl_event> waitEvents, cl_event* finishedEvent) const
	{
		const uint64 threadIDNull = 0;
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

		size_t globalWorkOffset = 0;
		size_t globalWorkSize = dynamicParticlesCount;
		size_t localWorkSize = updateParticlesDynamicsKernelWorkGroupSize;
		CL_CALL(clEnqueueNDRangeKernel(clCommandQueue, updateParticlesDynamicsKernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), finishedEvent));
	}
	void SystemGPUKernels::DetermineHashGroupSize(uintMem targetHashMapSize, uintMem& dynamicParticlesHashMapGroupSize, uintMem& hashMapSize) const
	{
		uintMem maxWorkGroupSize1 = 0;
		uintMem preferredWorkGroupSize1 = 0;
		uintMem maxWorkGroupSize2 = 0;
		uintMem preferredWorkGroupSize2 = 0;
		CL_CALL(clGetKernelWorkGroupInfo(inclusiveScanUpPassKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &maxWorkGroupSize1, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(inclusiveScanUpPassKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &preferredWorkGroupSize1, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(inclusiveScanDownPassKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &maxWorkGroupSize2, nullptr));
		CL_CALL(clGetKernelWorkGroupInfo(inclusiveScanDownPassKernel, clDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &preferredWorkGroupSize2, nullptr));

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
	void SystemGPUKernels::Load()
	{
		program = BuildOpenCLProgram(clContext, clDevice, {
			String(compatibilityHeaderOpenCLBytes, compatibiliyHeaderOpenCLSize),
			String(SPHKernelSourceBytes, SPHKernelSourceSize),
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

	SystemGPU::SystemGPU(cl_context clContext, cl_device_id clDevice, cl_command_queue clCommandQueue, Graphics::OpenGL::GraphicsContext_OpenGL& glContext) :
		clContext(clContext), clDevice(clDevice), clCommandQueue(clCommandQueue), glContext(glContext), kernels(clContext, clDevice),
		initialized(false),
		staticParticlesBufferManager(nullptr), dynamicParticlesBufferManager(nullptr),
		dynamicParticlesHashMapSize(0), staticParticlesHashMapSize(0), dynamicParticlesHashMapGroupSize(0),
		reorderElapsedTime(0.0f), reorderTimeInterval(FLT_MAX),
		simulationTime(0)
	{
		uintMem deviceMajorVersion, deviceMinorVersion;
		GetDeviceVersion(clDevice, deviceMajorVersion, deviceMinorVersion);

		if (deviceMajorVersion < 2)
			Debug::Logger::LogError("Client", "Given OpenCL device OpenCL version is less than 2.0");

		if (deviceMajorVersion == 3)
		{
			uintMem size;
			CL_CALL(clGetDeviceInfo(clDevice, CL_DEVICE_OPENCL_C_FEATURES, 0, nullptr, &size));
			Array<cl_name_version> features{ size / sizeof(cl_name_version) };
			CL_CALL(clGetDeviceInfo(clDevice, CL_DEVICE_OPENCL_C_FEATURES, size, features.Ptr(), nullptr));

			Set<String> featuresSet;
			for (auto& feature : features)
				featuresSet.Insert(feature.name);

			if (!featuresSet.Contains("__opencl_c_device_enqueue"))
				Debug::Logger::LogError("Client", "Given OpenCL device doesn't support the \"__opencl_c_device_enqueue\" OpenCL C feature");
		}

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
	}
	SystemGPU::~SystemGPU()
	{
		Clear();
	}
	void SystemGPU::Clear()
	{
		clFinish(clCommandQueue);

		if (dynamicParticlesHashMap != nullptr)
		{
			clReleaseMemObject(dynamicParticlesHashMap);
			dynamicParticlesHashMap = nullptr;
		}
		if (particleMapBuffer != nullptr)
		{
			clReleaseMemObject(particleMapBuffer);
			particleMapBuffer = nullptr;
		}
		if (staticParticlesHashMap != nullptr)
		{
			clReleaseMemObject(staticParticlesHashMap);
			staticParticlesHashMap = nullptr;
		}
		if (particleBehaviourParametersBuffer != nullptr)
		{
			clReleaseMemObject(particleBehaviourParametersBuffer);
			particleBehaviourParametersBuffer = nullptr;
		}

		dynamicParticlesBufferManager = nullptr;
		staticParticlesBufferManager = nullptr;

		dynamicParticlesHashMapSize = 0;
		staticParticlesHashMapSize = 0;

		dynamicParticlesHashMapGroupSize = 0;

		reorderElapsedTime = 0.0f;
		reorderTimeInterval = FLT_MAX;

		simulationTime = 0;

		initialized = false;
	}
	void SystemGPU::Initialize(Scene& scene, ParticleBufferManager& dynamicParticlesBufferManager, ParticleBufferManager& staticParticlesBufferManager)
	{
		Clear();

		this->dynamicParticlesBufferManager = &dynamicParticlesBufferManager;
		this->staticParticlesBufferManager = &staticParticlesBufferManager;

		auto parameters = scene.GetSystemParameters();
		parameters.ParseParameter("reorderTimeInterval", reorderTimeInterval);

		ParticleBehaviourParameters particleBehaviourParameters = parameters.particleBehaviourParameters;
		particleBehaviourParameters.smoothingKernelConstant = SmoothingKernelConstant(particleBehaviourParameters.maxInteractionDistance);
		particleBehaviourParameters.selfDensity = particleBehaviourParameters.particleMass * SmoothingKernelD0(0, particleBehaviourParameters.maxInteractionDistance) * particleBehaviourParameters.smoothingKernelConstant;
		CL_CHECK_RET(particleBehaviourParametersBuffer = clCreateBuffer(clContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(ParticleBehaviourParameters), &particleBehaviourParameters, &ret));

		InitializeStaticParticles(scene, staticParticlesBufferManager);
		InitializeDynamicParticles(scene, dynamicParticlesBufferManager);

		initialized = true;
	}
	void SystemGPU::Update(float deltaTime, uint simulationStepCount)
	{
		if (!initialized)
		{
			Debug::Logger::LogWarning("Client", "Updating a uninitialized SPHSystem");
			return;
		}

		if (dynamicParticlesBufferManager->GetBufferSize() == 0)
			return;

		Array<PerformanceProfile<20>> performanceProfiles{ simulationStepCount };

		cl::Event staticParticlesReadStartEvent;
		auto staticParticlesLockGuard = staticParticlesBufferManager->LockRead(&staticParticlesReadStartEvent());
		auto staticParticles = (cl_mem)staticParticlesLockGuard.GetResource();

		cl::Event updateEndEvent;

		for (uint i = 0; i < simulationStepCount; ++i)
		{
			cl::Event readLockAcquiredEvent;
			auto inputParticlesLockGuard = dynamicParticlesBufferManager->LockRead(&readLockAcquiredEvent);
			cl_mem inputParticles = (cl_mem)inputParticlesLockGuard.GetResource();

			dynamicParticlesBufferManager->Advance();

			cl::Event writeLockAcquiredEvent;
			auto outputParticlesLockGuard = dynamicParticlesBufferManager->LockWrite(&writeLockAcquiredEvent);
			cl_mem outputParticles = (cl_mem)outputParticlesLockGuard.GetResource();


#ifdef DEBUG_BUFFERS_GPU			
			DebugParticles(inputParticles);
#endif										

			cl::Event updatePressureFinishedEvent;
			kernels.EnqueueUpdateParticlesPressureKernel(clCommandQueue, dynamicParticlesHashMap, dynamicParticlesHashMapSize, staticParticlesHashMap, staticParticlesHashMapSize, particleMapBuffer, inputParticles, outputParticles, staticParticles, dynamicParticlesBufferManager->GetBufferSize() / sizeof(DynamicParticle), staticParticlesBufferManager->GetBufferSize() / sizeof(StaticParticle), particleBehaviourParametersBuffer, EventWaitArray<3>{ readLockAcquiredEvent, writeLockAcquiredEvent, staticParticlesReadStartEvent  }, & updatePressureFinishedEvent());
			staticParticlesReadStartEvent = cl::Event();
			readLockAcquiredEvent = cl::Event();
			writeLockAcquiredEvent = cl::Event();

			performanceProfiles[i].AddPendingMeasurement("Update particle pressure kernel", updatePressureFinishedEvent());

			cl::Event updateDynamicsFinishedEvent;
			kernels.EnqueueUpdateParticlesDynamicsKernel(clCommandQueue, dynamicParticlesHashMap, dynamicParticlesHashMapSize, staticParticlesHashMap, staticParticlesHashMapSize, particleMapBuffer, inputParticles, outputParticles, staticParticles, dynamicParticlesBufferManager->GetBufferSize() / sizeof(DynamicParticle), staticParticlesBufferManager->GetBufferSize() / sizeof(StaticParticle), particleBehaviourParametersBuffer, deltaTime, { &updatePressureFinishedEvent(), 1 }, &updateDynamicsFinishedEvent());
			updatePressureFinishedEvent = cl::Event();

			performanceProfiles[i].AddPendingMeasurement("Update particle dynamics kernel", updateDynamicsFinishedEvent());

			inputParticlesLockGuard.Unlock({ (void**)&updateDynamicsFinishedEvent(), 1 });

#ifdef DEBUG_BUFFERS_GPU
			DebugParticles(outputParticles);
#endif

			cl::Event clearHashMapFinishedEvent;
			uint32 zeroPattern = 0;
			CL_CALL(clEnqueueFillBuffer(clCommandQueue, dynamicParticlesHashMap, &zeroPattern, sizeof(zeroPattern), 0, dynamicParticlesHashMapSize * sizeof(zeroPattern), 1, &updateDynamicsFinishedEvent(), &clearHashMapFinishedEvent()));
			updateDynamicsFinishedEvent = cl::Event();

			performanceProfiles[i].AddPendingMeasurement("Clear hash map", clearHashMapFinishedEvent());

			cl::Event incrementHashMapEventFinished;
			kernels.EnqueueComputeDynamicParticlesHashAndPrepareHashMapKernel(clCommandQueue, dynamicParticlesHashMap, dynamicParticlesHashMapSize, outputParticles, dynamicParticlesBufferManager->GetBufferSize() / sizeof(DynamicParticle), particleBehaviourParameters.maxInteractionDistance, { &clearHashMapFinishedEvent(), 1 }, &incrementHashMapEventFinished());
			clearHashMapFinishedEvent = cl::Event();

			performanceProfiles[i].AddPendingMeasurement("Increment hash map kernel", incrementHashMapEventFinished());

#ifdef DEBUG_BUFFERS_GPU
			DebugParticles(outputParticles);
			DebugPrePrefixSumHashes(outputParticles, dynamicParticlesHashMap);
#endif

			cl::Event partialSumFinishedEvent;
			kernels.EnqueueInclusiveScanKernels(clCommandQueue, dynamicParticlesHashMap, dynamicParticlesHashMapSize, dynamicParticlesHashMapGroupSize, { &incrementHashMapEventFinished(), 1 }, &partialSumFinishedEvent());
			incrementHashMapEventFinished = cl::Event();

			cl::Event computeParticleMapFinishedEvent;
			if (reorderElapsedTime > reorderTimeInterval)
			{
				reorderElapsedTime -= reorderTimeInterval;

				dynamicParticlesBufferManager->Advance();

				cl::Event startIntermediateEvent;
				auto intermediateParticlesLockGuard = dynamicParticlesBufferManager->LockWrite(&startIntermediateEvent());
				auto intermediateParticles = (cl_mem)intermediateParticlesLockGuard.GetResource();

				EventWaitArray<2> computeParticleMapWaitEvents{ startIntermediateEvent, partialSumFinishedEvent };				
				kernels.EnqueueReorderDynamicParticlesAndFinishHashMapKernel(clCommandQueue, particleMapBuffer, dynamicParticlesHashMap, outputParticles, intermediateParticles, dynamicParticlesBufferManager->GetBufferSize() / sizeof(DynamicParticle), computeParticleMapWaitEvents, &computeParticleMapFinishedEvent());
				computeParticleMapWaitEvents.Release();				

				outputParticlesLockGuard.Unlock({ (void**)&computeParticleMapFinishedEvent(), 1 });

				std::swap(intermediateParticles, outputParticles);
				std::swap(intermediateParticlesLockGuard, outputParticlesLockGuard);
			}
			else
			{
				kernels.EnqueueFillDynamicParticleMapAndFinishHashMapKernel(clCommandQueue, particleMapBuffer, dynamicParticlesHashMap, outputParticles, dynamicParticlesBufferManager->GetBufferSize() / sizeof(DynamicParticle), { &partialSumFinishedEvent(), 1 }, &computeParticleMapFinishedEvent());
			}
			partialSumFinishedEvent = cl::Event();

			performanceProfiles[i].AddPendingMeasurement("Compute particle map kernel", computeParticleMapFinishedEvent());

#ifdef DEBUG_BUFFERS_GPU
			DebugHashes(outputParticles, dynamicParticlesHashMap);
#endif						
			outputParticlesLockGuard.Unlock({ (void**)&computeParticleMapFinishedEvent(), 1 });

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
	void SystemGPU::InitializeStaticParticles(Scene& scene, ParticleBufferManager& staticParticlesBufferManager)
	{
		uintMem staticParticlesCount = 0;
		cl::Buffer tempStaticParticles;

		{
			Array<StaticParticle> staticParticles;
			scene.GenerateLayerParticles("static", staticParticles);

			if (staticParticles.Empty())
				return;

			CL_CHECK_RET(tempStaticParticles = clCreateBuffer(clContext, CL_MEM_USE_HOST_PTR, staticParticles.Count() * sizeof(StaticParticle), staticParticles.Ptr(), &ret));

			staticParticlesCount = staticParticles.Count();
		}

		staticParticlesHashMapSize = staticParticlesCount;
		kernels.DetermineHashGroupSize(staticParticlesHashMapSize, staticParticlesHashMapGroupSize, staticParticlesHashMapSize);

		CL_CHECK_RET(staticParticlesHashMap = clCreateBuffer(clContext, CL_MEM_READ_WRITE, sizeof(uint32) * (staticParticlesHashMapSize + 1), nullptr, &ret))
		staticParticlesBufferManager.Allocate(staticParticlesCount * sizeof(StaticParticle), nullptr, 1);

		uint32 pattern0 = 0;
		uint32 patternCount = staticParticlesCount;

		cl::Event fillHashMapFinishedEvent1;
		CL_CALL(clEnqueueFillBuffer(clCommandQueue, staticParticlesHashMap, &pattern0, sizeof(uint32), 0, staticParticlesHashMapSize * sizeof(uint32), 0, nullptr, &fillHashMapFinishedEvent1()));

		cl::Event fillHashMapFinishedEvent2;
		CL_CALL(clEnqueueFillBuffer(clCommandQueue, staticParticlesHashMap, &patternCount, sizeof(uint32), staticParticlesHashMapSize * sizeof(uint32), sizeof(uint32), 0, nullptr, &fillHashMapFinishedEvent2()));

		cl::Event prepareHashMapFinishedEvent;
		EventWaitArray<2> prepareHahsMapWaitEvents{ fillHashMapFinishedEvent1, fillHashMapFinishedEvent2 };
		kernels.EnqueuePrepareStaticParticlesHashMapKernel(clCommandQueue, staticParticlesHashMap, staticParticlesHashMapSize, tempStaticParticles(), staticParticlesCount, particleBehaviourParameters.maxInteractionDistance, prepareHahsMapWaitEvents, &prepareHashMapFinishedEvent());

		cl::Event scanFinishedEvent;
		EventWaitArray<1> scanWaitEvents{ prepareHashMapFinishedEvent };
		kernels.EnqueueInclusiveScanKernels(clCommandQueue, staticParticlesHashMap, staticParticlesHashMapSize, staticParticlesHashMapGroupSize, scanWaitEvents, &scanFinishedEvent());

		cl::Event lockAcquiredEvent;
		ResourceLockGuard finalStaticParticlesLockGuard = staticParticlesBufferManager.LockWrite(&lockAcquiredEvent());
		cl_mem finalStaticParticles = (cl_mem)finalStaticParticlesLockGuard.GetResource();

		cl::Event reorderFinishedEvent;
		EventWaitArray<2> reorderWaitEvents{ lockAcquiredEvent, scanFinishedEvent };
		kernels.EnqueueReorderStaticParticlesAndFinishHashMapKernel(clCommandQueue, staticParticlesHashMap, staticParticlesHashMapSize, tempStaticParticles(), finalStaticParticles, staticParticlesCount, particleBehaviourParameters.maxInteractionDistance, reorderWaitEvents, &reorderFinishedEvent());

		finalStaticParticlesLockGuard.Unlock({ reorderFinishedEvent() });
	}
	void SystemGPU::InitializeDynamicParticles(Scene& scene, ParticleBufferManager& dynamicParticlesBufferManager)
	{
		uintMem dynamicParticlesCount = 0;

		{
			Array<DynamicParticle> dynamicParticles;
			scene.GenerateLayerParticles("dynamic", dynamicParticles);

			if (dynamicParticles.Empty())
				return;

			dynamicParticlesBufferManager.Allocate(dynamicParticles.Count() * sizeof(DynamicParticle), dynamicParticles.Ptr(), 3);
			dynamicParticlesCount = dynamicParticles.Count();
		}

		dynamicParticlesHashMapSize = dynamicParticlesCount * 2;
		kernels.DetermineHashGroupSize(dynamicParticlesHashMapSize, dynamicParticlesHashMapGroupSize, dynamicParticlesHashMapSize);

		CL_CHECK_RET(dynamicParticlesHashMap = clCreateBuffer(clContext, CL_MEM_READ_WRITE, sizeof(uint32) * (dynamicParticlesHashMapSize + 1), nullptr, &ret));
		CL_CHECK_RET(particleMapBuffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, sizeof(uint32) * dynamicParticlesCount, nullptr, &ret));		

		uint32 pattern0 = 0;
		uint32 patternCount = dynamicParticlesCount;

		cl::Event fillHashMapFinishedEvent1;
		CL_CALL(clEnqueueFillBuffer(clCommandQueue, dynamicParticlesHashMap, &pattern0, sizeof(uint32), 0, dynamicParticlesHashMapSize * sizeof(uint32), 0, nullptr, &fillHashMapFinishedEvent1()));

		cl::Event fillHashMapFinishedEvent2;
		CL_CALL(clEnqueueFillBuffer(clCommandQueue, dynamicParticlesHashMap, &patternCount, sizeof(uint32), dynamicParticlesHashMapSize * sizeof(uint32), sizeof(uint32), 0, nullptr, &fillHashMapFinishedEvent2()));

		cl::Event initialDynamicParticlesLockAcquiredEvent;
		ResourceLockGuard initialDynamicParticlesLockGuard = dynamicParticlesBufferManager.LockWrite(&initialDynamicParticlesLockAcquiredEvent());
		cl_mem initialDynamicParticles = (cl_mem)initialDynamicParticlesLockGuard.GetResource();

		cl::Event prepareHashMapFinishedEvent;
		EventWaitArray<3> prepareHahsMapWaitEvents{ fillHashMapFinishedEvent1, fillHashMapFinishedEvent2, initialDynamicParticlesLockAcquiredEvent };
		kernels.EnqueueComputeDynamicParticlesHashAndPrepareHashMapKernel(clCommandQueue, dynamicParticlesHashMap, dynamicParticlesHashMapSize, initialDynamicParticles, dynamicParticlesCount, particleBehaviourParameters.maxInteractionDistance, prepareHahsMapWaitEvents, &prepareHashMapFinishedEvent());

		cl::Event scanFinishedEvent;
		EventWaitArray<1> scanWaitEvents{ prepareHashMapFinishedEvent };
		kernels.EnqueueInclusiveScanKernels(clCommandQueue, dynamicParticlesHashMap, dynamicParticlesHashMapSize, dynamicParticlesHashMapGroupSize, scanWaitEvents, &scanFinishedEvent());

		dynamicParticlesBufferManager.Advance();

		cl::Event finalDynamicParticlesLockAcquiredEvent;
		ResourceLockGuard finalDynamicParticlesLockGuard = dynamicParticlesBufferManager.LockWrite(&finalDynamicParticlesLockAcquiredEvent());
		cl_mem finalDynamicParticles = (cl_mem)finalDynamicParticlesLockGuard.GetResource();

		cl::Event reorderFinishedEvent;
		EventWaitArray<2> reorderWaitEvents{ finalDynamicParticlesLockAcquiredEvent, scanFinishedEvent };
		kernels.EnqueueReorderDynamicParticlesAndFinishHashMapKernel(clCommandQueue, particleMapBuffer, dynamicParticlesHashMap, initialDynamicParticles, finalDynamicParticles, dynamicParticlesCount, reorderWaitEvents, &reorderFinishedEvent());


#ifdef DEBUG_BUFFERS_GPU		
		debugParticlesArray.Resize(dynamicParticlesCount);
		debugHashMapArray.Resize(dynamicParticlesHashMapSize + 1);
		debugParticleMapArray.Resize(dynamicParticlesCount);

		DebugParticles(finalDynamicParticles);
		DebugHashes(finalDynamicParticles, dynamicParticlesHashMap);
#endif				

		initialDynamicParticlesLockGuard.Unlock({ (void**)&reorderFinishedEvent(), 1 });
		finalDynamicParticlesLockGuard.Unlock({ (void**)&reorderFinishedEvent(), 1 });
	}
#ifdef DEBUG_BUFFERS_GPU
	void SystemGPU::DebugParticles(cl_mem particles)
	{
		cl_int ret = 0;

		CL_CALL(clFinish(clCommandQueue));

		CL_CALL(clEnqueueReadBuffer(clCommandQueue, particles, CL_TRUE, 0, dynamicParticlesBufferManager->GetBufferSize(), debugParticlesArray.Ptr(), 0, nullptr, nullptr))

		System::DebugParticles<DynamicParticle>(debugParticlesArray, particleBehaviourParameters.maxInteractionDistance, dynamicParticlesHashMapSize);
	}
	void SystemGPU::DebugPrePrefixSumHashes(cl_mem particles, cl_mem hashMapBuffer)
	{
		cl_int ret = 0;

		CL_CALL(clFinish(clCommandQueue));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticlesHashMapSize + 1), debugHashMapArray.Ptr(), 0, nullptr, nullptr));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, particles, CL_TRUE, 0, dynamicParticlesBufferManager->GetBufferSize(), debugParticlesArray.Ptr(), 0, nullptr, nullptr));

		System::DebugPrePrefixSumHashes<DynamicParticle>(debugParticlesArray, debugHashMapArray);
	}
	void SystemGPU::DebugInterPrefixSumHashes(cl_mem* particles, cl_mem hashMapBuffer, uintMem layerCount)
	{
		cl_int ret = 0;

		CL_CALL(clFinish(clCommandQueue));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticlesHashMapSize + 1), debugHashMapArray.Ptr(), 0, nullptr, nullptr));

		if (particles != nullptr)
			CL_CALL(clEnqueueReadBuffer(clCommandQueue, *particles, CL_TRUE, 0, dynamicParticlesBufferManager->GetBufferSize(), debugParticlesArray.Ptr(), 0, nullptr, nullptr));

		System::DebugInterPrefixSumHashes(debugParticlesArray, debugHashMapArray, dynamicParticlesHashMapGroupSize, layerCount);
	}
	void SystemGPU::DebugHashes(cl_mem particles, cl_mem hashMapBuffer)
	{
		cl_int ret = 0;

		CL_CALL(clFinish(clCommandQueue));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, hashMapBuffer, CL_TRUE, 0, sizeof(uint32) * (dynamicParticlesHashMapSize + 1), debugHashMapArray.Ptr(), 0, nullptr, nullptr));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, particleMapBuffer, CL_TRUE, 0, sizeof(uint32) * dynamicParticlesBufferManager->GetBufferSize() / sizeof(DynamicParticle), debugParticleMapArray.Ptr(), 0, nullptr, nullptr));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, particles, CL_TRUE, 0, dynamicParticlesBufferManager->GetBufferSize(), debugParticlesArray.Ptr(), 0, nullptr, nullptr));

		System::DebugHashAndParticleMap<uint32>(debugParticlesArray, debugHashMapArray, debugParticleMapArray);
	}
#else
	void SystemGPU::DebugParticles(cl_mem particles) {}
	void SystemGPU::DebugPrePrefixSumHashes(cl_mem particles, cl_mem hashMapBuffer) {}
	void SystemGPU::DebugInterPrefixSumHashes(cl_mem* particles, cl_mem hashMapBuffer, uintMem layerCount) {}
	void SystemGPU::DebugHashes(cl_mem particles, cl_mem hashMapBuffer) {}
#endif	
}