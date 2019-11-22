// AsyncIoClientV2.cpp
// Enhanced client application for AsyncIO driver: now with async reads / writes!

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
VOID DoKillCommand(HANDLE hDevice);

VOID CALLBACK OverlappedReadCompletionRoutine(_In_ DWORD, _In_ DWORD, _Inout_ LPOVERLAPPED);
VOID CALLBACK OverlappedWriteCompletionRoutine(_In_ DWORD, _In_ DWORD, _Inout_ LPOVERLAPPED);

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
		FILE_FLAG_OVERLAPPED,
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
	LogInfo("\t(k) issue KILL command");
	LogInfo("\t(s) begin SLEEP to enter alertable wait state");
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
		case 'k':
		case 'K':
		{
			LogInfo("Handling KILL command");
			DoKillCommand(hDevice);
			break;
		}
		case 's':
		case 'S':
		{
			LogInfo("Entering alterable wait state...");
			SleepEx(0, TRUE);
			break;
		}
		case 'q':
		case 'Q':
		{
			LogInfo("Exiting command loop");
			quit = TRUE;
			break;
		}
		default:
		{
			LogWarning("Unrecognized command");
			break;
		}
		}
	}

	// send an IOCTL to cancel all pending operations prior to close
	DoKillCommand(hDevice);

	CloseHandle(hDevice);

	return STATUS_SUCCESS_I;
}

// handle a read command
VOID DoReadCommand(HANDLE hDevice)
{
	UCHAR buffer[MAX_READ_SIZE];

	// allocate a new OVERLAPPED structure
	LPOVERLAPPED pOverlapped = new OVERLAPPED{};
	pOverlapped->Offset = 0;
	pOverlapped->OffsetHigh = 0;

	BOOL bRet = ReadFileEx(
		hDevice, 
		static_cast<PVOID>(buffer), 
		MAX_READ_SIZE, 
		pOverlapped, 
		OverlappedReadCompletionRoutine
		);
	
	if (!bRet)
	{
		LogError("READ request failed (ReadFileEx())");
		return;
	}

	LogInfo("Successfully completed overlapped READ request");
}

VOID CALLBACK 
OverlappedReadCompletionRoutine(
	_In_	DWORD dwErrorCode, 
	_In_	DWORD dwNumberOfBytesTransferred, 
	_Inout_ LPOVERLAPPED lpOverlapped)
{
	std::cout << "[+] Completed READ request" << '\n';
	std::cout << "[+]	Bytes transferred: " << dwNumberOfBytesTransferred << '\n';
	std::cout << std::flush;

	// deallocate the overlapped structure
	delete lpOverlapped;
}

// handle a write command
VOID DoWriteCommand(HANDLE hDevice)
{
	UCHAR buffer[MAX_WRITE_SIZE];
	RtlZeroMemory(buffer, MAX_WRITE_SIZE);

	// just fill with recognizable data
	for (UINT i = 0; i < MAX_WRITE_SIZE; ++i)
	{
		buffer[i] = i;
	}

	LPOVERLAPPED pOverlapped = new OVERLAPPED{};
	pOverlapped->Offset = 0;
	pOverlapped->OffsetHigh = 0;

	BOOL bRet = WriteFileEx(
		hDevice, 
		static_cast<PVOID>(buffer), 
		MAX_WRITE_SIZE, 
		pOverlapped, 
		OverlappedWriteCompletionRoutine
		);

	if (!bRet)
	{
		LogError("WRITE request failed (WriteFileEx())");
		return;
	}

	LogInfo("Successfully completed overlapped WRITE request");
}

VOID CALLBACK 
OverlappedWriteCompletionRoutine(
	_In_	DWORD dwErrorCode, 
	_In_	DWORD dwNumberOfBytesTransferred, 
	_Inout_ LPOVERLAPPED lpOverlapped)
{
	std::cout << "[+] Completed WRITE request" << '\n';
	std::cout << "[+]	Bytes transferred: " << dwNumberOfBytesTransferred << '\n';
	std::cout << std::flush;

	// deallocate the overlapped structure
	delete lpOverlapped;
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
		LogError("COUNT query failed (DeviceIoControl())");
		return;
	}

	LogInfo("Successfully completed COUNT request:");
	std::cout << "[+] \tRead Queue Count:  " << marshaller.u1 << '\n';
	std::cout << "[+] \tWrite Queue Count: " << marshaller.u2 << '\n';
	std::cout << std::flush;
}

VOID DoKillCommand(HANDLE hDevice)
{
	DWORD dwBytesReturned;

	BOOL status = DeviceIoControl(
		hDevice,
		IOCTL_ASYNCIO_CANCEL_ALL_PENDING_IRPS,
		nullptr,
		0,
		nullptr,
		0,
		&dwBytesReturned,
		nullptr
		);

	if (!status)
	{
		LogError("KILL command failed (DeviceIoControl())");
		return;
	}

	LogInfo("Successfully sent KILL command to cancel all pending IRPs");
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
	std::cout << "[!]\t(GLE): " << GetLastError() << '\n';
	std::cout << std::flush;
}