/*
 * PriorityBooster.cpp
 * A kernel-mode driver to support less-constrained thread priority boosting. 
 */

#include <ntifs.h>
#include <ntddk.h>

#include "PriorityBoosterCommon.h"

// driver unload routine
void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject);

// dispatch handlers
NTSTATUS PriorityBoosterCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS PriorityBoosterDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);

/* ----------------------------------------------------------------------------
	Core Driver Routines
*/

extern "C"
NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	// TODO (in driver entry routine):
	// 1. set an unload routine
	// 2. set dispatch routines the driver supports
	// 3. create a device object
	// 4. create a symbolic link to device object

	// 1. set an unload routine 
	DriverObject->DriverUnload = PriorityBoosterUnload;

	// 2. register dispatch routines our driver supports
	DriverObject->MajorFunction[IRP_MJ_CREATE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE]  = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PriorityBoosterDeviceControl;

	// 3. create a device object
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\PriorityBooster");
	
	PDEVICE_OBJECT DeviceObject;
	NTSTATUS status = IoCreateDevice(
		DriverObject,         // our driver object
		0,				      // no need for extra space allocated
		&devName,			  // name of the device to create
		FILE_DEVICE_UNKNOWN,  // device type
		0,					  // characteristics flags
		FALSE,				  // not exclusive
		&DeviceObject         // resulting pointer to alloced device object 
	);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to create device object (0x%08X)\n", status));
		return status;
	}

	// 4. create a symbolic link to the newly created device, so clients can get handles 
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");
	status = IoCreateSymbolicLink(&symLink, &devName);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to create symbolic link (0x08X)\n", status));
		IoDeleteDevice(DeviceObject);
		return status;
	}

	return STATUS_SUCCESS;
}

void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");

	// delete the symbolic link to device object (allocated in load)
	IoDeleteSymbolicLink(&symLink);

	// delete the device object (allocated in load)
	IoDeleteDevice(DriverObject->DeviceObject);
}

/* ----------------------------------------------------------------------------
	Driver Dispatch Routines
*/

// create / close dispatch routine
// this is invoked when a client (user-space) program obtains and 
// subsequently relinquishes a handle to the device (???)
_Use_decl_annotations_
NTSTATUS PriorityBoosterCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	
	// just always return with success for both create and close
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

// the device control dispatch routine 
// - the routine that is invoked when we recv an IRP from the client
_Use_decl_annotations_
NTSTATUS PriorityBoosterDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	// grab our current driver stack location from the IRP
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;

	// switch on the control code of the IRP to determine if we know how to handle this request 
	switch (stack->Parameters.DeviceIoControl.IoControlCode) 
	{
	case IOCTL_PRIORITY_BOOSTER_SET_PRIORITY: {
		auto len = stack->Parameters.DeviceIoControl.InputBufferLength;
		// validate the size of the thread data buffer we recvd
		if (len < sizeof(ThreadData_t))
		{
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		// extract the data from the buffer embedded within the IRP
		auto data = (ThreadData_t*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
		if (nullptr == data)
		{
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		// validate the priority the client has provided
		if (data->Priority < 1 || data->Priority > 31)
		{
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		PETHREAD Thread;
		// grab a reference to the thread for which we want to change priority
		status = PsLookupThreadByThreadId(ULongToHandle(data->ThreadId), &Thread);
		if (!NT_SUCCESS(status))
			break;

		// where the magic happens: just set the thread's priority
		KeSetPriorityThread((PKTHREAD)Thread, data->Priority);

		// release our reference to the thread object, which we (implicity) got when we looked it up
		// NOTE: this mechanism ensures there is no race condition here, as the kernel cannot deallocate
		// the kernel thread object, even if the thread terminates, until the ref count goes to zero
		ObDereferenceObject(Thread);

		KdPrint(("Thread priority change for %d to %d successful!\n", data->ThreadId, data->Priority));
		break;
	}

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}