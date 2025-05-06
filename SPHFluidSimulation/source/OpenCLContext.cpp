#include "pch.h"
#include "OpenCLContext.h"
#include "OpenCLDebug.h"
#include "CL/opencl.hpp"
#include "CL/cl_gl.h"

#include "GL/glew.h"
#include "GL/wglew.h"

#include "SPH/System/SystemGPU.h"

#define CaseReturnString(x) case x: return #x;

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

static void PrintOpenCLError(uint code)
{
    Debug::Logger::LogFatal("OpenCL", "OpenCL function returned \"" + opencl_errstr(code) + "\"");
}

static void PrintOpenCLWarning(cl_uint code)
{
    Debug::Logger::LogWarning("OpenCL", "OpenCL function returned \"" + opencl_errstr(code) + "\"");
}

#define CL_CALL(x) if ((ret = x) != CL_SUCCESS) { PrintOpenCLError(ret); return; }
#define CL_CALL(x, r) if ((ret = x) != CL_SUCCESS) { PrintOpenCLError(ret); return r; }
#define CL_CHECK() if (ret != CL_SUCCESS) { PrintOpenCLError(ret); return; }
#define CL_CHECK(r) if (ret != CL_SUCCESS) { PrintOpenCLError(ret); return r; }

OpenCLContext::OpenCLContext(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext) :
    supportedCLGLInterop(true)
{    
    PrintPlatformAndDeviceInfo();

    if (!SearchPlatformAndDevice()) return;    

    if (!CreateContext(graphicsContext)) return;

    auto name = cl::Device(device).getInfo<CL_DEVICE_NAME>();
    auto version = cl::Device(device).getInfo<CL_DEVICE_OPENCL_C_VERSION>();
    Debug::Logger::LogInfo("Client", "Successfully initialized " + StringView(version.data(), version.size()) + " device name " + StringView(name.data(), name.size()));

    //auto deviceExtensions = device.getInfo<CL_DEVICE_EXTENSIONS>();
    //Debug::Logger::LogInfo("Client", "Available OpenCL extensions: \n" + StringView(deviceExtensions.data(), deviceExtensions.size()));
}

OpenCLContext::~OpenCLContext()
{
}

cl_command_queue OpenCLContext::GetCommandQueue(bool profiling, bool outOfOrder)
{
    cl_int ret;

    cl_command_queue_properties propertiesBitfield = 0;
    if (profiling) propertiesBitfield |= CL_QUEUE_PROFILING_ENABLE;
    if (outOfOrder) propertiesBitfield |= CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;

    cl_queue_properties properties[]{
        CL_QUEUE_PROPERTIES, propertiesBitfield,
        0
    };

    auto queue = clCreateCommandQueueWithProperties(context, device, properties, &ret);
    CL_CHECK(nullptr);
    return queue;
}

void OpenCLContext::PrintPlatformAndDeviceInfo()
{
    cl_int ret;

    std::vector<cl::Platform> platforms;
    CL_CALL(cl::Platform::get(&platforms));

    Console::WriteLine("\nAvailable OpenCL platforms include:");
    String platformsString;
    for (auto& platform : platforms)
    {
        std::string name = platform.getInfo<CL_PLATFORM_NAME>();
        std::string vendor = platform.getInfo<CL_PLATFORM_VENDOR>();
        Console::WriteLine(StringView(name.data(), name.size()) + "(" + StringView(vendor.data(), vendor.size()) + ")");

        std::vector<cl::Device> devices;
        CL_CALL(platform.getDevices(CL_DEVICE_TYPE_ALL, &devices));

        for (auto& device : devices)
        {
            cl_device_type type = device.getInfo<CL_DEVICE_TYPE>();
            String typeString;

            switch (type)
            {
            case CL_DEVICE_TYPE_CPU: typeString = "CL_DEVICE_TYPE_CPU"; break;
            case CL_DEVICE_TYPE_GPU: typeString = "CL_DEVICE_TYPE_GPU"; break;
            case CL_DEVICE_TYPE_ACCELERATOR: typeString = "CL_DEVICE_TYPE_GPU"; break;
            default: typeString = "invalid"; break;
            }

            std::string name = device.getInfo<CL_DEVICE_NAME>();
            std::string vendor = device.getInfo<CL_DEVICE_VENDOR>();

            Console::WriteLine("\t" + StringView(name.c_str(), name.size()) + " (type: " + typeString + ")");
        }
    }


    Console::Write("\n");
}

bool OpenCLContext::CheckForExtensions(cl_device_id device, const Set<String>& requiredExtensions)
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

bool OpenCLContext::SearchPlatformAndDevice()
{
    Set<String> extensions = { "cl_khr_global_int32_base_atomics" };

    if (SearchPlatformAndDeviceWithCLGLInterop(extensions))
    {
        Debug::Logger::LogInfo("Client", "Found a suitable processor with OpenCL-OpenGL interop");
        return true;
    }

    supportedCLGLInterop = false;

    Debug::Logger::LogInfo("Client", "The computer might not support OpenCL-OpenGL interop. Looking for suitable processor without OpenCL-OpenGL interop but which support the required extensions. Note OpenCL-OpenGL interop is not required but it should speed up the application.");
    
    if (SearchAnyPlatformAndDevice(extensions))
        return true;

    return false;
}

