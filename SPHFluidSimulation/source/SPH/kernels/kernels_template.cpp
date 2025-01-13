#include "pch.h"
#include "kernels.h"

//$time "SPH.cl"$
const unsigned int SPHKernelSourceSize = $size "SPH.cl"$;
const unsigned char SPHKernelSource[]{
	$bytes "SPH.cl"$
};

//$time "partialSum.cl"$
const unsigned int partialSumKernelSourceSize = $size "partialSum.cl"$;
const unsigned char partialSumKernelSource[]{
	$bytes "partialSum.cl"$
};

//$time "../../../include/SPH/kernels/CL_CPP_SPHDeclarations.h"$
const unsigned int CL_CPP_SPHDeclarationsSourceSize = $size "../../../include/SPH/kernels/CL_CPP_SPHDeclarations.h"$;
const unsigned char CL_CPP_SPHDeclarationsSource[]{
	$bytes "../../../include/SPH/kernels/CL_CPP_SPHDeclarations.h"$
};

//$time "../../../include/SPH/kernels/CL_CPP_SPHFunctions.h"$
const unsigned int CL_CPP_SPHFuntionsSourceSize = $size "../../../include/SPH/kernels/CL_CPP_SPHFunctions.h"$;
const unsigned char CL_CPP_SPHFuntionsSource[]{
	$bytes "../../../include/SPH/kernels/CL_CPP_SPHFunctions.h"$
};