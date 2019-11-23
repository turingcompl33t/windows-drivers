// SysmonV2Client.cpp
// Client application for improved system monitoring kernel driver.

#include <tchar.h>
#include <windows.h>

#include <iostream>

#include "SysmonV2Common.h"

constexpr auto STATUS_SUCCESS_I = 0x0;
constexpr auto STATUS_FAILURE_I = 0x01;

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

	return STATUS_SUCCESS_I;
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