// AsyncIO.h
// Driver that manages some async IO.

#pragma once

#include <ntddk.h>

#include "SyncHelpers.h"

// generic queue entry header 
struct QUEUE_ITEM_HEADER
{
	LIST_ENTRY ListEntry;
};

// individual read queue item
typedef struct _READ_QUEUE_ITEM 
	: QUEUE_ITEM_HEADER
{
	PIRP   pIrp;
	ULONG  ReadLength;
} READ_QUEUE_ITEM, *PREAD_QUEUE_ITEM;

// individual write queue item
typedef struct _WRITE_QUEUE_ITEM 
	: QUEUE_ITEM_HEADER
{
	PIRP   pIrp;
	ULONG  WriteLength;
} WRITE_QUEUE_ITEM, *PWRITE_QUEUE_ITEM;

// global state management 
struct GlobalState
{
	LIST_ENTRY ListHeadRead;
	LIST_ENTRY ListHeadWrite;

	ULONG ReadQueueCount;
	ULONG WriteQueueCount;

	// protects both queues, SAD!
	FastMutex  GlobalStateLock;
};

extern "C" DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD                DriverUnload;

_Dispatch_type_(IRP_MJ_CREATE)
_Function_class_(DRIVER_DISPATCH)
NTSTATUS DispatchCreate(PDEVICE_OBJECT, PIRP pIrp);

_Dispatch_type_(IRP_MJ_CLOSE)
_Function_class_(DRIVER_DISPATCH)
NTSTATUS DispatchClose(PDEVICE_OBJECT, PIRP pIrp);

_Dispatch_type_(IRP_MJ_READ)
_Function_class_(DRIVER_DISPATCH)
NTSTATUS DispatchRead(PDEVICE_OBJECT, PIRP pIrp);
NTSTATUS HandleReadNoPendingWrites(PIRP pIrp, PIO_STACK_LOCATION pIoStack);
NTSTATUS HandleReadPendingWriteAvailable(PIRP pIrp, PIO_STACK_LOCATION pIoStack);

_Dispatch_type_(IRP_MJ_WRITE)
_Function_class_(DRIVER_DISPATCH)
NTSTATUS DispatchWrite(PDEVICE_OBJECT, PIRP pIrp);
NTSTATUS HandleWriteNoPendingReads(PIRP pIrp, PIO_STACK_LOCATION pIoStack);
NTSTATUS HandleWritePendingReadAvailable(PIRP pIrp, PIO_STACK_LOCATION pIoStack);

_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
_Function_class_(DRIVER_DISPATCH)
NTSTATUS DispatchDeviceIoControl(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

_Function_class_(DRIVER_CANCEL)
VOID AsyncIoCancelRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

VOID PushItemReadQueueUnsafe(LIST_ENTRY* pListEntry);
VOID PushItemWriteQueueUnsafe(LIST_ENTRY* pListEntry);
PLIST_ENTRY PopItemReadQueueUnsafe();
PLIST_ENTRY PopItemWriteQueueUnsafe();

VOID FlushReadQueueUnsafe();
VOID FlushWriteQueueUnsafe();
