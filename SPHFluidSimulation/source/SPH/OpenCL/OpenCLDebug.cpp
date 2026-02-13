#include "pch.h"
#include "SPH/OpenCL/OpenCLDebug.h"

#define CaseReturnString(x) case x: return #x;

namespace SPH
{
	static StringView opencl_errstr(cl_int err)
	{
		switch (err)
		{
			CaseReturnString(CL_SUCCESS)
				CaseReturnString(CL_DEVICE_NOT_FOUND)
				CaseReturnString(CL_DEVICE_NOT_AVAILABLE)
				CaseReturnString(CL_COMPILER_NOT_AVAILABLE)
				CaseReturnString(CL_MEM_OBJECT_ALLOCATION_FAILURE)
				CaseReturnString(CL_OUT_OF_RESOURCES)
				CaseReturnString(CL_OUT_OF_HOST_MEMORY)
				CaseReturnString(CL_PROFILING_INFO_NOT_AVAILABLE)
				CaseReturnString(CL_MEM_COPY_OVERLAP)
				CaseReturnString(CL_IMAGE_FORMAT_MISMATCH)
				CaseReturnString(CL_IMAGE_FORMAT_NOT_SUPPORTED)
				CaseReturnString(CL_BUILD_PROGRAM_FAILURE)
				CaseReturnString(CL_MAP_FAILURE)
				CaseReturnString(CL_MISALIGNED_SUB_BUFFER_OFFSET)
				CaseReturnString(CL_COMPILE_PROGRAM_FAILURE)
				CaseReturnString(CL_LINKER_NOT_AVAILABLE)
				CaseReturnString(CL_LINK_PROGRAM_FAILURE)
				CaseReturnString(CL_DEVICE_PARTITION_FAILED)
				CaseReturnString(CL_KERNEL_ARG_INFO_NOT_AVAILABLE)
				CaseReturnString(CL_INVALID_VALUE)
				CaseReturnString(CL_INVALID_DEVICE_TYPE)
				CaseReturnString(CL_INVALID_PLATFORM)
				CaseReturnString(CL_INVALID_DEVICE)
				CaseReturnString(CL_INVALID_CONTEXT)
				CaseReturnString(CL_INVALID_QUEUE_PROPERTIES)
				CaseReturnString(CL_INVALID_COMMAND_QUEUE)
				CaseReturnString(CL_INVALID_HOST_PTR)
				CaseReturnString(CL_INVALID_MEM_OBJECT)
				CaseReturnString(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR)
				CaseReturnString(CL_INVALID_IMAGE_SIZE)
				CaseReturnString(CL_INVALID_SAMPLER)
				CaseReturnString(CL_INVALID_BINARY)
				CaseReturnString(CL_INVALID_BUILD_OPTIONS)
				CaseReturnString(CL_INVALID_PROGRAM)
				CaseReturnString(CL_INVALID_PROGRAM_EXECUTABLE)
				CaseReturnString(CL_INVALID_KERNEL_NAME)
				CaseReturnString(CL_INVALID_KERNEL_DEFINITION)
				CaseReturnString(CL_INVALID_KERNEL)
				CaseReturnString(CL_INVALID_ARG_INDEX)
				CaseReturnString(CL_INVALID_ARG_VALUE)
				CaseReturnString(CL_INVALID_ARG_SIZE)
				CaseReturnString(CL_INVALID_KERNEL_ARGS)
				CaseReturnString(CL_INVALID_WORK_DIMENSION)
				CaseReturnString(CL_INVALID_WORK_GROUP_SIZE)
				CaseReturnString(CL_INVALID_WORK_ITEM_SIZE)
				CaseReturnString(CL_INVALID_GLOBAL_OFFSET)
				CaseReturnString(CL_INVALID_EVENT_WAIT_LIST)
				CaseReturnString(CL_INVALID_EVENT)
				CaseReturnString(CL_INVALID_OPERATION)
				CaseReturnString(CL_INVALID_GL_OBJECT)
				CaseReturnString(CL_INVALID_BUFFER_SIZE)
				CaseReturnString(CL_INVALID_MIP_LEVEL)
				CaseReturnString(CL_INVALID_GLOBAL_WORK_SIZE)
				CaseReturnString(CL_INVALID_PROPERTY)
				CaseReturnString(CL_INVALID_IMAGE_DESCRIPTOR)
				CaseReturnString(CL_INVALID_COMPILER_OPTIONS)
				CaseReturnString(CL_INVALID_LINKER_OPTIONS)
				CaseReturnString(CL_INVALID_DEVICE_PARTITION_COUNT)
		default: return "Unknown OpenCL error code";
		}
	}

