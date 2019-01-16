#pragma once
#include <Windows.h>
#include <TlHelp32.h>

#define UNINITIALIZED 0xFFFFFFFF

class RMEM
{
public:
	RMEM(DWORD, HANDLE);
	~RMEM();

	DWORD GetModuleBase(const char*) const;

	template <typename T>
	T Read(DWORD offset) const
	{
		T _read;
		ReadProcessMemory(this->procHandle, (const void*)offset, &_read, sizeof(T), NULL);
		return _read;
	}

private:
	HANDLE procHandle;
	DWORD procId;
};
