#include "pch.h"
#include "OpenCLContext.h"
#include "CL/cl_gl.h"

#include "GL/glew.h"
#include "GL/wglew.h"

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

void PrintOpenCLError(cl_uint code)
{
    Debug::Logger::LogFatal("OpenCL", "OpenCL function returned \"" + opencl_errstr(code) + "\"");
}

#define CL_CALL(x) if ((ret = x) != CL_SUCCESS) { PrintOpenCLError(ret); return; }
#define CL_CALL(x, r) if ((ret = x) != CL_SUCCESS) { PrintOpenCLError(ret); return r; }
#define CL_CHECK() if (ret != CL_SUCCESS) { PrintOpenCLError(ret); return; }
#define CL_CHECK(r) if (ret != CL_SUCCESS) { PrintOpenCLError(ret); return r; }

OpenCLContext::OpenCLContext(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
{    
    cl_int ret;

    std::vector<cl::Platform> platforms;
    CL_CALL(cl::Platform::get(&platforms));

    Console::WriteLine("Available OpenCL platforms include:");
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
            cl_version version = device.getInfo<CL_DEVICE_NUMERIC_VERSION>();
            
            Console::WriteLine("\t" + StringView(name.c_str(), name.size()) + "(" + StringView(vendor.data(), vendor.size()) + ") version " + StringParsing::Convert(CL_VERSION_MAJOR(version)) + "." + StringParsing::Convert(CL_VERSION_MINOR(version)) + "." + StringParsing::Convert(CL_VERSION_PATCH(version)));
        }
    }
    

    //for (auto& platform : platforms)
    //{
    //    clGetGLContextInfoKHR_fn pclGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn)
    //        clGetExtensionFunctionAddressForPlatform(platform(), "clGetGLContextInfoKHR");
    //
    //    if (!pclGetGLContextInfoKHR)
    //        continue;
    //
    //    cl_context_properties properties[] =
    //    {
    //    CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
    //    CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
    //    CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
    //    0
    //    };
    //
    //    cl_device_id device_id;
    //    size_t value_size_ret;
    //
    //    if (CL_SUCCESS != pclGetGLContextInfoKHR(properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR
    //        , sizeof(cl_device_id), &device_id, &value_size_ret))
    //        continue;
    //
    //    if (0 == value_size_ret)
    //        continue;
    //
    //    this->device = cl::Device(device_id);
    //    this->platform = std::move(platform);
    //}
    
    if (!SelectPlatform()) return;
    
    if (!SelectDevice(graphicsContext)) return;

    if (!CreateContext(graphicsContext)) return;

    auto name = device.getInfo<CL_DEVICE_NAME>();
    auto version = device.getInfo<CL_DEVICE_OPENCL_C_VERSION>();
    Debug::Logger::LogInfo("Client", "Successfully initialized " + StringView(version.data(), version.size()) + " device name " + StringView(name.data(), name.size()));

    //auto deviceExtensions = device.getInfo<CL_DEVICE_EXTENSIONS>();
    //Debug::Logger::LogInfo("Client", "Available OpenCL extensions: \n" + StringView(deviceExtensions.data(), deviceExtensions.size()));
}

OpenCLContext::~OpenCLContext()
{
}

bool OpenCLContext::SelectPlatform()
{
    cl_int ret;

    std::vector<cl::Platform> platforms;
    CL_CALL(cl::Platform::get(&platforms), false);

    for (auto& platform : platforms)
    {
        std::string name = platform.getInfo<CL_PLATFORM_NAME>(&ret);
        CL_CHECK(false);

        if (name.find("NVIDIA") != std::string::npos ||
            name.find("AMD") != std::string::npos ||
            name.find("MESA") != std::string::npos ||
            name.find("INTEL") != std::string::npos ||
            name.find("APPLE") != std::string::npos)
        {
            this->platform = std::move(platform);
            return true;
        }
    }

    Debug::Logger::LogFatal("Client", "No OpenCL platform found");       

    return false;
}

bool OpenCLContext::SelectDevice(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
{
    cl_int ret;

    Array<String> extensions{
        "cl_khr_gl_sharing",
        "cl_khr_global_int32_base_atomics",
        "cl_khr_gl_event"
    };    

    std::vector<cl::Device> devices;
    CL_CALL(platform.getDevices(CL_DEVICE_TYPE_GPU, &devices), false);          

    bool found = false;
    for (auto& device : devices)
    {
        std::string extensionsString = device.getInfo<CL_DEVICE_EXTENSIONS>(&ret);
        CL_CHECK(false);

        clGetGLContextInfoKHR_fn pclGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn)
            clGetExtensionFunctionAddressForPlatform(platform(), "clGetGLContextInfoKHR");

        if (!pclGetGLContextInfoKHR)
            continue;

        cl_context_properties properties[] =
        {
        CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
        CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
        CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
        0
        };

        cl_device_id device_id;
        size_t value_size_ret;

        if (CL_SUCCESS != pclGetGLContextInfoKHR(properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR
            , sizeof(cl_device_id), &device_id, &value_size_ret))
            continue;

        if (0 == value_size_ret)
            continue;

        if (device() != device_id)
            continue;
        
        Set<String> extensionsSet = (ArrayView<String>)StringParsing::Split(StringView(extensionsString.data(), extensionsString.size()), ' ');

        bool supported = true;
        for (auto& extension : extensions)
            if (extensionsSet.Find(extension).IsNull())
            {                
                supported = false;
                break;                
            }
        
        if (supported)
        {
            this->device = std::move(device);
            found = true;
            break;
        }
    }

    if (!found)
    {
        Debug::Logger::LogFatal("Client", "No device found that supports all the extensions");
        return false;
    }
                    
    return true;
}

bool OpenCLContext::CreateContext(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext)
{
    cl_int ret;

    //SDL_SysWMinfo info;
    //SDL_VERSION(&info.version);
    //
    //auto window = (SDL_Window*)graphicsContext.GetActiveWindowSDLHandle();
    //
    //if (window == nullptr)
    //{
    //    Debug::Logger::LogFatal("Client", "There is no active window on the graphics context");
    //}
    //
    //if (SDL_GetWindowWMInfo(window, &info) != SDL_TRUE)
    //{
    //    Debug::Logger::LogError("SDL", "Failed to get WM info");
    //    return false;
    //}    
    clGetGLContextInfoKHR_fn pclGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn)clGetExtensionFunctionAddressForPlatform(platform(), "clGetGLContextInfoKHR");

    cl_context_properties properties[] =
    {
      CL_GL_CONTEXT_KHR,   (cl_context_properties)wglGetCurrentContext(),
      CL_WGL_HDC_KHR,      (cl_context_properties)wglGetCurrentDC(),
      CL_CONTEXT_PLATFORM, (cl_context_properties)platform(), // OpenCL platform object
      0
    };

    context = cl::Context(device, properties, nullptr, nullptr, &ret);    
    CL_CHECK(false);

    return true;
}
