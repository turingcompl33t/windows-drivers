/*
 * Client.cpp
 * User-mode client for the SysMon driver.
 */

#include <Windows.h>
#include <stdio.h>
#include <string>

#include "SysMonCommon.h"

void DisplayInfo(BYTE* buffer, DWORD size);
void DisplayTime(const LARGE_INTEGER& time);

int Error(const char* msg);

/* ----------------------------------------------------------------------------
	Entry Point
*/

int main()
{
	auto hFile = ::CreateFileW(L"\\\\.\\SysMon", GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (INVALID_HANDLE_VALUE == hFile)
		return Error("Failed to open file");

	BYTE buffer[1 << 16];  // 64KB buffer

	while (true)
	{
		DWORD bytes;
		if (!::ReadFile(hFile, buffer, sizeof(buffer), &bytes, nullptr))
			return Error("Failed to read");

		if (0 != bytes)
			DisplayInfo(buffer, bytes);

		::Sleep(200);
	}

	return 0;
}

/* ----------------------------------------------------------------------------
	Helpers
*/

// display information recvd from driver
void DisplayInfo(BYTE* buffer, DWORD size)
{
	auto count = size;
	while (count > 0)
	{
		auto header = (ItemHeader*) buffer;

		switch (header->Type) {
			case ItemType::ProcessExit:
			{
				DisplayTime(header->Time);
				auto info = (ProcessExitInfo*)buffer;
				printf("Process %d Exited\n", info->ProcessId);
				break;
			}

			case ItemType::ProcessCreate:
			{
				DisplayTime(header->Time);
				auto info = (ProcessCreateInfo*)buffer;
				std::wstring commandLine((WCHAR*)buffer + info->CommandLineOffset);
				printf("Process %d Created. Coommand Line: %ws\n", info->ProcessId, commandLine.c_str());
				break;
			}

			case ItemType::ThreadCreate:
			{
				DisplayTime(header->Time);
				auto info = (ThreadCreateExitInfo*)buffer;
				printf("Thread %d Created in Process %d\n", info->ThreadId, info->ProcessId);
				break;
			}

			case ItemType::ThreadExit:
			{
				DisplayTime(header->Time);
				auto info = (ThreadCreateExitInfo*)buffer;
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
	::FileTimeToSystemTime((FILETIME*)& time, &st);
	printf("%02d:%02d:%02d.%03d: ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

// error handling helper
int Error(const char* msg)
{
	DWORD err = GetLastError();
	printf("[ERROR] %s (0x%08X)\n", msg, err);
	return err;
}