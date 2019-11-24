// SysmonV2.cpp
// Improved system monitoring driver.

#pragma warning(disable : 26461)  // C26461 can be marked as a pointer to const

#include <ntddk.h>

#include "Tuple.h"
#include "SysmonV2.h"
#include "SysmonV2Common.h"

GLOBAL_STATE g_GlobalState;

/* ---------------------------------------------------------------------------- 
 *	Driver Entry
 */

extern "C" NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT pDriverObject,
	_In_ PUNICODE_STRING pRegistryPath
)
{
	UNREFERENCED_PARAMETER(pRegistryPath);

	auto status = STATUS_SUCCESS;

	PDEVICE_OBJECT pDeviceObject = nullptr;
	BOOLEAN        bSymlinkCreated = FALSE;
	UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\SysmonV2");
	UNICODE_STRING SymlinkName = RTL_CONSTANT_STRING(L"\\??\\SysmonV2");

	InitializeGlobalState();

	// create the device object

	status = IoCreateDevice(pDriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 0, TRUE, &pDeviceObject);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to create device\n"));
		goto EXIT;
	}

	// NOTE: only planning to use IOCTLs, so this doesn't really matter
	pDeviceObject->Characteristics |= DO_DIRECT_IO;

	// create the symbolic link

	status = IoCreateSymbolicLink(&SymlinkName, &DeviceName);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to create symbolic link"));
		goto EXIT;
	}

	bSymlinkCreated = TRUE;

	// setup dispatch handlers 

	pDriverObject->DriverUnload = DriverUnload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE]         = DispatchCreate;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE]          = DispatchClose;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceIoControl;

	// register callback routines

	status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to register process creation callback"));
		goto EXIT;
	}

	status = PsSetCreateThreadNotifyRoutine(OnThreadNotify);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to register thread creation callback"));
		goto EXIT;
	}

EXIT:

	// cleanup if we failed
	if (!NT_SUCCESS(status))
	{
		if (pDeviceObject != nullptr)
		{
			IoDeleteDevice(pDeviceObject);
		}

		if (bSymlinkCreated)
		{
			IoDeleteSymbolicLink(&SymlinkName);
		}
	}

	return STATUS_SUCCESS;
}

// helper function to initialize global state object
VOID InitializeGlobalState()
{
	InitializeListHead(&g_GlobalState.ProcessEventQueueHead);
	InitializeListHead(&g_GlobalState.ThreadEventQueueHead);
	g_GlobalState.ProcessEventQueueLock.Init();
	g_GlobalState.ThreadEventQueueLock.Init();
	g_GlobalState.ProcessEventQueueCount = 0;
	g_GlobalState.ThreadEventQueueCount = 0;
}

/* ----------------------------------------------------------------------------
 *	Unload Dispatch
 */

VOID DriverUnload(_In_ PDRIVER_OBJECT pDriverObject)
{
	UNICODE_STRING SymlinkName = RTL_CONSTANT_STRING(L"\\??\\SysmonV2");

	// remove the symbolic link
	IoDeleteSymbolicLink(&SymlinkName);

	// deallocate the device object
	IoDeleteDevice(pDriverObject->DeviceObject);

	FlushQueueSafe(&g_GlobalState.ProcessEventQueueHead, g_GlobalState.ProcessEventQueueLock);
	FlushQueueSafe(&g_GlobalState.ThreadEventQueueHead, g_GlobalState.ThreadEventQueueLock);
}

/* ----------------------------------------------------------------------------
 *	Create Dispatch
 */

