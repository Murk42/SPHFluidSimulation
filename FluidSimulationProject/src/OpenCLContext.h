#pragma once

using namespace Blaze;

void PrintOpenCLError(cl_uint code);

#define CL_CALL(x) if ((ret = x) != CL_SUCCESS) { PrintOpenCLError(ret); return; }
#define CL_CALL(x, r) if ((ret = x) != CL_SUCCESS) { PrintOpenCLError(ret); return r; }
#define CL_CHECK() if (ret != CL_SUCCESS) { PrintOpenCLError(ret); return; }
#define CL_CHECK(r) if (ret != CL_SUCCESS) { PrintOpenCLError(ret); return r; }

class OpenCLContext
{
public:
	cl::Platform platform;
	cl::Device device;
	cl::Context context;	
	
	OpenCLContext(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
	~OpenCLContext();	
private:	
	
	bool SelectPlatform();	
	bool SelectDevice(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
	bool CreateContext(Graphics::OpenGL::GraphicsContext_OpenGL& graphicsContext);
};