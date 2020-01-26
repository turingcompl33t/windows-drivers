// DeviceMonitor.cpp
// Generic filter driver for monitoring arbitrary kernel devices.

#include "Utility.h"
#include "DeviceMonitor.h"
#include "DeviceManager.h"
#include "DeviceMonitorCommon.h"

DeviceManager g_DeviceManager;

// DriverEntry
// Driver entry point.
extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\DeviceMonitor");
	PDEVICE_OBJECT DeviceObject;

	auto status = ::IoCreateDevice(
		DriverObject,
		0,
		&deviceName,
		FILE_DEVICE_UNKNOWN,
		0,
		TRUE,
		&DeviceObject
		);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	UNICODE_STRING linkName = RTL_CONSTANT_STRING(L"\\??\\DeviceMonitor");
	status = ::IoCreateSymbolicLink(&linkName, &deviceName);
	if (!NT_SUCCESS(status))
	{
		::IoDeleteDevice(DeviceObject);
		return status;
	}

	DriverObject->DriverUnload = DeviceMonitorUnload;

	// initialize all dispatch routines 
	// this step is very important as it ensure that the default
	// behavior for our filter driver is to forward the request to 
	// the underlying device to which we are attached

	for (auto& func : DriverObject->MajorFunction)
	{
		func = HandleFilterFunction;
	}

	// initialize the monitored device manager

	g_DeviceManager.CDO = DeviceObject;
	g_DeviceManager.Init(DriverObject);

	return status;
}

// DeviceMonitorUnload
// Driver unload callback.
void DeviceMonitorUnload(PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);

	// delete the symbolic link
	UNICODE_STRING linkName = RTL_CONSTANT_STRING(L"\\??\\DeviceMonitor");
	::IoDeleteSymbolicLink(&linkName);

	// delete the filter device
	NT_ASSERT(g_DeviceManager.CDO);
	::IoDeleteDevice(g_DeviceManager.CDO);

	// detach all currently active filters
	g_DeviceManager.RemoveAllDevices();
}

// HandleFilterFunction
// Handles all requests that directly or indirectly target this driver;
// such requests may have originally been intended for the CDO we define
// or may have targeted a device that we are currently filtering.
NTSTATUS HandleFilterFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	// this function is invoked for all major function table dipatch routines,
	// both for the CDO as well as for all of the filter device instances

	// first step: determine if this request is targeting our CDO or 
	// is invoked in the context of a filtering operation
	if (DeviceObject == g_DeviceManager.CDO)
	{
		switch (::IoGetCurrentIrpStackLocation(Irp)->MajorFunction)
		{
		case IRP_MJ_CREATE:
		case IRP_MJ_CLOSE:
			return CompleteRequest(Irp);
		
		case IRP_MJ_DEVICE_CONTROL:
			return DeviceMonitorIoControl(DeviceObject, Irp);
		}

		return CompleteRequest(Irp, STATUS_INVALID_DEVICE_REQUEST);
	}

	// this request is not targeting our CDO, so it must be targeting a filtered device

	auto ext = static_cast<DeviceExtension*>(DeviceObject->DeviceExtension);

	// get the thread that made the request
	// this will typically be the same thread in whose context
	// we are currently executing, but this is not guaranteed
	auto thread = Irp->Tail.Overlay.Thread;
	HANDLE tid = nullptr;
	HANDLE pid = nullptr;
	if (thread)
	{
		tid = ::PsGetThreadId(thread);
		pid = ::PsGetThreadProcessId(thread);
	}

	auto stack = ::IoGetCurrentIrpStackLocation(Irp);

	// log some output about this request to kernel debugger
	DbgPrint("Intercepted driver: %wZ: PID: %d, TID: %d, MJ=%d (%s)\n",
		&ext->LowerDeviceObject->DriverObject->DriverName,
		::HandleToULong(pid),
		::HandleToULong(tid),
		stack->MajorFunction,
		MajorFunctionToString(stack->MajorFunction)
		);

	// pass the request along unaltered
	::IoSkipCurrentIrpStackLocation(Irp);
	return ::IoCallDriver(ext->LowerDeviceObject, Irp);
}

// DeviceMonitorIoControl
// Invoked to handle IO control requests from user-mode client.
NTSTATUS DeviceMonitorIoControl(PDEVICE_OBJECT, PIRP Irp)
{
	auto stack = ::IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_INVALID_DEVICE_REQUEST;
	auto code = stack->Parameters.DeviceIoControl.IoControlCode;

	switch (code)
	{
	case IOCTL_DEVMON_ADD_DEVICE:
	case IOCTL_DEVMON_REMOVE_DEVICE:
	{
		auto buffer = static_cast<WCHAR*>(Irp->AssociatedIrp.SystemBuffer);
		auto len = stack->Parameters.DeviceIoControl.InputBufferLength;
		if (nullptr == buffer || len < 2 || len < 512)
		{
			status = STATUS_INVALID_BUFFER_SIZE;
			break;
		}

		buffer[len / sizeof(WCHAR) - 1] = L'\0';
		if (IOCTL_DEVMON_ADD_DEVICE == code)
		{
			status = g_DeviceManager.AddDevice(buffer);
		}
		else
		{
			auto removed = g_DeviceManager.RemoveDevice(buffer);
			status = removed ? STATUS_SUCCESS : STATUS_NOT_FOUND;
		}
		break;
	}
	case IOCTL_DEVMON_REMOVE_ALL:
	{
		g_DeviceManager.RemoveAllDevices();
		status = STATUS_SUCCESS;
		break;
	}
	}

	return CompleteRequest(Irp, status);
}

// CompleteRequest
// Helper function to complete an IO system request.
NTSTATUS CompleteRequest(
	PIRP Irp,
	NTSTATUS status,
	ULONG_PTR information
)
{
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = information;
	::IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

// MajorFunctionToString
// Helper function to convert major function code to string representation.
const char* MajorFunctionToString(UCHAR major)
{
	static const char* strings[] = {
		"IRP_MJ_CREATE",
		"IRP_MJ_CREATE_NAMED_PIPE",
		"IRP_MJ_CLOSE",
		"IRP_MJ_READ",
		"IRP_MJ_WRITE",
		"IRP_MJ_QUERY_INFORMATION",
		"IRP_MJ_SET_INFORMATION",
		"IRP_MJ_QUERY_EA",
		"IRP_MJ_SET_EA",
		"IRP_MJ_FLUSH_BUFFERS",
		"IRP_MJ_QUERY_VOLUME_INFORMATION",
		"IRP_MJ_SET_VOLUME_INFORMATION",
		"IRP_MJ_DIRECTORY_CONTROL",
		"IRP_MJ_FILE_SYSTEM_CONTROL",
		"IRP_MJ_DEVICE_CONTROL",
		"IRP_MJ_INTERNAL_DEVICE_CONTROL",
		"IRP_MJ_SHUTDOWN",
		"IRP_MJ_LOCK_CONTROL",
		"IRP_MJ_CLEANUP",
		"IRP_MJ_CREATE_MAILSLOT",
		"IRP_MJ_QUERY_SECURITY",
		"IRP_MJ_SET_SECURITY",
		"IRP_MJ_POWER",
		"IRP_MJ_SYSTEM_CONTROL",
		"IRP_MJ_DEVICE_CHANGE",
		"IRP_MJ_QUERY_QUOTA",
		"IRP_MJ_SET_QUOTA",
		"IRP_MJ_PNP",
	};

	NT_ASSERT(major <= IRP_MJ_MAXIMUM_FUNCTION);
	return strings[major];
}