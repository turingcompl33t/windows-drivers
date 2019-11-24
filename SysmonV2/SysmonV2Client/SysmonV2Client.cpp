// SysmonV2Client.cpp
// Client application for improved system monitoring kernel driver.

#include <tchar.h>
#include <windows.h>

#include <iostream>

#include "SysmonV2Common.h"

// 64KB results buffer
constexpr auto BUFFER_SIZE = (1 << 16);

constexpr auto STATUS_SUCCESS_I = 0x0;
constexpr auto STATUS_FAILURE_I = 0x01;

DWORD DoProcessEventQuery(HANDLE hDevice, LPBYTE buffer);
DWORD DoThreadEventQuery(HANDLE hDevice, LPBYTE buffer);

void DisplayResults(LPBYTE buffer, DWORD size);
void DisplayTime(const LARGE_INTEGER& time);

VOID LogInfo(const std::string& msg);
VOID LogWarning(const std::string& msg);
VOID LogError(const std::string& msg);

INT _tmain()
{
	LogInfo("SysmonV2 - Improved System Event Monitoring");

	HANDLE hDevice = CreateFile(
		"\\\\.\\SysmonV2",
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr
	);
	if (INVALID_HANDLE_VALUE == hDevice)
	{
		LogError("Failed to acquire handle to SysmonV2 device (CreateFile())");
		return STATUS_FAILURE_I;
	}

	LogInfo("Successfully acquired handle to SysmonV2 device");
	LogInfo("Entering command loop; <COMMAND> + ENTER to execute:");
	LogInfo("\t(p) query PROCESS events");
	LogInfo("\t(t) query THREAD events");

	DWORD dwBytesReturned;
	BOOL quit = FALSE;

	CHAR cmdBuffer[256];
	RtlZeroMemory(cmdBuffer, 256);

	LPBYTE resultsBuffer = static_cast<LPBYTE>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, BUFFER_SIZE));

	while (!quit)
	{
		std::cout << "> ";
		std::cin.getline(cmdBuffer, 256);

		switch (cmdBuffer[0])
		{
		case 'p':
		case 'P':
		{
			LogInfo("Queryimg PROCESS events...");

			dwBytesReturned = DoProcessEventQuery(hDevice, resultsBuffer);
			if (dwBytesReturned > 0)
			{
				DisplayResults(resultsBuffer, dwBytesReturned);
			}

			break;
		}
		case 't':
		case 'T':
		{
			LogInfo("Querying THREAD events...");

			dwBytesReturned = DoThreadEventQuery(hDevice, resultsBuffer);
			if (dwBytesReturned > 0)
			{
				DisplayResults(resultsBuffer, dwBytesReturned);
			}

			break;
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

// perform process event query
DWORD DoProcessEventQuery(HANDLE hDevice, LPBYTE buffer)
{
	DWORD dwBytesReturned;

	// perform the IO
	BOOL status = DeviceIoControl(
		hDevice,
		IOCTL_SYSMONV2_QUERY_PROCESS_EVENTS,
		nullptr,
		0,
		static_cast<LPVOID>(buffer),
		BUFFER_SIZE,
		&dwBytesReturned,
		nullptr
	);

	if (!status)
	{
		LogError("Failed to query process events (DeviceIoControl())");
		return 0;
	}

	return dwBytesReturned;
}

// perform thread event query
DWORD DoThreadEventQuery(HANDLE hDevice, LPBYTE buffer)
{
	DWORD dwBytesReturned;

	// perform the IO
	BOOL status = DeviceIoControl(
		hDevice,
		IOCTL_SYSMONV2_QUERY_THREAD_EVENTS,
		nullptr,
		0,
		static_cast<LPVOID>(buffer), 
		BUFFER_SIZE,
		&dwBytesReturned,
		nullptr
	);

	if (!status)
	{
		LogError("Failed to query thread events (DeviceIoControl())");
		return 0;
	}

	return dwBytesReturned;
}

// display information recvd from driver query
void DisplayResults(LPBYTE buffer, DWORD size)
{
	auto count = size;
	while (count > 0)
	{
		auto header = (ItemHeader*)buffer;

		switch (header->Type) {
		case ItemType::ProcessCreate:
		{
			DisplayTime(header->Time);
			auto info = (ProcessCreateItem*)buffer;
			std::wstring commandLine((WCHAR*)buffer + info->CommandLineOffset);
			printf("Process %d Created. Coommand Line: %ws\n", info->ProcessId, commandLine.c_str());
			break;
		}
		case ItemType::ProcessExit:
		{
			DisplayTime(header->Time);
			auto info = (ProcessExitItem*)buffer;
			printf("Process %d Exited\n", info->ProcessId);
			break;
		}
		case ItemType::ThreadCreate:
		{
			DisplayTime(header->Time);
			auto info = (ThreadCreateItem*)buffer;
			printf("Thread %d Created in Process %d\n", info->ThreadId, info->ProcessId);
			break;
		}

		case ItemType::ThreadExit:
		{
			DisplayTime(header->Time);
			auto info = (ThreadExitItem*)buffer;
			printf("Thread %d Exited from Process %d\n", info->ThreadId, info->ProcessId);
			break;
		}
		default:
			break;
		}

		buffer += header->Size;
		count -= header->Size;
	}
}

// disiplay time in human-readable format
void DisplayTime(const LARGE_INTEGER& time)
{
	SYSTEMTIME st;
	FileTimeToSystemTime((FILETIME*)&time, &st);
	printf("%02d:%02d:%02d.%03d: ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

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
	std::cout << "[!]\t(GLE): " << GetLastError() << '\n';
	std::cout << std::flush;
}