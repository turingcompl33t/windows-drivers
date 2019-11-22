// AsyncIoClient.cpp
// Client application for AsyncIO driver.

#include <tchar.h>
#include <windows.h>

#include <string>
#include <iostream>

#include "AsyncIoCommon.h"

constexpr auto STATUS_SUCCESS_I = 0x0;
constexpr auto STATUS_FAILURE_I = 0x1;

VOID DoReadCommand(HANDLE hDevice);
VOID DoWriteCommand(HANDLE hDevice);
VOID DoCountCommand(HANDLE hDevice);

VOID LogInfo(const std::string& msg);
VOID LogWarning(const std::string& msg);
VOID LogError(const std::string& msg);

INT _tmain()
{
	LogInfo("AsyncIO Driver Client");

	HANDLE hDevice = CreateFile(
		"\\\\.\\AsyncIO", 
		GENERIC_READ | GENERIC_WRITE, 
		FILE_SHARE_READ | FILE_SHARE_WRITE, 
		nullptr, 
		OPEN_EXISTING, 
		0, 
		nullptr
		);
	
	if (INVALID_HANDLE_VALUE == hDevice)
	{
		LogError("Failed to CreateFile()");
		return STATUS_FAILURE_I;
	}

	LogInfo("Successfully acquired handle to device");
	LogInfo("Entering command loop; <COMMAND> + ENTER to execute:");
	LogInfo("\t(r) issue READ request");
	LogInfo("\t(w) issue WRITE request");
	LogInfo("\t(c) issue COUNT query");
	LogInfo("\t(q) exit the command loop");

	BOOL quit = FALSE;

	CHAR cmdBuffer[256];
	RtlZeroMemory(cmdBuffer, 256);

	while (!quit)
	{
		std::cout << "> ";
		std::cin.getline(cmdBuffer, 256);

		switch (cmdBuffer[0])
		{
		case 'r':
		case 'R':
		{
			LogInfo("Handling READ request...");
			DoReadCommand(hDevice);
			break;
		}
		case 'w':
		case 'W':
		{
			LogInfo("Handling WRITE request...");
			DoWriteCommand(hDevice);
			break;
		}
		case 'c':
		case 'C':
		{
			LogInfo("Handling COUNT query");
			DoCountCommand(hDevice);
			break;
		}
		case 'q':
		case 'Q':
		{
			LogInfo("Exiting command loop");
			quit = TRUE;
			break;
		}
		case '\n':
		{
			// allow for empty prompt cycle
			continue;
		}
		default:
		{
			LogWarning("Unrecognized command");
			break;
		}
		}
	}

	CloseHandle(hDevice);

	return STATUS_SUCCESS_I;
}

// handle a read command
VOID DoReadCommand(HANDLE hDevice)
{
	DWORD dwBytesRead;
	UCHAR buffer[MAX_READ_SIZE];

	BOOL bRet = ReadFile(hDevice, static_cast<PVOID>(buffer), MAX_READ_SIZE, &dwBytesRead, nullptr);
	if (!bRet)
	{
		LogError("READ request failed (ReadFile())");
		return;
	}

	LogInfo("Successfully completed READ request:");
	for (UINT i = 0; i < dwBytesRead; ++i)
	{
		printf("0x%02X ", buffer[i]);
	}

	std::cout << std::endl;
}

// handle a write command
VOID DoWriteCommand(HANDLE hDevice)
{
	DWORD dwBytesWritten;
	UCHAR buffer[MAX_WRITE_SIZE];
	RtlZeroMemory(buffer, MAX_WRITE_SIZE);

	// just fill with recognizable data
	for (UINT i = 0; i < MAX_WRITE_SIZE; ++i)
	{
		buffer[i] = i;
	}

	BOOL bRet = WriteFile(hDevice, static_cast<PVOID>(buffer), MAX_WRITE_SIZE, &dwBytesWritten, nullptr);
	if (!bRet)
	{
		LogError("WRITE request failed (WriteFile())");
		return;
	}

	LogInfo("Successfully completed WRITE request");
}

// handle a count command
VOID DoCountCommand(HANDLE hDevice)
{
	DWORD dwBytesReturned;
	MARSHAL_HELPER marshaller{ 0 };

	BOOL status = DeviceIoControl(
		hDevice,
		IOCTL_ASYNCIO_QUERY_QUEUE_COUNTS,
		nullptr,
		0,
		reinterpret_cast<VOID*>(&marshaller),
		sizeof(marshaller),
		&dwBytesReturned,
		nullptr
	);

	if (!status)
	{
		LogWarning("COUNT query failed (DeviceIoControl())");
		return;
	}

	LogInfo("Successfully completed COUNT request:");
	std::cout << "[+] \tRead Queue Count:  " << marshaller.u1 << '\n';
	std::cout << "[+] \tWrite Queue Count: " << marshaller.u2 << '\n';
	std::cout << std::flush;
}

/* ----------------------------------------------------------------------------
 * Utility Functions
 */

VOID LogInfo(const std::string& msg)
{
	std::cout << "[+] " << msg << std::endl;
}

VOID LogWarning(const std::string& msg)
{
	std::cout << "[-] " << msg << std::endl;
}

VOID LogError(const std::string& msg)
{
	std::cout << "[!] " << msg << '\n';
	std::cout << "[!] (GLE): " << GetLastError() << '\n';
	std::cout << std::flush;
}