#pragma once
#include "CL/opencl.hpp"

void PrintOpenCLError(cl_uint code);

#pragma warning (disable: 4003)
#define CL_CALL(x, r) if ((ret = x) != CL_SUCCESS) { PrintOpenCLError(ret); return r; } else { }
#define CL_CHECK(r) if (ret != CL_SUCCESS) { PrintOpenCLError(ret); return r; } else { }

class OpenCLContext
{
public:
	cl::Platform platform;
	cl::Device device;
	cl::Context context;		
	bool supportedCLGLInterop;
	
	OpenCLContext(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
	~OpenCLContext();	
private:	
	void PrintPlatformAndDeviceInfo();

	bool CheckForExtensions(const cl::Device& device, const Set<String>& requiredExtensions);

	bool SearchPlatformAndDevice();
	bool SearchPlatformAndDeviceWithCLGLInterop(const Set<String>& requiredExtensions);
	bool SearchAnyPlatformAndDevice(const Set<String>& requiredExtensions);
	
	bool CreateContext(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
};