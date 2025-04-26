#include "Kernels.h"

const unsigned int partialSumKernelSourceSize = $size "partialSum.cl"$;
const char partialSumKernelSourceBytes[]{
	$bytes "partialSum.cl"$
};

const unsigned int compatibiliyHeaderOpenCLSize = $size "CompatibilityHeaderOpenCL.cl"$;
const char compatibilityHeaderOpenCLBytes[]{
	$bytes "CompatibilityHeaderOpenCL.cl"$
}; 

const unsigned int SPHKernelSourceSize = $size "SPHFunctions.cpp"$;
const char SPHKernelSourceBytes[]{
	$bytes "SPHFunctions.cpp"$
}; 