/*
 * SysMon.cpp
 */

#include "SysMon.h"
#include "SysMonCommon.h"

#include "SyncHelpers.h"

void PushItem(LIST_ENTRY* entry);
void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo);

Globals g_Globals;

extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	auto status = STATUS_SUCCESS;

	InitializeListHead(&g_Globals.ItemsHead);
	g_Globals.Mutex.Init();

	PDEVICE_OBJECT DeviceObject = nullptr;
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\sysmon");
	bool symLinkCreated = false;

	do {
		UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\sysmon");
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("Failed to create device (0x%08x)\n", status));
			break;
		}

		DeviceObject->Flags |= DO_DIRECT_IO;

		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("Failed to create symbolic link to device (0x%08x)\n", status));
			break;
		}

		symLinkCreated = true;

		status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("Failed to set process notify routine (0x%08x\n", status));
			break;
		}
	} while (false);

	if (!NT_SUCCESS(status))
	{
		if (symLinkCreated) 
		{
			IoDeleteSymbolicLink(&symLink);
		}

		if (DeviceObject)
		{
			IoDeleteDevice(DeviceObject);
		}
	}

	DriverObject->DriverUnload = SysMonUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = SysMonCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE]  = SysMonCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ]   = SysMonRead;

	return status;
}

void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo)
{
	if (CreateInfo)
	{
		// process create
	}
	else
	{
		// process exit 
		auto info = (FullItem<ProcessExitInfo>*) ExAllocatePoolWithTag(PagedPool, sizeof(FullItem<ProcessExitInfo>), DRIVER_TAG);
		if (nullptr == info)
		{
			KdPrint(("Failed to allocate memory for ProcessExitInfo\n"));
			return;
		}

		auto& item = info->Data;
		KeQuerySystemTimePrecise(&item.Time);
		item.Type = ItemType::ProcessExit;
		item.ProcessId = HandleToULong(ProcessId);
		item.Size = sizeof(ProcessExitInfo);

		PushItem(&info->Entry);
	}
}

void PushItem(LIST_ENTRY* entry)
{
	AutoLock<FastMutex> lock(g_Globals.Mutex);
	if (g_Globals.ItemCount > 1024)
	{
		// too many items, remove oldest
		auto head = RemoveHeadList(&g_Globals.ItemsHead);
		g_Globals.ItemCount--;
		auto item = CONTAINING_RECORD(head, FullItem<ItemHeader>, Entry);
		ExFreePool(item);
	}

	InsertTailList(&g_Globals.ItemsHead, entry);
	g_Globals.ItemCount++;
}