_Use_decl_annotations_
NTSTATUS DispatchCreate(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	// unconditionally successful create request

	UNREFERENCED_PARAMETER(pDeviceObject);

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

/* ----------------------------------------------------------------------------
 *	Close Dispatch
 */

_Use_decl_annotations_
NTSTATUS DispatchClose(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	// unconditionally successful close request

	UNREFERENCED_PARAMETER(pDeviceObject);

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

/* ----------------------------------------------------------------------------
 *	Primary Device IO Control Dispatch
 */

_Use_decl_annotations_
NTSTATUS DispatchDeviceIoControl(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDeviceObject);

	auto status           = STATUS_SUCCESS;
	auto information      = 0;
	auto pIoStackLocation = IoGetCurrentIrpStackLocation(pIrp);
	auto ControlCode      = pIoStackLocation->Parameters.DeviceIoControl.IoControlCode;

	// NOTE: what if this fails?? bugcheck??
	NT_ASSERT(pIrp->MdlAddress);

	// grab the MDL buffer and cast to fit our needs
	auto buffer = static_cast<PUCHAR>(MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, NormalPagePriority));
	if (!buffer)
	{
		pIrp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		pIrp->IoStatus.Information = 0;

		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// get the size of the output buffer
	auto bufferSize = pIoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
	
	switch (ControlCode)
	{
	case IOCTL_SYSMONV2_QUERY_PROCESS_EVENTS:
	{
		Tuple<NTSTATUS, ULONG> res = FlushEventQueueToBufferSafe(
			&g_GlobalState.ProcessEventQueueHead,
			g_GlobalState.ProcessEventQueueLock,
			g_GlobalState.ProcessEventQueueCount,
			buffer,
			bufferSize
		);

		// structured bindings?
		status      = res.First();
		information = res.Second();

		break;
	}
	case IOCTL_SYSMONV2_QUERY_THREAD_EVENTS:
	{
		Tuple<NTSTATUS, ULONG> res = FlushEventQueueToBufferSafe(
			&g_GlobalState.ThreadEventQueueHead,
			g_GlobalState.ThreadEventQueueLock,
			g_GlobalState.ThreadEventQueueCount,
			buffer,
			bufferSize
		);

		// structured bindings?
		status      = res.First();
		information = res.Second();

		break;
	}
	default:
	{
		status      = STATUS_INVALID_DEVICE_REQUEST;
		information = 0;
		break;
	}
	}

	pIrp->IoStatus.Status      = status;
	pIrp->IoStatus.Information = information;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return status;
}

/* ----------------------------------------------------------------------------
 *	Process Event Handlers
 */

// process notification callback
VOID OnProcessNotify(
	PEPROCESS pProcess,
	HANDLE ProcessId,
	PPS_CREATE_NOTIFY_INFO pCreateInfo)
{
	UNREFERENCED_PARAMETER(pProcess);

	if (pCreateInfo)
	{
		// process creation
		HandleProcessCreate(ProcessId, pCreateInfo);
	}
	else
	{
		// process exit
		HandleProcessExit(ProcessId, pCreateInfo);
	}
}

VOID HandleProcessCreate(HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO pCreateInfo)
{
	USHORT CommandlineSize = 0;
	auto allocSize = sizeof(QUEUE_ITEM<ProcessCreateItem>);
	if (pCreateInfo->CommandLine)
	{
		CommandlineSize = pCreateInfo->CommandLine->Length;
		allocSize += CommandlineSize;
	}

	auto pQueueItem = static_cast<QUEUE_ITEM<ProcessCreateItem>*>(
		ExAllocatePoolWithTag(PagedPool, allocSize, SYSMONV2_ALLOC_TAG)
		);

	if (nullptr == pQueueItem)
	{
		KdPrint(("Failed to allocate memory [THIS IS REALLY BAD]\n"));
		return;
	}

	auto& Data = pQueueItem->Data;

	KeQuerySystemTimePrecise(&Data.Time);
	
	Data.Type = ItemType::ProcessCreate;
	Data.Size = sizeof(ProcessCreateItem) + CommandlineSize;
	Data.ProcessId = HandleToUlong(ProcessId);
	Data.ParentProcessId = HandleToULong(pCreateInfo->ParentProcessId);

	if (CommandlineSize > 0)
	{
		// commandline is present
		RtlCopyMemory(reinterpret_cast<PUCHAR>(&Data + sizeof(Data)), pCreateInfo->CommandLine, CommandlineSize);

		Data.CommandLineLength = CommandlineSize / sizeof(WCHAR);
		Data.CommandLineOffset = sizeof(Data);
	}
	else
	{
		// commandline is not present
		Data.CommandLineLength = 0;
		Data.CommandLineOffset = 0;
	}

	// insert the new item into the queue
	PushQueueSafe(
		&g_GlobalState.ProcessEventQueueHead,
		g_GlobalState.ProcessEventQueueLock,
		g_GlobalState.ProcessEventQueueCount,
		&pQueueItem->ListEntry
	);
}

VOID HandleProcessExit(HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO pCreateInfo)
{
	UNREFERENCED_PARAMETER(pCreateInfo);

	auto allocSize = sizeof(QUEUE_ITEM<ProcessExitItem>);
	auto pQueueItem = static_cast<QUEUE_ITEM<ProcessExitItem>*>(
		ExAllocatePoolWithTag(PagedPool, allocSize, SYSMONV2_ALLOC_TAG)
		);
	if (nullptr == pQueueItem)
	{
		KdPrint(("Failed to allocate memory [THIS IS REALLY BAD]\n"));
		return;
	}

	auto& Data = pQueueItem->Data;

	KeQuerySystemTimePrecise(&Data.Time);

	Data.Type = ItemType::ProcessExit;
	Data.Size = sizeof(ProcessExitItem);
	Data.ProcessId = HandleToULong(ProcessId);

	PushQueueSafe(
		&g_GlobalState.ProcessEventQueueHead,
		g_GlobalState.ProcessEventQueueLock,
		g_GlobalState.ProcessEventQueueCount,
		&pQueueItem->ListEntry
	);
}

/* ----------------------------------------------------------------------------
 *	Thread Event Handlers
 */

// thread notification callback
VOID OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN bCreate)
{
	if (bCreate)
	{
		// thread creation
		HandleThreadCreate(ProcessId, ThreadId);
	}
	else
	{
		// thread exit
		HandleThreadExit(ProcessId, ThreadId);
	}
}

