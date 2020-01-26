// DeviceMonitor.h
// Generic filter driver for monitoring arbitrary kernel devices.

#pragma once

#include <ntifs.h>
#include <ntddk.h>

extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING);

void DeviceMonitorUnload(PDRIVER_OBJECT DriverObject);

NTSTATUS HandleFilterFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS DeviceMonitorIoControl(PDEVICE_OBJECT, PIRP Irp);

NTSTATUS CompleteRequest(
	PIRP Irp,
	NTSTATUS status = STATUS_SUCCESS,
	ULONG_PTR information = 0
);

const char* MajorFunctionToString(UCHAR major);