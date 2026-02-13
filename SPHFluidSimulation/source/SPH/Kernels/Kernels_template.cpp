#include "Kernels.h"

namespace SPH::Kernels
{
	static const char compatibilityHeader_str[] = { $bytes "CompatibilityHeaderOpenCL.cl"$, '\0' };
	const Blaze::StringView compatibilityHeader = Blaze::StringView(compatibilityHeader_str);

	static const char SPHKernelSource_str[] = { $bytes "SPHFunctions.cpp"$, '\0' };
	const Blaze::StringView SPHKernelSource = Blaze::StringView(SPHKernelSource_str);
}