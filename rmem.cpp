#include "rmem.h"

RMEM::RMEM(DWORD procId, HANDLE procHandle)
{
	this->procId = procId;
	this->procHandle = procHandle;
}
RMEM::~RMEM()
{
	CloseHandle(this->procHandle);
}

DWORD RMEM::GetModuleBase(const char* moduleName) const
{
	DWORD dwModuleBase = UNINITIALIZED;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, this->procId);
	MODULEENTRY32 moduleEntry;
	moduleEntry.dwSize = sizeof(MODULEENTRY32);

	do
	{
		if (!strcmp(moduleEntry.szModule, moduleName))
			dwModuleBase = (DWORD)(moduleEntry.modBaseAddr);
	} 
	while (Module32Next(hSnapshot, &moduleEntry) && dwModuleBase == UNINITIALIZED);

	CloseHandle(hSnapshot);

	if (dwModuleBase == UNINITIALIZED) exit(1);

	return dwModuleBase;
}