	void PrintOpenCLError(uint code)
	{
		Debug::Logger::LogFatal("OpenCL", "OpenCL function returned \"" + opencl_errstr(code) + "\"");
	}

	bool CheckForExtensions(cl_device_id device, const Set<String>& requiredExtensions)
	{
		std::string extensionsSTDString = cl::Device(device).getInfo<CL_DEVICE_EXTENSIONS>();
		StringView extensionsString{ extensionsSTDString.data(), extensionsSTDString.size() };
		Set<StringView> platformExtensions = Set<StringView>(extensionsString.Split(" "));

		bool hasAllRequiredExtensions = true;

		for (auto& requiredExtension : requiredExtensions)
			if (platformExtensions.Find(requiredExtension).IsNull())
				return false;

		return true;
	}

	static StringView DeviceTypeToString(cl_device_type type)
	{
		switch (type)
		{
		case CL_DEVICE_TYPE_CPU: return "CL_DEVICE_TYPE_CPU";
		case CL_DEVICE_TYPE_GPU: return "CL_DEVICE_TYPE_GPU";
		case CL_DEVICE_TYPE_ACCELERATOR: return "CL_DEVICE_TYPE_ACCELERATOR";
		case CL_DEVICE_TYPE_CUSTOM: return "CL_DEVICE_TYPE_CUSTOM";
		default: return "Invalid";
		}
	}
	static StringView DeviceMemCacheTypeToString(cl_device_mem_cache_type type)
	{
		switch (type)
		{
		case CL_NONE: return "CL_NONE";
		case CL_READ_ONLY_CACHE: return "CL_READ_ONLY_CACHE";
		case CL_READ_WRITE_CACHE: return "CL_READ_WRITE_CACHE";
		default: return "Invalid";
		}
	}
	static StringView DeviceLocalMemTypeToString(cl_device_local_mem_type type)
	{
		switch (type)
		{
		case CL_NONE: return "CL_NONE";
		case CL_LOCAL: return "CL_LOCAL";
		case CL_GLOBAL: return "CL_GLOBAL";
		default: return "Invalid";
		}
	}

	static void Write(WriteStream& stream, StringView string)
	{
		stream.Write(string.Ptr(), string.Count());
	}

	static String ByteCountToString(uintMem bytes)
	{
		uintMem exponent = static_cast<uintMem>(std::floor(std::log(bytes) / std::log(1024.0f)));
		float decimal = (float)bytes / std::pow(1024.0f, static_cast<float>(exponent));
		if (exponent == 0)
			return Format("{} bytes", (uintMem)decimal);
		else if ((float)(uintMem)decimal == decimal)
			return Format("{} {}B", static_cast<uintMem>(decimal), "KMGTPEZY"[exponent - 1]);
		else if (decimal < 10.0f)
			return Format("{3.1} {}B", decimal, "KMGTPEZY"[exponent - 1]);
		else if (decimal < 100.0f)
			return Format("{4.1} {}B", decimal, "KMGTPEZY"[exponent - 1]);
		else
			return
			Format("{3.1} {}B", decimal, "KMGTPEZY"[exponent - 1]);
	}