bool OpenCLContext::SearchPlatformAndDeviceWithCLGLInterop(const Set<String>& requiredExtensions)
{
    cl_int ret;        

    HGLRC wglCurrentContext = wglGetCurrentContext();
    HDC wglCurrentDC = wglGetCurrentDC();    

    std::vector<cl::Platform> platforms;
    if ((ret = cl::Platform::get(&platforms)) != CL_SUCCESS)
        Debug::Logger::LogFatal("OpenCL", "clGetPlatformIDs failed and returned " + opencl_errstr(ret));
        
    cl_device_id device_id;
    size_t value_size_ret;

    for (auto& platform : platforms)
    {                        
        std::string platformNameSTD = platform.getInfo<CL_PLATFORM_NAME>();
        StringView platformName{ platformNameSTD.data(), platformNameSTD.size() };

        clGetGLContextInfoKHR_fn pclGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn)clGetExtensionFunctionAddressForPlatform(platform(), "clGetGLContextInfoKHR");

        if (!pclGetGLContextInfoKHR)
        {
            Debug::Logger::LogWarning("Client", "clGetGLContextInfoKHR function not found for platform \"" + platformName + "\". Skipping platform");
            return false;
        }

        const cl_context_properties properties[] =
        {
            CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
            CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
            CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
            0
        };

        if ((ret = pclGetGLContextInfoKHR(properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &device_id, &value_size_ret)) != CL_SUCCESS)
        {
            Debug::Logger::LogWarning("OpenCL", "clGetGLContextInfoKHR returned \"" + opencl_errstr(ret) + "\" on platform \"" + platformName + "\". Skipping platform");
            continue;
        }

        if (value_size_ret == 0)
        {
            Debug::Logger::LogWarning("OpenCL", "pclGetGLContextInfoKHR returned a value with size 0 on platform \"" + platformName + "\". Skipping platform");
            continue;
        }

        cl::Device foundDevice = cl::Device(device_id);

        std::string deviceNameSTD = foundDevice.getInfo<CL_DEVICE_NAME>();
        StringView deviceName{ deviceNameSTD.data(), deviceNameSTD.size() };

        if (!CheckForExtensions(foundDevice(), requiredExtensions))
        {
            Debug::Logger::LogInfo("OpenCL", "Skipping processor named \"" + deviceName + "\" on platform \"" + platformName + "\" because it doesn't have all required extensions");
            continue;
        }

        this->device = std::move(foundDevice());
        this->platform = std::move(platform());

        return true;
    }

    Debug::Logger::LogWarning("Client", "No processor with OpenCL-OpenGL interop found that supports the required extensions");


    return false;
}

bool OpenCLContext::SearchAnyPlatformAndDevice(const Set<String>& requiredExtensions)
{
    cl_int ret;

    std::vector<cl::Platform> platforms;
    CL_CALL(cl::Platform::get(&platforms), false);

    for (auto& platform : platforms)
    {
        std::string platformNameSTD = platform.getInfo<CL_PLATFORM_NAME>();
        StringView platformName{ platformNameSTD.data(), platformNameSTD.size() };

        std::vector<cl::Device> devices;
        CL_CALL(platform.getDevices(CL_DEVICE_TYPE_GPU, &devices), false);

        for (auto& device : devices)
        {
            std::string deviceNameSTD = device.getInfo<CL_DEVICE_NAME>();
            StringView deviceName{ deviceNameSTD.data(), deviceNameSTD.size() };

            if (!CheckForExtensions(device(), requiredExtensions))
            {
                Debug::Logger::LogInfo("OpenCL", "Skipping processor named \"" + deviceName + "\" on platform \"" + platformName + "\" because it doesn't have all required extensions");                
                continue;
            }

            this->device = std::move(device());
            this->platform = std::move(platform());

            return true;
        }
    }

    Debug::Logger::LogWarning("Client", "No processor found that supports the required extensions");

    return false;
}

bool OpenCLContext::CreateContext(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
{
    cl_int ret;
        
    cl_context_properties properties[] =
    {
      CL_GL_CONTEXT_KHR,   (cl_context_properties)wglGetCurrentContext(),
      CL_WGL_HDC_KHR,      (cl_context_properties)wglGetCurrentDC(),
      CL_CONTEXT_PLATFORM, (cl_context_properties)platform, // OpenCL platform object
      0
    };

    auto errorCallback = [](const char* errInfo, const void* privateInfo, size_t cv, void* userData) {
        Debug::Logger::LogFatal("OpenCL", "OpenCL error callback:\n " + StringView(errInfo, strlen(errInfo)));
        };

    context = clCreateContext(properties, 1, &device, errorCallback, nullptr, &ret);
    CL_CHECK(false);

    return true;
}
