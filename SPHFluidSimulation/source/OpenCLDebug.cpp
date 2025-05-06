#include "pch.h"
#include "OpenCLDebug.h"

#define CaseReturnString(x) case x: return #x;

StringView opencl_errstr(cl_int err)
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

void PrintOpenCLWarning(cl_uint code)
{
    Debug::Logger::LogWarning("OpenCL", "OpenCL function returned \"" + opencl_errstr(code) + "\"");
}

bool CheckForExtensions(cl_device_id device, const Set<String>& requiredExtensions)
{
    std::string extensionsSTDString = cl::Device(device).getInfo<CL_DEVICE_EXTENSIONS>();
    StringView extensionsString{ extensionsSTDString.data(), extensionsSTDString.size() };
    Set<String> platformExtensions = Set<String>(Array<String>(StringParsing::Split(extensionsString, ' ')));

    bool hasAllRequiredExtensions = true;

    for (auto& requiredExtension : requiredExtensions)
        if (platformExtensions.Find(requiredExtension).IsNull())
            return false;

    return true;
}

static StringView ToString(cl_device_type type)
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

static void Write(WriteStream& stream, StringView string)
{
	stream.Write(string.Ptr(), string.Count());
}

void PrintDeviceInfo(cl_device_id device, WriteStream& stream)
{
	ConsoleOutputStream cos;

	uintMem paramRetSize = 0;

	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_NAME, 0, nullptr, &paramRetSize));
	String name{ paramRetSize - 1};
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_NAME, paramRetSize, name.Ptr(), nullptr));

	cl_device_type type;
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_TYPE, sizeof(type), &type, nullptr));
	
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_VERSION, 0, nullptr, &paramRetSize));
	String version{ paramRetSize - 1 };
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_VERSION, paramRetSize, version.Ptr(), nullptr));

	uint maxComputeUnits;
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(maxComputeUnits), &maxComputeUnits, nullptr));

	uint maxWorkItemDimensions;
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(maxWorkItemDimensions), &maxWorkItemDimensions, nullptr));

	Array<uintMem> maxWorkItemSizes(maxWorkItemDimensions);
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(uintMem) * maxWorkItemSizes.Count(), maxWorkItemSizes.Ptr(), nullptr));

	uintMem maxWorkGroupSize;
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(maxWorkGroupSize), &maxWorkGroupSize, nullptr));

	uint addressBits;
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_ADDRESS_BITS, sizeof(addressBits), &addressBits, nullptr));

	uint globalMemCachelineSize;
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE, sizeof(globalMemCachelineSize), &globalMemCachelineSize, nullptr));

	uint64 globalMemCacheSize;
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, sizeof(globalMemCacheSize), &globalMemCacheSize, nullptr));

	uint64 globalMemSize;
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(globalMemSize), &globalMemSize, nullptr));

	uint64 localMemSize;
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(localMemSize), &localMemSize, nullptr));

	uint hostUnifiedMemory;
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_HOST_UNIFIED_MEMORY, sizeof(hostUnifiedMemory), &hostUnifiedMemory, nullptr));

	uintMem profilingTimerResoultion;
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_PROFILING_TIMER_RESOLUTION, sizeof(profilingTimerResoultion), &profilingTimerResoultion, nullptr));

	uint preferredInteropUserSync;
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_PREFERRED_INTEROP_USER_SYNC, sizeof(preferredInteropUserSync), &preferredInteropUserSync, nullptr));

	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_OPENCL_C_VERSION, 0, nullptr, &paramRetSize));
	String openCLCVersion{ paramRetSize - 1 };
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_OPENCL_C_VERSION, paramRetSize, openCLCVersion.Ptr(), nullptr));

	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 0, nullptr, &paramRetSize));
	String extensions{ paramRetSize - 1};
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, paramRetSize, extensions.Ptr(), nullptr));

	//uint nonUniformWorkGroupSupport;
	//CL_CALL(clGetDeviceInfo(device, CL_DEVICE_NON_UNIFORM_WORK_GROUP_SUPPORT, sizeof(nonUniformWorkGroupSupport), &nonUniformWorkGroupSupport, nullptr));
	//
	//uintMem preferredWorkGroupSizeMultiple;
	//CL_CALL(clGetDeviceInfo(device, CL_DEVICE_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(preferredWorkGroupSizeMultiple), &preferredWorkGroupSizeMultiple, nullptr));

	Write(stream, "Name: " + name + "\n");
	Write(stream, "Type: " + ToString(type) + "\n");
	Write(stream, "Version: " + version + "\n");
	Write(stream, "Max compute units: " + StringParsing::Convert(maxComputeUnits) + "\n");
	Write(stream, "Max work item dimensions: " + StringParsing::Convert(maxWorkItemDimensions) + "\n");
	Write(stream, "Max work item sizes: ");
	for (uintMem i = 0; i < maxWorkItemSizes.Count(); ++i)
		Write(stream, StringParsing::Convert(maxWorkItemSizes[i]) + (i == maxWorkItemSizes.Count() - 1 ? StringView("\n") : StringView(", ")));
	Write(stream, "Max work group size: " + StringParsing::Convert(maxWorkGroupSize) + "\n");
	Write(stream, "Address bits: " + StringParsing::Convert(addressBits) + "\n");
	Write(stream, "Global mem cacheline size: " + StringParsing::Convert(globalMemCachelineSize) + "\n");
	Write(stream, "Global mem cache size: " + StringParsing::Convert(globalMemCacheSize) + "\n");
	Write(stream, "Global mem size: " + StringParsing::Convert(globalMemSize) + "\n");
	Write(stream, "Local mem size: " + StringParsing::Convert(localMemSize) + "\n");
	Write(stream, "Host unified memory: " + (hostUnifiedMemory == 1 ? StringView("true") : StringView("false")) + "\n");
	Write(stream, "Profiling timer resoultion: " + StringParsing::Convert(profilingTimerResoultion) + "ns" + "\n");
	Write(stream, "Preferred interop user sync: " + (preferredInteropUserSync == 1 ? StringView("true") : StringView("false")) + "\n");
	//Write(stream, "Non uniform work group support: " + (nonUniformWorkGroupSupport == 1 ? StringView("true") : StringView("false")) + "\n");
	//Write(stream, "Preferred work group size multiple: " + StringParsing::Convert(preferredWorkGroupSizeMultiple) + "\n");
	Write(stream, "OpenCL C version: " + openCLCVersion + "\n");
	Write(stream, "Extensions: " + extensions + "\n");
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
	for (uintMem i = 0; i < argCount; ++i)
	{
		CL_CALL(clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_TYPE_NAME, 0, nullptr, &paramRetSize));
		String argumentType{ paramRetSize - 1 };
		CL_CALL(clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_TYPE_NAME, paramRetSize, argumentType.Ptr(), nullptr));

		CL_CALL(clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_NAME, 0, nullptr, &paramRetSize));
		String argumentName{ paramRetSize - 1};
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

	Write(stream, "Function name: \"" + functionName + "\"\n");
	Write(stream, "Argument count: " + StringParsing::Convert(argCount) + "\n");
	Write(stream, "Arguments: ");
	
	for (uintMem i = 0; i < arguments.Count(); ++i)
		Write(stream, arguments[i].type + " " + arguments[i].name + (i == arguments.Count() - 1 ? StringView("\n") : StringView(", ")));

	//Write(stream, "Global work size: " + StringParsing::Convert(globalWorkSize[0]) + ", " + StringParsing::Convert(globalWorkSize[1]) + ", " + StringParsing::Convert(globalWorkSize[2]) + "\n");
	Write(stream, "Work group size: " + StringParsing::Convert(workGroupSize) + "\n");
	Write(stream, "Compiled work group size: " + StringParsing::Convert(compileWorkGroupSize[0]) + ", " + StringParsing::Convert(compileWorkGroupSize[1]) + ", " + StringParsing::Convert(compileWorkGroupSize[2]) + "\n");
	Write(stream, "Local memory size: " + StringParsing::Convert(localMemSize) + "\n");
	Write(stream, "Preferred work group size multiple: " + StringParsing::Convert(preferredWorkGroupSizeMultiple) + "\n");
	Write(stream, "Private memory size: " + StringParsing::Convert(privateMemSize) + "\n");
	Write(stream, "\n");
}

void GetDeviceVersion(cl_device_id device, uintMem& major, uintMem& minor)
{
	major = 0;
	minor = 0;

	uintMem size = 0;
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_VERSION, 0, nullptr, &size));
	String versionString{ size - 1 };
	CL_CALL(clGetDeviceInfo(device, CL_DEVICE_VERSION, size, versionString.Ptr(), nullptr));
	
	auto words = StringParsing::Split(versionString, ' ');
	String majorVersionString, minorVersionString;
	StringParsing::SplitAtFirst(words[1], majorVersionString, minorVersionString, '.');

	StringParsing::Convert((StringView)majorVersionString, major);
	StringParsing::Convert((StringView)minorVersionString, minor);
}
