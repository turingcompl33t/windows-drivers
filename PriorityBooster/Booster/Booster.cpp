/*
 * Booster.cpp
 * User-space client for boosting thread priorities via PriorityBooster kernel driver. 
 */

#include <windows.h>
#include <stdio.h>

#include "..\PriorityBooster\PriorityBoosterCommon.h"

int Error(const char* msg);

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		printf("ERROR: invalid arguments\n");
		printf("USAGE: Booster <thread id> <priority>\n");
		return 1;
	}

	// open a handle to our device
	HANDLE hDevice = CreateFile(L"\\\\.\\PriorityBooster", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (INVALID_HANDLE_VALUE == hDevice)
	{
		return Error("ERROR: failed to open device");
	}

	// construct data to send to driver 
	ThreadData_t data;
	data.ThreadId = atoi(argv[1]);
	data.Priority = atoi(argv[2]);

	DWORD returned;
	BOOL success = DeviceIoControl(
		hDevice,
		IOCTL_PRIORITY_BOOSTER_SET_PRIORITY,
		&data,
		sizeof(data),
		nullptr,
		0,
		&returned,
		nullptr
	);
	if (success)
		printf("Priority change succeeded!\n");
	else
		Error("Priority change failed!");

	CloseHandle(hDevice);

	return 0;
}

// helper function for printing useful error messages
int Error(const char* msg)
{
	printf("%s (error = %d)\n", msg, GetLastError());
	return 1;
}