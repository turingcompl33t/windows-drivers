// AsyncIO.cpp
// Driver that manages some async IO.

#include <ntddk.h>

#include "AsyncIO.h"
#include "AsyncIoCommon.h"

#pragma warning( disable : 28166 ) // C28166 changes IRQL and does not restore (doesn't like dtors)
#pragma warning( disable : 28167 ) // C28167 changes IRQL and does not restore (doesn't like dtors)
#pragma warning( disable : 26110 ) // C26110 caller failing to hold global cancel spin lock

GlobalState g_GlobalState;

constexpr ULONG ASYNCIO_DRIVER_TAG = 0x11223344;

/* ----------------------------------------------------------------------------
 *	DriverEntry
 */

extern "C"
NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
	)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	auto status = STATUS_SUCCESS;

	// initialize internal state
	InitializeListHead(&g_GlobalState.ListHeadRead);
	InitializeListHead(&g_GlobalState.ListHeadWrite);
	
	g_GlobalState.ReadQueueCount = 0;
	g_GlobalState.WriteQueueCount = 0;

	g_GlobalState.GlobalStateLock.Init();

	BOOLEAN bSymlinkCreated = FALSE;
	PDEVICE_OBJECT pDeviceObject = nullptr;

	UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\AsyncIO");
	UNICODE_STRING SymlinkName = RTL_CONSTANT_STRING(L"\\??\\AsyncIO");

	do
	{
		// create device object
		status = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDeviceObject);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("Failed to IoCreateDevice()\n"));
			break;
		}

		// specify buffered IO for reads / writes
		pDeviceObject->Flags |= DO_BUFFERED_IO;

		// create symbolic link to device object
		status = IoCreateSymbolicLink(&SymlinkName, &DeviceName);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("Failed to IoCreateSymbolicLink()\n"));
			break;
		}

		bSymlinkCreated = TRUE;

	} while (false);

	// cleanup if we have failed
	if (!NT_SUCCESS(status))
	{
		if (bSymlinkCreated)
		{
			IoDeleteSymbolicLink(&SymlinkName);
		}

		if (nullptr != pDeviceObject)
		{
			IoDeleteDevice(pDeviceObject);
		}
	}

	// set up unload routine
	DriverObject->DriverUnload = DriverUnload;

	// set up dispatch routines
	DriverObject->MajorFunction[IRP_MJ_CREATE]         = DispatchCreate;
	DriverObject->MajorFunction[IRP_MJ_CLOSE]          = DispatchClose;
	DriverObject->MajorFunction[IRP_MJ_READ]           = DispatchRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE]          = DispatchWrite;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceIoControl;

	return STATUS_SUCCESS;
}

/* ----------------------------------------------------------------------------
 *	DriverUnload
 */

VOID DriverUnload(PDRIVER_OBJECT pDriverObject)
{
	UNICODE_STRING SymlinkName = RTL_CONSTANT_STRING(L"\\??\\AsyncIO");
	
	IoDeleteSymbolicLink(&SymlinkName);
	IoDeleteDevice(pDriverObject->DeviceObject);

	AutoLock<FastMutex> locker(g_GlobalState.GlobalStateLock);

	FlushReadQueueUnsafe();
	FlushWriteQueueUnsafe();
}

/* ----------------------------------------------------------------------------
 *	Dispatch Functions
 */

