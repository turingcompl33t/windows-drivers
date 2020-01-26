// DeviceManager.cpp
// Helper class implementation for managing filtered devices.

#include "Utility.h"
#include "DeviceManager.h"

void DeviceManager::Init(PDRIVER_OBJECT driver)
{
	// initialize the fast mutex guard
	Lock.init();
	
	// save the pointer to the driver object
	// (used in subsequent calls to IoCreateDevice())
	DriverObject = driver;
}

NTSTATUS DeviceManager::AddDevice(PCWSTR name)
{
	kdl::auto_lock<kdl::fast_mutex> guard{ Lock };

	// determine if we have reached the maximum number of monitored devices
	if (MonitoredDeviceCount >= MAXIMUM_DEVICE_COUNT)
	{
		return STATUS_TOO_MANY_NAMES;
	}

	// if the device is already being filtered, just return success
	if (FindDevice(name) >= 0)
	{
		return STATUS_SUCCESS;
	}

	int index = -1;

	// locate an empty location in the monitored device array 
	for (int i = 0; i < MAXIMUM_DEVICE_COUNT; ++i)
	{
		if (nullptr == Devices[i].DeviceObject)
		{
			index = i;
			break;
		}
	}

	// redundant check, not really necessary
	if (index < 0)
	{
		return STATUS_TOO_MANY_NAMES;
	}

	UNICODE_STRING target;
	RtlInitUnicodeString(&target, name);

	PFILE_OBJECT FileObject;
	PDEVICE_OBJECT LowerDeviceObject = nullptr;

	// attempt to acquire a pointer to the device object to which we want to attach
	// this function may in fact NOT return a pointer to the device object to which
	// we want to attach, and instead return a pointer to the device object at the 
	// top of the device stack in which the actual target device is located
	auto status = ::IoGetDeviceObjectPointer(&target, FILE_READ_DATA, &FileObject, &LowerDeviceObject);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to acquire device object pointer (%ws) (0x%8X)\n", name, status));
		return status;
	}

	PDEVICE_OBJECT DeviceObject = nullptr;
	WCHAR* buffer = nullptr;

	do
	{
		status = ::IoCreateDevice(
			DriverObject, 
			sizeof(DeviceExtension), 
			nullptr, 
			FILE_DEVICE_UNKNOWN, 
			0, 
			FALSE, 
			&DeviceObject
			);

		if (!NT_SUCCESS(status))
		{
			break;
		}

		buffer = static_cast<WCHAR*>(
			::ExAllocatePoolWithTag(PagedPool, target.Length, DRIVER_TAG));
		if (!buffer)
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		auto ext = static_cast<DeviceExtension*>(DeviceObject->DeviceExtension);

		// copy various properties from the lower device object to the new device object
		// this step is EXTREMELY IMPORTANT to ensure that the device that is now
		// being filtered by our device continues to function properly (and doesn't crash system)
		DeviceObject->Flags |= (LowerDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO));
		DeviceObject->Type = LowerDeviceObject->Type;

		// initialize the fields of the new entry
		Devices[index].DeviceName.Buffer = buffer;
		Devices[index].DeviceName.MaximumLength = target.Length;
		::RtlCopyUnicodeString(&Devices[index].DeviceName, &target);
		Devices[index].DeviceObject = DeviceObject;

		// attach the device to the device stack
		// TODO: why is IoAttachDeviceToDeviceStackSafe() not available?
		ext->LowerDeviceObject = ::IoAttachDeviceToDeviceStack(
			DeviceObject,
			LowerDeviceObject
		);
		if (NULL == ext->LowerDeviceObject)
		{
			// TODO: more appropriate return code here?
			status = STATUS_DEVICE_UNREACHABLE;
			break;
		}

		Devices[index].LowerDeviceObject = ext->LowerDeviceObject;

		// these flags required for hardware devices
		DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
		DeviceObject->Flags |= DO_POWER_PAGABLE;

		MonitoredDeviceCount++;
	} while (false);

	if (!NT_SUCCESS(status))
	{
		if (buffer)
		{
			::ExFreePool(buffer);
		}
		if (DeviceObject)
		{
			::IoDeleteDevice(DeviceObject);
		}
	}

	// dereference the file object obtained earlier
	// note that we must not dereference the device object
	// itself - it will be automatically dereferenced when
	// the file object's reference count reaches 0
	if (LowerDeviceObject)
	{
		::ObDereferenceObject(FileObject);
	}

	return status;
}

void DeviceManager::RemoveAllDevices()
{
	kdl::auto_lock<kdl::fast_mutex> guard{ Lock };

	for (int i = 0; i < MAXIMUM_DEVICE_COUNT; ++i)
	{
		RemoveDevice(i);
	}
}

bool DeviceManager::RemoveDevice(PCWSTR name)
{
	kdl::auto_lock<kdl::fast_mutex> guard{ Lock };
	auto index = FindDevice(name);
	if (index < 0)
	{
		return false;
	}

	return RemoveDevice(index);
}

bool DeviceManager::RemoveDevice(int index)
{
	auto& device = Devices[index];
	if (nullptr == device.DeviceObject)
	{
		return false;
	}

	// cleanup the resources for the monitored device
	::ExFreePool(device.DeviceName.Buffer);
	::IoDetachDevice(device.LowerDeviceObject);
	::IoDeleteDevice(device.DeviceObject);
	device.DeviceObject = nullptr;

	MonitoredDeviceCount--;
	return true;
}

// Find Device
// Helper function to locate specified device within the array
// maintained by the manager by name.
//
// NOTE: device manager lock must be held before calling.
int DeviceManager::FindDevice(PCWSTR name)
{
	UNICODE_STRING uname;
	::RtlInitUnicodeString(&uname, name);
	for (int i = 0; i < MAXIMUM_DEVICE_COUNT; ++i)
	{
		auto& device = Devices[i];
		if (device.DeviceObject &&
			::RtlEqualUnicodeString(&device.DeviceName, &uname, TRUE))
		{
			return i;
		}
	}

	// not found
	return -1;
}