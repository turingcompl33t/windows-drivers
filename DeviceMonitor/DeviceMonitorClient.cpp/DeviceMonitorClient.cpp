// DeviceMonitorClient.cpp
// Client application for interacting with DeviceMonitor kernel driver.

#include <windows.h>
#include <cstdio>

#include <DeviceMonitorCommon.h>

constexpr auto STATUS_SUCCESS_I = 0x0;
constexpr auto STATUS_FAILURE_I = 0x1;

int wmain(int argc, wchar_t* argv[])
{
	if (argc < 2)
	{
		printf("[-] Error: invalid arguments\n");
		printf("[-] Usage: %ws <COMMAND> [ARGUMENT]\n", argv[0]);
		return STATUS_FAILURE_I;
	}

	auto&  cmd = argv[1];
	HANDLE hDevice;
	DWORD  bytes;

	hDevice = ::CreateFileW(
		L"\\\\.\\DeviceMonitor",
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr
		);

	if (INVALID_HANDLE_VALUE == hDevice)
	{
		printf("[-] Failed to acquire handle to device object (%u)\n", ::GetLastError());
		return STATUS_FAILURE_I;
	}

	if (0 == ::_wcsicmp(cmd, L"add"))
	{
		if (!::DeviceIoControl(
			hDevice,
			IOCTL_DEVMON_ADD_DEVICE,
			static_cast<PVOID>(argv[2]),
			static_cast<DWORD>(::wcslen(argv[2]) + 1) * sizeof(WCHAR),
			nullptr,
			0,
			&bytes,
			nullptr
		))
		{
			printf("[-] Add device failed (%u)\n", ::GetLastError());
		}

		printf("[+] Successfully added device: %ws\n", argv[2]);
	}
	else if (0 == _wcsicmp(cmd, L"remove"))
	{
		if (!::DeviceIoControl(
			hDevice,
			IOCTL_DEVMON_REMOVE_DEVICE,
			static_cast<PVOID>(argv[2]),
			static_cast<DWORD>(::wcslen(argv[2]) + 1) * sizeof(WCHAR),
			nullptr,
			0,
			&bytes,
			nullptr
		))
		{
			printf("[-] Remove device failed (%u)\n", ::GetLastError());
		}

		printf("[+] Successfully removed device: %ws\n", argv[2]);
	}
	else if (0 == _wcsicmp(cmd, L"clear"))
	{
		if (!::DeviceIoControl(
			hDevice,
			IOCTL_DEVMON_REMOVE_ALL,
			nullptr,
			0,
			nullptr,
			0,
			&bytes,
			nullptr
		))
		{
			printf("[-] Clear devices failed (%u)\n", ::GetLastError());
		}

		printf("[+] Successfully removed all devices\n");
	}
	else
	{
		printf("[-] Error: unrecognized command\n");
	}

	return STATUS_SUCCESS_I;
}