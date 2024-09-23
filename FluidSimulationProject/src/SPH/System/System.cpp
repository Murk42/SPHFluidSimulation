#include "pch.h"
#include "System.h"

namespace SPH
{    
	uint32 System::GetHash(Vec3i cell)
	{
		return (
			(uint)(cell.x * 73856093)
			^ (uint)(cell.y * 19349663)
			^ (uint)(cell.z * 83492791));
	}	
}