VOID HandleThreadCreate(HANDLE ProcessId, HANDLE ThreadId)
{
	auto allocSize = sizeof(QUEUE_ITEM<ThreadCreateItem>);
	auto pQueueItem = static_cast<QUEUE_ITEM<ThreadCreateItem>*>(
		ExAllocatePoolWithTag(PagedPool, allocSize, SYSMONV2_ALLOC_TAG)
		);
	if (nullptr == pQueueItem)
	{
		KdPrint(("Failed to allocate memory [THIS IS REALLY BAD]\n"));
		return;
	}

	auto& Data = pQueueItem->Data;
	
	KeQuerySystemTimePrecise(&Data.Time);

	Data.Type = ItemType::ThreadCreate;
	Data.Size = sizeof(ThreadCreateItem);
	Data.ProcessId = HandleToULong(ProcessId);
	Data.ThreadId = HandleToUlong(ThreadId);
	
	PushQueueSafe(
		&g_GlobalState.ThreadEventQueueHead,
		g_GlobalState.ThreadEventQueueLock,
		g_GlobalState.ThreadEventQueueCount,
		&pQueueItem->ListEntry
	);
}

VOID HandleThreadExit(HANDLE ProcessId, HANDLE ThreadId)
{
	auto allocSize = sizeof(QUEUE_ITEM<ThreadCreateItem>);
	auto pQueueItem = static_cast<QUEUE_ITEM<ThreadCreateItem>*>(
		ExAllocatePoolWithTag(PagedPool, allocSize, SYSMONV2_ALLOC_TAG)
		);
	if (nullptr == pQueueItem)
	{
		KdPrint(("Failed to allocate memory [THIS IS REALLY BAD]\n"));
		return;
	}

	auto& Data = pQueueItem->Data;

	KeQuerySystemTimePrecise(&Data.Time);

	Data.Type = ItemType::ThreadExit;
	Data.Size = sizeof(ThreadExitItem);
	Data.ProcessId = HandleToULong(ProcessId);
	Data.ThreadId = HandleToUlong(ThreadId);

	PushQueueSafe(
		&g_GlobalState.ThreadEventQueueHead, 
		g_GlobalState.ThreadEventQueueLock, 
		g_GlobalState.ThreadEventQueueCount, 
		&pQueueItem->ListEntry
	);
}

/* ---------------------------------------------------------------------------- 
 *	Utility Functions
 */

// remove elements from queue one at a time, and serialize
// to the provided buffer, under lock
_Use_decl_annotations_
template<typename LockType>
Tuple<NTSTATUS, ULONG> FlushEventQueueToBufferSafe(
	PLIST_ENTRY pQueueHead,
	LockType& QueueLock,
	ULONG& QueueCount,
	PUCHAR buffer,
	ULONG bufferSize)
{
	auto status = STATUS_SUCCESS;
	ULONG information = 0;

	// handle case in which user does not provide a
	// sufficiently large buffer for all events
	auto bufferRemaining = bufferSize;

	AutoLock<LockType> locker(QueueLock);

	while (TRUE)
	{
		if (IsListEmpty(pQueueHead))
		{
			break;
		}

		auto pQueueEntry = RemoveHeadList(pQueueHead);
		auto pFullItem = CONTAINING_RECORD(pQueueEntry, QUEUE_ITEM<ItemHeader>, ListEntry);
		auto& Data = pFullItem->Data;

		auto itemSize = Data.Size;

		if (bufferRemaining < itemSize)
		{
			// user's buffer is full, insert item back into list
			InsertHeadList(pQueueHead, pQueueEntry);
			break;
		}

		// copy the item to the user buffer
		RtlCopyMemory(buffer, &Data, itemSize);

		// deallocate the removed item
		ExFreePool(pFullItem);

		// bookkeeping
		QueueCount--;
		bufferRemaining -= itemSize;
		buffer          += itemSize;
		information     += itemSize;
	}

	return Tuple<NTSTATUS, ULONG>{status, information};
}

// add a new element to the queue, under lock
_Use_decl_annotations_
template <typename LockType>
VOID PushQueueSafe(
	PLIST_ENTRY pQueueHead,
	LockType& QueueLock,
	ULONG& QueueCount,
	PLIST_ENTRY entry)
{
	AutoLock<LockType> lock(QueueLock);

	// NOTE: here we impose the limitation that only a fixed, maximum
	// number of events are maintained in the internal queue;
	// this maximum number may quickly be exhausted, given the number of events we register
	if (QueueCount > MAX_QUEUE_ITEMS)
	{
		// too many items, remove oldest
		auto head = RemoveHeadList(pQueueHead);
		QueueCount--;
		
		auto item = CONTAINING_RECORD(head, QUEUE_ITEM<ItemHeader>, ListEntry);
		ExFreePool(item);
	}

	InsertTailList(pQueueHead, entry);
	QueueCount++;
}

// remove all elements from queue and deallocate, under lock
template<typename LockType>
VOID FlushQueueSafe(const PLIST_ENTRY pQueueHead, LockType& QueueLock)
{
	AutoLock<LockType> locker(QueueLock);

	// empty the queue
	while (!IsListEmpty(pQueueHead))
	{
		auto pQueueEntry = RemoveHeadList(pQueueHead);
		auto pItem = CONTAINING_RECORD(pQueueEntry, QUEUE_ITEM<ItemHeader>, ListEntry);

		ExFreePool(pItem);
	}
}