// SingleInstanceClient.cpp
// Client for SingleInstance Driver.

#include <tchar.h>
#include <windows.h>

#include <string>
#include <iostream>

static constexpr auto STATUS_SUCCESS_I = 0x0;
static constexpr auto STATUS_FAILURE_I = 0x1;

VOID LogInfo(const std::string& msg);
VOID LogError(const std::string& msg);

INT _tmain()
{
	LogInfo("SingleInstance Driver Client");

	auto hSymlink = CreateFile(
		"\\\\.\\SingleInstance", 
		GENERIC_READ, 
		0, 
		nullptr, 
		OPEN_EXISTING, 
		0, 
		nullptr
	);

	if (INVALID_HANDLE_VALUE == hSymlink)
	{
		LogError("Failed to acquire handle to device");
		return STATUS_FAILURE_I;
	}

	LogInfo("Successfully acquired handle to device");
	LogInfo("Holding handle indefinitely... (ENTER to release)");

	std::cin.get();

	CloseHandle(hSymlink);

	return STATUS_SUCCESS_I;
}

VOID LogInfo(const std::string& msg)
{
	std::cout << "[+] " << msg << std::endl;
}

VOID LogError(const std::string& msg)
{
	std::cout << "[-] " << msg << '\n';
	std::cout << "[-] (GLE): " << GetLastError();
	std::cout << std::endl;
}
