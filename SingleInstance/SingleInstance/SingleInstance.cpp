// SingleInstance.cpp
// A simple driver that only allows a single open device instance, system-wide.

#include <ntddk.h>

#include "SingleInstance.h"

extern "C" DRIVER_INITIALIZE DriverEntry;

_Function_class_(DRIVER_UNLOAD) 
VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject);

_Function_class_(DRIVER_DISPATCH) 
_Dispatch_type_(IRP_MJ_CREATE)
NTSTATUS DispatchCreate(PDEVICE_OBJECT, PIRP Irp);

_Function_class_(DRIVER_DISPATCH) 
_Dispatch_type_(IRP_MJ_CLOSE)
NTSTATUS DispatchClose(PDEVICE_OBJECT, PIRP Irp);

GlobalState g_GlobalState;

extern "C" 
NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject, 
	_In_ PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	DbgBreakPoint();

	NTSTATUS status = STATUS_SUCCESS;

	// initialize global state
	g_GlobalState.Mutex.Init();
	g_GlobalState.Flag = FALSE;

	UNICODE_STRING DeviceName;
	PDEVICE_OBJECT DeviceObject;

	RtlInitUnicodeString(&DeviceName, L"\\Device\\SingleInstance");

	status = IoCreateDevice(
		DriverObject,
		0,
		&DeviceName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&DeviceObject
		);

	if (!NT_SUCCESS(status))
	{
		// die
		KdPrint(("Failed to create device: 0x%08X\n", status));
		return status;
	}

	KdPrint(("Successfully created device object\n"));

	// create symlink to device

	UNICODE_STRING Symlink = RTL_CONSTANT_STRING(L"\\??\\SingleInstance");

	status = IoCreateSymbolicLink(&Symlink, &DeviceName);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to create symbolic link: 0x%08X\n", status));
		IoDeleteDevice(DeviceObject);
		return status;
	}

	KdPrint(("Successfully created symbolic link\n"));

	// init driver unload
	DriverObject->DriverUnload = DriverUnload;

	// init dispatch callbacks
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreate;
	DriverObject->MajorFunction[IRP_MJ_CLOSE]  = DispatchClose;

	KdPrint(("Successfully loaded driver\n"));

	return STATUS_SUCCESS;
}

_Use_decl_annotations_
VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);

	UNICODE_STRING Symlink = RTL_CONSTANT_STRING(L"\\??\\SingleInstance");
	IoDeleteSymbolicLink(&Symlink);
	IoDeleteDevice(DriverObject->DeviceObject);

	return;
}

_Use_decl_annotations_
NTSTATUS DispatchCreate(PDEVICE_OBJECT, PIRP Irp)
{
	KdPrint(("DispatchCreate() called\n"));

	auto status = STATUS_SUCCESS;

	{
		AutoLock<FastMutex> locker(g_GlobalState.Mutex);

		if (TRUE == g_GlobalState.Flag)
		{
			// device handle already acquired, deny access
			status = STATUS_ACCESS_DENIED;
		}
		else
		{
			// device handle is available, allow access
			g_GlobalState.Flag = TRUE;
		}
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, 0);

	return status;
}

_Use_decl_annotations_
NTSTATUS DispatchClose(PDEVICE_OBJECT, PIRP Irp)
{
	KdPrint(("DispatchClose() called\n"));

	{
		AutoLock<FastMutex> locker(g_GlobalState.Mutex);

		// release the instance
		g_GlobalState.Flag = FALSE;
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, 0);

	return STATUS_SUCCESS;
}

