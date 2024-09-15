#include "pch.h"
#include "VSConsoleWriteStream.h"

#include <Windows.h>

Blaze::uintMem VSConsoleWriteStream::Write(const void* ptr, Blaze::uintMem byteCount)
{
	Blaze::String str{ (const char*)ptr, byteCount };
	OutputDebugStringA(str.Ptr());
	return byteCount;
}