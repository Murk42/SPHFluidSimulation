#pragma once

void PrintOpenCLError(uint code);

#pragma warning (disable: 4003)
#define CL_CALL(x, r) if ((ret = x) != CL_SUCCESS) { PrintOpenCLError(ret); return r; } else { }
#define CL_CHECK(r) if (ret != CL_SUCCESS) { PrintOpenCLError(ret); return r; } else { }

bool CheckForExtensions(cl_device_id device, const Set<String>&requiredExtensions);