	void PrintDeviceInfo(cl_device_id device, WriteStream& stream)
	{
		uintMem paramRetSize = 0;

		Write(stream, "-------------Basic device info-------------\n");

		cl_device_type type;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_TYPE, sizeof(type), &type, nullptr));
		Write(stream, "Type: " + DeviceTypeToString(type) + "\n");

		uint vendorID;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_VENDOR_ID, sizeof(vendorID), &vendorID, nullptr));
		Write(stream, Format("Vendor ID: {}\n", vendorID));

		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_VENDOR, 0, nullptr, &paramRetSize));
		String vendor{ paramRetSize - 1 };
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_VENDOR, paramRetSize, vendor.Ptr(), nullptr));
		Write(stream, Format("Vendor: {}\n", vendor));

		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_NAME, 0, nullptr, &paramRetSize));
		String name{ paramRetSize - 1 };
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_NAME, paramRetSize, name.Ptr(), nullptr));
		Write(stream, Format("Name: {}\n", name));

		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_VERSION, 0, nullptr, &paramRetSize));
		String version{ paramRetSize - 1 };
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_VERSION, paramRetSize, version.Ptr(), nullptr));
		Write(stream, Format("Version: {}\n", version));

		Write(stream, "-------------Device compute capabilites info-------------\n");

		uint maxComputeUnits;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(maxComputeUnits), &maxComputeUnits, nullptr));
		Write(stream, Format("Max compute units: {}\n", maxComputeUnits));

		uint maxWorkItemDimensions;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(maxWorkItemDimensions), &maxWorkItemDimensions, nullptr));
		Write(stream, Format("Max work item dimensions: {}\n", maxWorkItemDimensions));

		Array<uintMem> maxWorkItemSizes(maxWorkItemDimensions);
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(uintMem) * maxWorkItemSizes.Count(), maxWorkItemSizes.Ptr(), nullptr));
		Write(stream, "Max work item sizes: ");
		for (uintMem i = 0; i < maxWorkItemSizes.Count(); ++i)
			Write(stream, String::Parse(maxWorkItemSizes[i]) + (i == maxWorkItemSizes.Count() - 1 ? StringView("\n") : StringView(", ")));

		uintMem maxWorkGroupSize;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(maxWorkGroupSize), &maxWorkGroupSize, nullptr));
		Write(stream, Format("Max work group size: {}\n", maxWorkGroupSize));

		Write(stream, "-------------Device memory info-------------\n");

		uint hostUnifiedMemory;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_HOST_UNIFIED_MEMORY, sizeof(hostUnifiedMemory), &hostUnifiedMemory, nullptr));
		Write(stream, "Host unified memory: " + (hostUnifiedMemory == 1 ? StringView("true") : StringView("false")) + "\n");

		uint addressBits;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_ADDRESS_BITS, sizeof(addressBits), &addressBits, nullptr));
		Write(stream, Format("Address bits: {}\n", addressBits));

		cl_device_mem_cache_type globalMemCacheType;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_CACHE_TYPE, sizeof(globalMemCacheType), &globalMemCacheType, nullptr));
		Write(stream, Format("Global mem cache type: {}\n", DeviceMemCacheTypeToString(globalMemCacheType)));

		uint globalMemCachelineSize;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE, sizeof(globalMemCachelineSize), &globalMemCachelineSize, nullptr));
		Write(stream, Format("Global mem cacheline size: {}\n", ByteCountToString(globalMemCachelineSize)));

		uint64 globalMemCacheSize;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, sizeof(globalMemCacheSize), &globalMemCacheSize, nullptr));
		Write(stream, Format("Global mem cache size: {}\n", ByteCountToString(globalMemCacheSize)));

		uint64 globalMemSize;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(globalMemSize), &globalMemSize, nullptr));
		Write(stream, Format("Global mem size: {}\n", ByteCountToString(globalMemSize)));

		cl_device_local_mem_type localMemType;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_TYPE, sizeof(localMemType), &localMemType, nullptr));
		Write(stream, Format("Local mem type: {}\n", DeviceLocalMemTypeToString(localMemType)));

		uint64 localMemSize;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(localMemSize), &localMemSize, nullptr));
		Write(stream, Format("Local mem size: {}\n", ByteCountToString(localMemSize)));

		Write(stream, "-------------Other device info-------------\n");

		uint64 maxDeviceQueues;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_MAX_ON_DEVICE_EVENTS, sizeof(maxDeviceQueues), &maxDeviceQueues, nullptr));
		Write(stream, Format("Maximum device queues: {}\n", maxDeviceQueues));

		uintMem profilingTimerResoultion;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_PROFILING_TIMER_RESOLUTION, sizeof(profilingTimerResoultion), &profilingTimerResoultion, nullptr));
		Write(stream, Format("Profiling timer resoultion: {}ns\n", profilingTimerResoultion));

		uint preferredInteropUserSync;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_PREFERRED_INTEROP_USER_SYNC, sizeof(preferredInteropUserSync), &preferredInteropUserSync, nullptr));
		Write(stream, Format("Preferred interop user sync: {}\n", (preferredInteropUserSync == 1 ? StringView("true") : StringView("false"))));

		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_OPENCL_C_VERSION, 0, nullptr, &paramRetSize));
		String openCLCVersion{ paramRetSize - 1 };
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_OPENCL_C_VERSION, paramRetSize, openCLCVersion.Ptr(), nullptr));
		Write(stream, Format("OpenCL C version: {}\n", openCLCVersion));

		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 0, nullptr, &paramRetSize));
		String extensions{ paramRetSize - 1 };
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, paramRetSize, extensions.Ptr(), nullptr));
		Write(stream, Format("Extensions: {}\n", extensions));

		Write(stream, "\n");
	}
	void PrintKernelInfo(cl_kernel kernel, cl_device_id device, WriteStream& stream)
	{
		uintMem paramRetSize = 0;

		CL_CALL(clGetKernelInfo(kernel, CL_KERNEL_FUNCTION_NAME, 0, nullptr, &paramRetSize));
		String functionName{ paramRetSize - 1 };
		CL_CALL(clGetKernelInfo(kernel, CL_KERNEL_FUNCTION_NAME, paramRetSize, functionName.Ptr(), nullptr));

		uint argCount;
		CL_CALL(clGetKernelInfo(kernel, CL_KERNEL_NUM_ARGS, sizeof(uint), &argCount, nullptr));

		struct Argument
		{
			String type;
			String name;
		};

		Array<Argument> arguments;
		for (uint i = 0; i < argCount; ++i)
		{
			CL_CALL(clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_TYPE_NAME, 0, nullptr, &paramRetSize));
			String argumentType{ paramRetSize - 1 };
			CL_CALL(clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_TYPE_NAME, paramRetSize, argumentType.Ptr(), nullptr));

			CL_CALL(clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_NAME, 0, nullptr, &paramRetSize));
			String argumentName{ paramRetSize - 1 };
			CL_CALL(clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_NAME, paramRetSize, argumentName.Ptr(), nullptr));

			arguments.AddBack(argumentType, argumentName);
		}

		//This returns a error
		//uintMem globalWorkSize[3];
		//CL_CALL(clGetKernelWorkGroupInfo(kernel, device, CL_KERNEL_GLOBAL_WORK_SIZE, sizeof(uintMem) * 3, globalWorkSize, nullptr));

		uintMem workGroupSize;
		CL_CALL(clGetKernelWorkGroupInfo(kernel, device, CL_KERNEL_WORK_GROUP_SIZE, sizeof(uintMem), &workGroupSize, nullptr));

		uintMem compileWorkGroupSize[3];
		CL_CALL(clGetKernelWorkGroupInfo(kernel, device, CL_KERNEL_COMPILE_WORK_GROUP_SIZE, sizeof(uintMem) * 3, compileWorkGroupSize, nullptr));

		uint64 localMemSize;
		CL_CALL(clGetKernelWorkGroupInfo(kernel, device, CL_KERNEL_LOCAL_MEM_SIZE, sizeof(uint64), &localMemSize, nullptr));

		uintMem preferredWorkGroupSizeMultiple;
		CL_CALL(clGetKernelWorkGroupInfo(kernel, device, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(uintMem), &preferredWorkGroupSizeMultiple, nullptr));

		uint64 privateMemSize;
		CL_CALL(clGetKernelWorkGroupInfo(kernel, device, CL_KERNEL_PRIVATE_MEM_SIZE, sizeof(uint64), &privateMemSize, nullptr));

		Write(stream, Format("Function name: \"{}\"\n", functionName));
		Write(stream, Format("Argument count: {}\n", argCount));
		Write(stream, "Arguments: ");

		for (uintMem i = 0; i < arguments.Count(); ++i)
			Write(stream, arguments[i].type + " " + arguments[i].name + (i == arguments.Count() - 1 ? StringView("\n") : StringView(", ")));

		//Write(stream, "Global work size: " + StringParsing::Convert(globalWorkSize[0]) + ", " + StringParsing::Convert(globalWorkSize[1]) + ", " + StringParsing::Convert(globalWorkSize[2]) + "\n");
		Write(stream, Format("Work group size: {}\n", workGroupSize));
		Write(stream, Format("Compiled work group size: {}, {}, {}\n", compileWorkGroupSize[0], compileWorkGroupSize[1], compileWorkGroupSize[2]));
		Write(stream, Format("Local memory size: {}\n", localMemSize));
		Write(stream, Format("Preferred work group size multiple: {}\n", preferredWorkGroupSizeMultiple));
		Write(stream, Format("Private memory size: {}\n", privateMemSize));
		Write(stream, "\n");
	}

	void GetDeviceVersion(cl_device_id device, uint& major, uint& minor)
	{
		major = 0;
		minor = 0;

		uintMem size = 0;
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_VERSION, 0, nullptr, &size));
		String versionString{ size - 1 };
		CL_CALL(clGetDeviceInfo(device, CL_DEVICE_VERSION, size, versionString.Ptr(), nullptr));

		auto words = versionString.Split(" ");

		auto versionSubStrings = words[1].Split(".", 1);
		versionSubStrings[0].ConvertToInteger(major);
		versionSubStrings[1].ConvertToInteger(minor);
	}
}