#include "pch.h"
#include "SPH/Core/SceneBlueprint.h"
#include "SPH/Core/ParticleBufferManager.h"
#include "SPH/SimulationEngines/SimulationEngineGPU.h"
#include "SPH/OpenCL/OpenCLDebug.h"
#include "SPH/OpenCL/PerformanceProfile.h"
#include "SPH/OpenCL/EventWaitArray.h"

#define DEBUG_BUFFERS_GPU

namespace SPH
{
	struct OpenCLTriangle
	{
		Vec4f p1;
		Vec4f p2;
		Vec4f p3;
	};
	static Array<OpenCLTriangle> GetOpenCLTriangles(const Graphics::BasicIndexedMesh& mesh)
	{
		auto tris = mesh.CreateTriangleArray();
		Array<OpenCLTriangle> out{ tris.Count() };
		for (uintMem i = 0; i < tris.Count(); ++i)
		{
			out[i].p1 = Vec4f(tris[i].p1, 0.0f);
			out[i].p2 = Vec4f(tris[i].p2, 0.0f);
			out[i].p3 = Vec4f(tris[i].p3, 0.0f);
		}
		return out;
	}

	SimulationEngineGPU::SimulationEngineGPU(cl_context clContext, cl_device_id clDevice, cl_command_queue clCommandQueue) :
		clContext(clContext), clDevice(clDevice), clCommandQueue(clCommandQueue), kernels(clContext, clDevice)
	{
		uint deviceMajorVersion, deviceMinorVersion;
		GetDeviceVersion(clDevice, deviceMajorVersion, deviceMinorVersion);

		if (deviceMajorVersion < 2)
			Debug::Logger::LogError("Client", "Given OpenCL device OpenCL version is less than 2.0");

		if (deviceMajorVersion == 3)
		{
			uintMem size;
			CL_CALL(clGetDeviceInfo(clDevice, CL_DEVICE_OPENCL_C_FEATURES, 0, nullptr, &size));
			Array<cl_name_version> features{ size / sizeof(cl_name_version) };
			CL_CALL(clGetDeviceInfo(clDevice, CL_DEVICE_OPENCL_C_FEATURES, size, features.Ptr(), nullptr));

			String openCLCFeaturesString;
			for (auto& feature : features)
				openCLCFeaturesString += Format("{} ", StringView(feature.name, strnlen(feature.name, sizeof(feature.name))));
			BLAZE_LOG_INFO("Available OpenCL C features: {}", openCLCFeaturesString);

			Set<String> featuresSet;
			for (auto& feature : features)
				featuresSet.Insert(feature.name);

			//if (!featuresSet.Contains("__opencl_c_device_enqueue"))
			//	Debug::Logger::LogError("Client", "Given OpenCL device doesn't support the \"__opencl_c_device_enqueue\" OpenCL C feature");
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
	SimulationEngineGPU::~SimulationEngineGPU()
	{
		Clear();
	}
	void SimulationEngineGPU::Clear()
	{
		if (dynamicParticlesBufferManager != nullptr)
		{
			dynamicParticlesBufferManager->FlushAllOperations();
			dynamicParticlesBufferManager = nullptr;
		}

		if (staticParticlesBufferManager != nullptr)
		{
			staticParticlesBufferManager->FlushAllOperations();
			staticParticlesBufferManager = nullptr;
		}

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
		if (triangles != nullptr)
		{
			clReleaseMemObject(triangles);
			triangles = nullptr;
		}


		dynamicParticlesHashMapSize = 0;
		staticParticlesHashMapSize = 0;

		dynamicParticlesHashMapGroupSize = 0;

		reorderElapsedTime = 0.0f;
		reorderTimeInterval = FLT_MAX;

		simulationTime = 0;

		initialized = false;
	}
	void SimulationEngineGPU::Initialize(SceneBlueprint& scene, ParticleBufferManager& dynamicParticlesBufferManager, ParticleBufferManager& staticParticlesBufferManager)
	{
		Clear();

		this->dynamicParticlesBufferManager = &dynamicParticlesBufferManager;
		this->staticParticlesBufferManager = &staticParticlesBufferManager;

		auto parameters = scene.GetSystemParameters();
		parameters.ParseParameter("reorderTimeInterval", reorderTimeInterval);

		particleBehaviourParameters = parameters.particleBehaviourParameters;
		particleBehaviourParameters.smoothingKernelConstant = SmoothingKernelConstant(particleBehaviourParameters.maxInteractionDistance);
		particleBehaviourParameters.selfDensity = particleBehaviourParameters.particleMass * SmoothingKernelD0(0, particleBehaviourParameters.maxInteractionDistance) * particleBehaviourParameters.smoothingKernelConstant;
		CL_CHECK_RET(particleBehaviourParametersBuffer = clCreateBuffer(clContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(ParticleBehaviourParameters), &particleBehaviourParameters, &ret));

		InitializeStaticParticles(scene, staticParticlesBufferManager);
		InitializeDynamicParticles(scene, dynamicParticlesBufferManager);

		const auto& mesh = scene.GetMesh();
		auto _triangles = GetOpenCLTriangles(mesh);
		if (_triangles.Empty())
			triangles = NULL;
		else
			CL_CHECK_RET(triangles = clCreateBuffer(clContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(OpenCLTriangle) * _triangles.Count(), _triangles.Ptr(), &ret));

		triangleCount = _triangles.Count();
		initialized = true;
	}
	void SimulationEngineGPU::Update(float deltaTime, uint simulationStepCount)
	{
		if (!initialized)
		{
			Debug::Logger::LogWarning("Client", "Updating a uninitialized SPHSystem");
			return;
		}

		if (dynamicParticlesBufferManager->GetParticleCount() == 0)
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
			DebugDynamicParticles(clCommandQueue, debugParticlesArray, inputParticles, dynamicParticlesHashMapSize, particleBehaviourParameters.maxInteractionDistance);
#endif

			cl::Event updatePressureFinishedEvent;
			kernels.EnqueueUpdateParticlesPressureKernel(clCommandQueue, dynamicParticlesHashMap, dynamicParticlesHashMapSize, staticParticlesHashMap, staticParticlesHashMapSize, particleMapBuffer, inputParticles, outputParticles, staticParticles, dynamicParticlesBufferManager->GetParticleCount(), staticParticlesBufferManager->GetParticleCount(), particleBehaviourParametersBuffer, EventWaitArray<3>{ readLockAcquiredEvent, writeLockAcquiredEvent, staticParticlesReadStartEvent  }, & updatePressureFinishedEvent());
			staticParticlesReadStartEvent = cl::Event();
			readLockAcquiredEvent = cl::Event();
			writeLockAcquiredEvent = cl::Event();

			performanceProfiles[i].AddPendingMeasurement("Update particle pressure kernel", updatePressureFinishedEvent());

			cl::Event updateDynamicsFinishedEvent;
			kernels.EnqueueUpdateParticlesDynamicsKernel(clCommandQueue, dynamicParticlesHashMap, dynamicParticlesHashMapSize, staticParticlesHashMap, staticParticlesHashMapSize, particleMapBuffer, inputParticles, outputParticles, staticParticles, dynamicParticlesBufferManager->GetParticleCount(), staticParticlesBufferManager->GetParticleCount(), particleBehaviourParametersBuffer, deltaTime, triangleCount, triangles, { &updatePressureFinishedEvent(), 1 }, &updateDynamicsFinishedEvent());
			updatePressureFinishedEvent = cl::Event();

			performanceProfiles[i].AddPendingMeasurement("Update particle dynamics kernel", updateDynamicsFinishedEvent());

			inputParticlesLockGuard.Unlock({ (void**)&updateDynamicsFinishedEvent(), 1 });

#ifdef DEBUG_BUFFERS_GPU
			DebugDynamicParticles(clCommandQueue, debugParticlesArray, outputParticles, dynamicParticlesHashMapSize, particleBehaviourParameters.maxInteractionDistance);
#endif

			cl::Event clearHashMapFinishedEvent;
			uint32 zeroPattern = 0;
			CL_CALL(clEnqueueFillBuffer(clCommandQueue, dynamicParticlesHashMap, &zeroPattern, sizeof(zeroPattern), 0, dynamicParticlesHashMapSize * sizeof(zeroPattern), 1, &updateDynamicsFinishedEvent(), &clearHashMapFinishedEvent()));
			updateDynamicsFinishedEvent = cl::Event();

			performanceProfiles[i].AddPendingMeasurement("Clear hash map", clearHashMapFinishedEvent());

			cl::Event incrementHashMapEventFinished;
			kernels.EnqueueComputeDynamicParticlesHashAndPrepareHashMapKernel(clCommandQueue, dynamicParticlesHashMap, dynamicParticlesHashMapSize, outputParticles, dynamicParticlesBufferManager->GetParticleCount(), particleBehaviourParameters.maxInteractionDistance, { &clearHashMapFinishedEvent(), 1 }, &incrementHashMapEventFinished());
			clearHashMapFinishedEvent = cl::Event();

			performanceProfiles[i].AddPendingMeasurement("Increment hash map kernel", incrementHashMapEventFinished());

#ifdef DEBUG_BUFFERS_GPU
			CL_CALL(clFinish(clCommandQueue));
			CL_CALL(clEnqueueReadBuffer(clCommandQueue, dynamicParticlesHashMap, CL_TRUE, 0, sizeof(uint32) * debugHashMapArray.Count(), debugHashMapArray.Ptr(), 0, nullptr, nullptr));

			DebugDynamicParticles(clCommandQueue, debugParticlesArray, outputParticles, dynamicParticlesHashMapSize, particleBehaviourParameters.maxInteractionDistance);
#endif

			cl::Event partialSumFinishedEvent;
			kernels.EnqueueInclusiveScanKernels(clCommandQueue, dynamicParticlesHashMap, dynamicParticlesHashMapSize, dynamicParticlesHashMapGroupSize, { &incrementHashMapEventFinished(), 1 }, &partialSumFinishedEvent());
			incrementHashMapEventFinished = cl::Event();

#ifdef DEBUG_BUFFERS_GPU
			CL_CALL(clFinish(clCommandQueue));
			CL_CALL(clEnqueueReadBuffer(clCommandQueue, dynamicParticlesHashMap, CL_TRUE, 0, sizeof(uint32) * debugHashMapArray.Count(), debugHashMapArray.Ptr(), 0, nullptr, nullptr));
#endif
			
			cl::Event computeParticleMapFinishedEvent;
			if (reorderElapsedTime > reorderTimeInterval)
			{
				reorderElapsedTime -= reorderTimeInterval;

				dynamicParticlesBufferManager->Advance();

				cl::Event startIntermediateEvent;
				auto intermediateParticlesLockGuard = dynamicParticlesBufferManager->LockWrite(&startIntermediateEvent());
				auto intermediateParticles = (cl_mem)intermediateParticlesLockGuard.GetResource();

				EventWaitArray<2> computeParticleMapWaitEvents{ startIntermediateEvent, partialSumFinishedEvent };
				kernels.EnqueueReorderDynamicParticlesAndFinishHashMapKernel(clCommandQueue, particleMapBuffer, dynamicParticlesHashMap, outputParticles, intermediateParticles, dynamicParticlesBufferManager->GetParticleCount(), computeParticleMapWaitEvents, &computeParticleMapFinishedEvent());
				computeParticleMapWaitEvents.Release();

				outputParticlesLockGuard.Unlock({ (void**)&computeParticleMapFinishedEvent(), 1 });

				std::swap(intermediateParticles, outputParticles);
				std::swap(intermediateParticlesLockGuard, outputParticlesLockGuard);
			}
			else
			{
				kernels.EnqueueFillDynamicParticleMapAndFinishHashMapKernel(clCommandQueue, particleMapBuffer, dynamicParticlesHashMap, outputParticles, dynamicParticlesBufferManager->GetParticleCount(), { &partialSumFinishedEvent(), 1 }, &computeParticleMapFinishedEvent());
			}
			partialSumFinishedEvent = cl::Event();

			performanceProfiles[i].AddPendingMeasurement("Compute particle map kernel", computeParticleMapFinishedEvent());

#ifdef DEBUG_BUFFERS_GPU
			DebugDynamicParticleHashAndParticleMap(clCommandQueue, debugParticlesArray, debugHashMapArray, debugParticleMapArray, outputParticles, dynamicParticlesHashMap, particleMapBuffer);
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
	}
	void SimulationEngineGPU::InitializeStaticParticles(SceneBlueprint& scene, ParticleBufferManager& staticParticlesBufferManager)
	{
		uintMem staticParticlesCount = 0;
		cl::Buffer tempStaticParticles;

		Array<StaticParticle> staticParticles;
		scene.GenerateLayerParticles("static", staticParticles);

		if (staticParticles.Empty())
			return;

		CL_CHECK_RET(tempStaticParticles = clCreateBuffer(clContext, CL_MEM_USE_HOST_PTR, staticParticles.Count() * sizeof(StaticParticle), staticParticles.Ptr(), &ret));

		staticParticlesCount = staticParticles.Count();

		staticParticlesHashMapSize = staticParticlesCount;
		kernels.DetermineHashGroupSize(staticParticlesHashMapSize, staticParticlesHashMapGroupSize, staticParticlesHashMapSize);

#ifdef DEBUG_BUFFERS_GPU
		debugStaticParticlesArray.Resize(staticParticlesCount);
		debugStaticHashMapArray.Resize(staticParticlesHashMapSize + 1);
#endif

		CL_CHECK_RET(staticParticlesHashMap = clCreateBuffer(clContext, CL_MEM_READ_WRITE, sizeof(uint32) * (staticParticlesHashMapSize + 1), nullptr, &ret))
		staticParticlesBufferManager.Allocate(sizeof(StaticParticle), staticParticlesCount, nullptr, 1);

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

#ifdef DEBUG_BUFFERS_GPU
		DebugStaticParticles(clCommandQueue, debugStaticParticlesArray, finalStaticParticles, staticParticlesHashMapSize, particleBehaviourParameters.maxInteractionDistance);
		DebugStaticParticleHashAndParticleMap(clCommandQueue, debugStaticParticlesArray, debugStaticHashMapArray, finalStaticParticles, staticParticlesHashMap, particleBehaviourParameters.maxInteractionDistance);
#endif
		finalStaticParticlesLockGuard.Unlock({ reorderFinishedEvent() });

		//We have to wait so that 'staticParticles' memory doesn't get freed
		reorderFinishedEvent.wait();
	}
	void SimulationEngineGPU::InitializeDynamicParticles(SceneBlueprint& scene, ParticleBufferManager& dynamicParticlesBufferManager)
	{
		uintMem dynamicParticlesCount = 0;

		{
			Array<DynamicParticle> dynamicParticles;
			scene.GenerateLayerParticles("dynamic", dynamicParticles);

			if (dynamicParticles.Empty())
				return;

			dynamicParticlesBufferManager.Allocate(sizeof(DynamicParticle), dynamicParticles.Count(), dynamicParticles.Ptr(), 3);
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

		DebugDynamicParticles(clCommandQueue, debugParticlesArray, finalDynamicParticles, dynamicParticlesHashMapSize, particleBehaviourParameters.maxInteractionDistance);
		DebugDynamicParticleHashAndParticleMap(clCommandQueue, debugParticlesArray, debugHashMapArray, debugParticleMapArray, finalDynamicParticles, dynamicParticlesHashMap, particleMapBuffer);
#endif

		initialDynamicParticlesLockGuard.Unlock({ (void**)&reorderFinishedEvent(), 1 });
		finalDynamicParticlesLockGuard.Unlock({ (void**)&reorderFinishedEvent(), 1 });
	}
	void SimulationEngineGPU::InspectStaticBuffers(cl_mem particles)
	{
		debugStaticParticlesArray.Resize(staticParticlesBufferManager->GetParticleCount());
		debugStaticHashMapArray.Resize(staticParticlesHashMapSize + 1);

		CL_CALL(clFinish(clCommandQueue));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, particles, CL_TRUE, 0, staticParticlesBufferManager->GetParticleCount() * sizeof(StaticParticle), debugStaticParticlesArray.Ptr(), 0, nullptr, nullptr))
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, staticParticlesHashMap, CL_TRUE, 0, sizeof(uint32) * debugStaticHashMapArray.Count(), debugStaticHashMapArray.Ptr(), 0, nullptr, nullptr));