_Use_decl_annotations_
NTSTATUS DispatchCreate(PDEVICE_OBJECT, PIRP pIrp)
{
	// unconditionally successful device handle acquisition

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;

	IoCompleteRequest(pIrp, 0);
	
	return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS DispatchClose(PDEVICE_OBJECT, PIRP pIrp)
{
	// unconditionally successful device handle close

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;

	IoCompleteRequest(pIrp, 0);
	
	return STATUS_SUCCESS;
}

/* ----------------------------------------------------------------------------
 *	Read Dispatch
 */

_Use_decl_annotations_
NTSTATUS DispatchRead(PDEVICE_OBJECT, PIRP pIrp)
{
	// handle read requests

	// Driver recvs a read request:
	// - no need to check read length, we don't care
	// - check to see if there are queued write requests
	// - if there is a queued write, use the write to satisfy the read, and complete both requests
	// - else, queue the read, and defer the request with STATUS_PENDING 

	PIO_STACK_LOCATION pIoStack = IoGetCurrentIrpStackLocation(pIrp);
	if (0 == pIoStack->Parameters.Read.Length)
	{
		// zero-length reads are invalid

		pIrp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
		pIrp->IoStatus.Information = 0;

		IoCompleteRequest(pIrp, IO_NO_INCREMENT);

		return STATUS_INVALID_DEVICE_REQUEST;
	}

	// acquire the global state lock to protect queue operations
	AutoLock<FastMutex> locker(g_GlobalState.GlobalStateLock);

	if (IsListEmpty(&g_GlobalState.ListHeadWrite))
	{
		return HandleReadNoPendingWrites(pIrp, pIoStack);
	}
	else
	{
		return HandleReadPendingWriteAvailable(pIrp, pIoStack);
	}
}

// no pending writes available, queue the read 
NTSTATUS HandleReadNoPendingWrites(PIRP pIrp, PIO_STACK_LOCATION pIoStack)
{
	constexpr auto allocSize = sizeof(READ_QUEUE_ITEM);
	auto newItem = static_cast<PREAD_QUEUE_ITEM>(ExAllocatePoolWithTag(PagedPool, allocSize, ASYNCIO_DRIVER_TAG));
	if (nullptr == newItem)
	{
		KdPrint(("Failed to ExAllocatePoolWithTag() [THIS IS REALLY BAD]"));

		pIrp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		pIrp->IoStatus.Information = 0;

		IoCompleteRequest(pIrp, IO_NO_INCREMENT);

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// add the pointer to the IRP to the new item
	newItem->pIrp = pIrp;
	newItem->ReadLength = pIoStack->Parameters.Read.Length;

	// push the new entry onto the read queue
	PushItemReadQueueUnsafe(&newItem->ListEntry);
	g_GlobalState.ReadQueueCount++;

	IoMarkIrpPending(pIrp);

	// set the cancellation routine
	IoSetCancelRoutine(pIrp, AsyncIoCancelRoutine);

	return STATUS_PENDING;
}

NTSTATUS HandleReadPendingWriteAvailable(PIRP pIrp, PIO_STACK_LOCATION pIoStack)
{
	// remove an item from the write queue
	auto pListEntry = PopItemWriteQueueUnsafe();
	auto pWriteEntry = CONTAINING_RECORD(pListEntry, WRITE_QUEUE_ITEM, ListEntry);

	// prepare to complete the pending write request

	auto pWriteIrp = pWriteEntry->pIrp;
	const auto WriteLength = pWriteEntry->WriteLength;

	pWriteIrp->IoStatus.Status = STATUS_SUCCESS;
	pWriteIrp->IoStatus.Information = WriteLength;
	pWriteIrp->CancelRoutine = nullptr;

	// prepare to complete the read request

	const auto ReadLength = pIoStack->Parameters.Read.Length;

	// if the read is larger than the write from which the read is serviced,
	// only read up to the length of the original write request
	// otherwise, read the full requested read size
	const ULONG CopyLength = (ReadLength >= WriteLength) ? WriteLength : ReadLength;

	// perform the data transfer
	RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer, pWriteIrp->AssociatedIrp.SystemBuffer, CopyLength);

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = CopyLength;

	// complete both requests
	IoCompleteRequest(pWriteIrp, IO_NO_INCREMENT);
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	// decrement the number of pending writes
	g_GlobalState.WriteQueueCount--;

	// cleanup the popped entry
	ExFreePoolWithTag(pWriteEntry, ASYNCIO_DRIVER_TAG);

	return STATUS_SUCCESS;
}

/* ----------------------------------------------------------------------------
 *	Write Dispatch
 */

_Use_decl_annotations_
NTSTATUS DispatchWrite(PDEVICE_OBJECT, PIRP pIrp)
{
	// handle write requests

	// Driver recvs a write request:
	// - check to see if there are queued read requests
	// - if there is a queued read, use the write to satisfy the read, and complete both requests
	// - else, queue the write, and defer the request with STATUS_PENDING

	PIO_STACK_LOCATION pIoStack = IoGetCurrentIrpStackLocation(pIrp);
	if (0 == pIoStack->Parameters.Write.Length)
	{
		// zero-length writes are invalid

		pIrp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
		pIrp->IoStatus.Information = 0;

		IoCompleteRequest(pIrp, IO_NO_INCREMENT);

		return STATUS_INVALID_DEVICE_REQUEST;
	}

	// acquire the global state lock to protect queue operations
	AutoLock<FastMutex> locker(g_GlobalState.GlobalStateLock);

	if (IsListEmpty(&g_GlobalState.ListHeadRead))
	{
		return HandleWriteNoPendingReads(pIrp, pIoStack);
	}
	else
	{
		return HandleWritePendingReadAvailable(pIrp, pIoStack);
	}
}

// no pending reads available, queue the write
NTSTATUS HandleWriteNoPendingReads(PIRP pIrp, PIO_STACK_LOCATION pIoStack)
{
	constexpr auto allocSize = sizeof(WRITE_QUEUE_ITEM);
	auto newItem = static_cast<PWRITE_QUEUE_ITEM>(ExAllocatePoolWithTag(PagedPool, allocSize, ASYNCIO_DRIVER_TAG));
	if (nullptr == newItem)
	{
		KdPrint(("Failed to ExAllocatePoolWithTag() [THIS IS REALLY BAD]"));

		pIrp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		pIrp->IoStatus.Information = 0;

		IoCompleteRequest(pIrp, 0);

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// add the pointer to the IRP to the new item
	newItem->pIrp = pIrp;
	newItem->WriteLength = pIoStack->Parameters.Write.Length;

	// push the new item onto the queue
	PushItemWriteQueueUnsafe(&newItem->ListEntry);
	g_GlobalState.WriteQueueCount++;

	IoMarkIrpPending(pIrp);

	// set the cancellation routine
	IoSetCancelRoutine(pIrp, AsyncIoCancelRoutine);

	return STATUS_PENDING;
}

NTSTATUS HandleWritePendingReadAvailable(PIRP pIrp, PIO_STACK_LOCATION pIoStack)
{
	// pending read is available, complete both requests

	// remove the pending read entry from the queue
	auto pListEntry = PopItemReadQueueUnsafe();
	auto pReadEntry = CONTAINING_RECORD(pListEntry, READ_QUEUE_ITEM, ListEntry);

	// compute the transfer size

	const auto ReadSize = pReadEntry->ReadLength;
	const auto WriteSize = pIoStack->Parameters.Write.Length;

	const auto CopySize = (ReadSize >= WriteSize) ? WriteSize : ReadSize;

	// prepare to complete the pending read request

	auto pReadIrp = pReadEntry->pIrp;

	RtlCopyMemory(pReadIrp->AssociatedIrp.SystemBuffer, pIrp->AssociatedIrp.SystemBuffer, CopySize);

	pReadIrp->IoStatus.Status = STATUS_SUCCESS;
	pReadIrp->IoStatus.Information = CopySize;
	pReadIrp->CancelRoutine = nullptr;

	// prepare to complete the write request

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = CopySize;

	// complete both requests
	IoCompleteRequest(pReadIrp, IO_NO_INCREMENT);
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	// decrement the number of pending reads
	g_GlobalState.ReadQueueCount--;

	// cleanup the dequeued item
	ExFreePoolWithTag(pReadEntry, ASYNCIO_DRIVER_TAG);

	return STATUS_SUCCESS;
}

/* ----------------------------------------------------------------------------
 *	DeviceIoControl Dispatch
 */

_Use_decl_annotations_
NTSTATUS DispatchDeviceIoControl(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDeviceObject);

	auto status   = STATUS_SUCCESS;
	auto infoSize = 0;
	auto pIoStack = IoGetCurrentIrpStackLocation(pIrp);

	switch (pIoStack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_ASYNCIO_QUERY_QUEUE_COUNTS:
	{
		if (pIoStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(MARSHAL_HELPER))
		{
			// output buffer is too small to satisfy query
			status = STATUS_INVALID_BUFFER_SIZE;
			infoSize = 0;
			break;
		}

		ULONG ReadQueueCount  = 0;
		ULONG WriteQueueCount = 0;
		
		{
			AutoLock<FastMutex> locker(g_GlobalState.GlobalStateLock);
			ReadQueueCount  = g_GlobalState.ReadQueueCount;
			WriteQueueCount = g_GlobalState.WriteQueueCount;
		}

		auto marshaller = static_cast<PMARSHAL_HELPER>(pIrp->AssociatedIrp.SystemBuffer);
		marshaller->u1 = ReadQueueCount;
		marshaller->u2 = WriteQueueCount;

		status = STATUS_SUCCESS;
		infoSize = sizeof(MARSHAL_HELPER);
		
		break;
	}
	case IOCTL_ASYNCIO_CANCEL_ALL_PENDING_IRPS:
	{
		AutoLock<FastMutex> locker(g_GlobalState.GlobalStateLock);

		while (!IsListEmpty(&g_GlobalState.ListHeadRead))
		{
			auto pListEntry = PopItemReadQueueUnsafe();
			auto pReadEntry = CONTAINING_RECORD(pListEntry, READ_QUEUE_ITEM, ListEntry);

			// cancel the pended IRP
			IoCancelIrp(pReadEntry->pIrp);

			// and deallocate the queue entry
			ExFreePoolWithTag(pReadEntry, ASYNCIO_DRIVER_TAG);
		}

		while (!IsListEmpty(&g_GlobalState.ListHeadWrite))
		{
			auto pListEntry = PopItemWriteQueueUnsafe();
			auto pWriteEntry = CONTAINING_RECORD(pListEntry, READ_QUEUE_ITEM, ListEntry);

			// cancel the pended IRP
			IoCancelIrp(pWriteEntry->pIrp);

			// and deallocate the queue entry
			ExFreePoolWithTag(pWriteEntry, ASYNCIO_DRIVER_TAG);
		}

		g_GlobalState.ReadQueueCount  = 0;
		g_GlobalState.WriteQueueCount = 0;

		status = STATUS_SUCCESS;
		infoSize = 0;

		break;
	}
	default:
	{
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}
	}

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = infoSize;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return status;
}

_Use_decl_annotations_
VOID AsyncIoCancelRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDeviceObject);

	// release the global cancel spinlock, after changing cancellable state of the IRP
	IoReleaseCancelSpinLock(pIrp->CancelIrql);
	
	// NOTE: should remove from the internal queue here?

	// set the cancel routine to nullptr before relinquishing control of IRP
	pIrp->CancelRoutine = nullptr;

	// complete the IRP with CANCELLED status
	pIrp->IoStatus.Status = STATUS_CANCELLED;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
}

