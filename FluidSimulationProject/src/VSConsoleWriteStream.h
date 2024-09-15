#pragma once

class VSConsoleWriteStream : public Blaze::WriteStream
{
public:
	// Inherited via WriteStream
	bool MovePosition(Blaze::intMem offset) override
	{
		return false;
	}
	bool SetPosition(Blaze::uintMem offset) override
	{
		return false;
	}
	bool SetPositionFromEnd(Blaze::intMem offset) override
	{
		return false;
	}
	Blaze::uintMem GetPosition() const override
	{
		return Blaze::uintMem();
	}
	Blaze::uintMem GetSize() const override
	{
		return Blaze::uintMem();
	}
	Blaze::uintMem Write(const void* ptr, Blaze::uintMem byteCount) override;
};