		__debugbreak();
	}
#ifdef DEBUG_BUFFERS_GPU
	void SimulationEngineGPU::DebugStaticParticles(cl_command_queue clCommandQueue, Array<StaticParticle>& tempBuffer, cl_mem particles, uintMem hashMapSize, float maxInteractionDistance)
	{
		CL_CALL(clFinish(clCommandQueue));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, particles, CL_TRUE, 0, tempBuffer.Count() * sizeof(StaticParticle), tempBuffer.Ptr(), 0, nullptr, nullptr))

		SimulationEngine::DebugParticles<StaticParticle>(tempBuffer, maxInteractionDistance, hashMapSize);
	}
	void SimulationEngineGPU::DebugDynamicParticles(cl_command_queue clCommandQueue, Array<DynamicParticle>& tempBuffer, cl_mem particles, uintMem hashMapSize, float maxInteractionDistance)
	{
		CL_CALL(clFinish(clCommandQueue));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, particles, CL_TRUE, 0, tempBuffer.Count() * sizeof(DynamicParticle), tempBuffer.Ptr(), 0, nullptr, nullptr))

		SimulationEngine::DebugParticles<DynamicParticle>(tempBuffer, maxInteractionDistance, hashMapSize);
	}
	void SimulationEngineGPU::DebugStaticParticleHashAndParticleMap(cl_command_queue clCommandQueue, Array<StaticParticle>& tempParticles, Array<uint32>& tempHashMap, cl_mem particles, cl_mem hashMap, float maxInteractionDistance)
	{
		CL_CALL(clFinish(clCommandQueue));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, hashMap, CL_FALSE, 0, sizeof(uint32) * tempHashMap.Count(), tempHashMap.Ptr(), 0, nullptr, nullptr));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, particles, CL_FALSE, 0, sizeof(StaticParticle) * tempParticles.Count(), tempParticles.Ptr(), 0, nullptr, nullptr));
		CL_CALL(clFinish(clCommandQueue));

		SimulationEngine::DebugHashAndParticleMap<uint32>(tempParticles, tempHashMap, maxInteractionDistance);
	}
	void SimulationEngineGPU::DebugDynamicParticleHashAndParticleMap(cl_command_queue clCommandQueue, Array<DynamicParticle>& tempParticles, Array<uint32>& tempHashMap, Array<uint32>& tempParticleMap, cl_mem particles, cl_mem hashMap, cl_mem particleMap)
	{
		CL_CALL(clFinish(clCommandQueue));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, hashMap, CL_FALSE, 0, sizeof(uint32) * tempHashMap.Count(), tempHashMap.Ptr(), 0, nullptr, nullptr));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, particleMap, CL_FALSE, 0, sizeof(uint32) * tempParticleMap.Count(), tempParticleMap.Ptr(), 0, nullptr, nullptr));
		CL_CALL(clEnqueueReadBuffer(clCommandQueue, particles, CL_FALSE, 0, sizeof(DynamicParticle) * tempParticles.Count(), tempParticles.Ptr(), 0, nullptr, nullptr));
		CL_CALL(clFinish(clCommandQueue));

		SimulationEngine::DebugHashAndParticleMap<uint32>(tempParticles, tempHashMap, tempParticleMap);
	}
#else
	void SimulationEngineGPU::DebugStaticParticles(cl_command_queue clCommandQueue, Array<StaticParticle>& tempBuffer, cl_mem particles, uintMem hashMapSize, float maxInteractionDistance)
	{
	}
	void SimulationEngineGPU::DebugDynamicParticles(cl_command_queue clCommandQueue, Array<DynamicParticle>& tempBuffer, cl_mem particles, uintMem hashMapSize, float maxInteractionDistance)
	{
	}
	void SimulationEngineGPU::DebugStaticParticleHashAndParticleMap(cl_command_queue clCommandQueue, Array<StaticParticle> tempParticles, Array<uint32> tempHashMap, cl_mem particles, cl_mem hashMap, float maxInteractionDistance)
	{
	}
	void SimulationEngineGPU::DebugDynamicParticleHashAndParticleMap(cl_command_queue clCommandQueue, Array<DynamicParticle> tempParticles, Array<uint32> tempHashMap, Array<uint32> tempParticleMap, cl_mem particles, cl_mem hashMap, cl_mem particleMap)
	{
	}
#endif
}