/* ----------------------------------------------------------------------------
 *	Utility Functions
 */

// IMPT: assumes lock is already held
VOID PushItemReadQueueUnsafe(LIST_ENTRY* pListEntry)
{
	InsertTailList(&g_GlobalState.ListHeadRead, pListEntry);
}

// IMPT: assumes lock is already held
VOID PushItemWriteQueueUnsafe(LIST_ENTRY* pListEntry)
{
	InsertTailList(&g_GlobalState.ListHeadWrite, pListEntry);
}

// IMPT: assumes lock is already held
PLIST_ENTRY PopItemReadQueueUnsafe()
{
	return RemoveHeadList(&g_GlobalState.ListHeadRead);
}

// IMPT: assumes lock is already held
PLIST_ENTRY PopItemWriteQueueUnsafe()
{
	return RemoveHeadList(&g_GlobalState.ListHeadWrite);
}

// flush the read queue
VOID FlushReadQueueUnsafe()
{
	while (!IsListEmpty(&g_GlobalState.ListHeadRead))
	{
		auto pListEntry = PopItemReadQueueUnsafe();
		auto pReadEntry = CONTAINING_RECORD(pListEntry, READ_QUEUE_ITEM, ListEntry);

		ExFreePoolWithTag(pReadEntry, ASYNCIO_DRIVER_TAG);
	}
}

// flush the write queue
VOID FlushWriteQueueUnsafe()
{
	while (!IsListEmpty(&g_GlobalState.ListHeadWrite))
	{
		auto pListEntry = PopItemWriteQueueUnsafe();
		auto pWriteEntry = CONTAINING_RECORD(pListEntry, WRITE_QUEUE_ITEM, ListEntry);

		ExFreePoolWithTag(pWriteEntry, ASYNCIO_DRIVER_TAG);
	}
}

constexpr ULONG GetDriverAllocTag()
{
	// TODO: placeholder
	return 0x11223344;
}

