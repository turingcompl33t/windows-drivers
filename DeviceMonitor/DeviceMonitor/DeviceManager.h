// DeviceManager.h
// Helper class implementation for managing filtered devices.

#pragma once

#include <wdm.h>
#include <kdl/auto_lock.h>
#include <kdl/fast_mutex.h>

constexpr int MAXIMUM_DEVICE_COUNT = 32;

// MonitoredDevice
// Represents an instance of a monitored device object.

struct MonitoredDevice
{
	UNICODE_STRING DeviceName;         // the name of the monitored device
	PDEVICE_OBJECT DeviceObject;       // pointer to device object for monitored device
	PDEVICE_OBJECT LowerDeviceObject;  // pointer to device object for next lower device in stack
};

// DeviceExtension
// For each filter device object we construct, we will store 
// a pointer to the lower device object in a custom device extension.

struct DeviceExtension
{
	PDEVICE_OBJECT LowerDeviceObject;
};

// DeviceManager
// Helper class for managing monitored device objects

class DeviceManager
{
public:
	void Init(PDRIVER_OBJECT DriverObject);
	NTSTATUS AddDevice(PCWSTR name);
	int FindDevice(PCWSTR name);
	bool RemoveDevice(PCWSTR name);
	void RemoveAllDevices();
	MonitoredDevice& GetDevice(int index);

public:
	PDEVICE_OBJECT CDO;

private:
	bool RemoveDevice(int index);

private:
	MonitoredDevice Devices[MAXIMUM_DEVICE_COUNT];
	int             MonitoredDeviceCount;
	kdl::fast_mutex Lock;
	PDRIVER_OBJECT  DriverObject;
};