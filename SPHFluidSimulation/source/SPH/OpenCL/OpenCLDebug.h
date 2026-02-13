#pragma once

namespace SPH
{
	void PrintOpenCLError(uint code);
	bool CheckForExtensions(cl_device_id device, const Set<String>& requiredExtensions);
	void PrintDeviceInfo(cl_device_id device, WriteStream& stream);
	void PrintKernelInfo(cl_kernel kernel, cl_device_id device, WriteStream& stream);
	void GetDeviceVersion(cl_device_id device, uint& major, uint& minor);
}

#define CL_CALL(x, r) { cl_int ret; if ((ret = x) != CL_SUCCESS) { ::SPH::PrintOpenCLError(ret); return r; } }
#define CL_CHECK_RET(x, r) { cl_int ret; x; if (ret != CL_SUCCESS) { ::SPH::PrintOpenCLError(ret); return r; } }
#define CL_CHECK(r) if (ret != CL_SUCCESS) { ::SPH::PrintOpenCLError(ret); return r; } else { }