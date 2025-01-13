#pragma once

void PrintOpenCLError(uint code);

#pragma warning (disable: 4003)
#define CL_CALL(x, r) if ((ret = x) != CL_SUCCESS) { PrintOpenCLError(ret); return r; } else { }
#define CL_CHECK(r) if (ret != CL_SUCCESS) { PrintOpenCLError(ret); return r; } else { }

typedef struct _cl_platform_id* cl_platform_id;
typedef struct _cl_device_id* cl_device_id;
typedef struct _cl_context* cl_context;
typedef struct _cl_event* cl_event;
typedef struct _cl_mem* cl_mem;
typedef struct _cl_command_queue* cl_command_queue;
typedef struct _cl_kernel* cl_kernel;
typedef struct _cl_program* cl_program;

class OpenCLContext
{
public:	
	cl_platform_id platform;
	cl_device_id device;
	cl_context context;		
	bool supportedCLGLInterop;
	
	OpenCLContext(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
	~OpenCLContext();	

	cl_command_queue GetCommandQueue(bool profiling, bool outOfOrder);
private:	
	void PrintPlatformAndDeviceInfo();

	bool CheckForExtensions(cl_device_id device, const Set<String>& requiredExtensions);

	bool SearchPlatformAndDevice();
	bool SearchPlatformAndDeviceWithCLGLInterop(const Set<String>& requiredExtensions);
	bool SearchAnyPlatformAndDevice(const Set<String>& requiredExtensions);
	
	bool CreateContext(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
};