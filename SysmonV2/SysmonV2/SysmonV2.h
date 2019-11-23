// SysmonV2.h
// Improved system monitoring driver.

#pragma once

#include <ntddk.h>

#include "SyncHelpers.h"

// maximum number of items allowed in the queue
constexpr auto MAX_QUEUE_ITEMS = 1024;

// tag for dynamic allocations
constexpr ULONG SYSMONV2_ALLOC_TAG = 0x13371337;

// global state manager
typedef struct _GLOBAL_STATE
{
	LIST_ENTRY ProcessEventQueueHead;
	LIST_ENTRY ThreadEventQueueHead;
	ULONG      ProcessEventQueueCount;
	ULONG      ThreadEventQueueCount;
	FastMutex  ProcessEventQueueLock;
	FastMutex  ThreadEventQueueLock;
} GLOBAL_STATE, *PGLOBAL_STATE;

// generic queue item
template <typename T>
struct QUEUE_ITEM
{
	LIST_ENTRY ListEntry;
	T          Data;
};

extern "C" DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD                DriverUnload;

VOID InitializeGlobalState();

_Dispatch_type_(IRP_MJ_CREATE)
_Function_class_(DRIVER_DISPATCH)
NTSTATUS DispatchCreate(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

_Dispatch_type_(IRP_MJ_CLOSE)
_Function_class_(DRIVER_DISPATCH)
NTSTATUS DispatchClose(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
_Function_class_(DRIVER_DISPATCH)
NTSTATUS DispatchDeviceIoControl(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

VOID OnProcessNotify(
	PEPROCESS pProcess, 
	HANDLE ProcessId, 
	PPS_CREATE_NOTIFY_INFO pCreateInfo);

VOID HandleProcessCreate(HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO pCreateInfo);
VOID HandleProcessExit(HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO pCreateInfo);

VOID OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN bCreate);

VOID HandleThreadCreate(HANDLE ProcessId, HANDLE ThreadId);
VOID HandleThreadExit(HANDLE ProcessId, HANDLE ThreadId);

_Requires_lock_not_held_(g_GlobalState.QueueLock)
template <typename LockType>
VOID PushQueueSafe(
	PLIST_ENTRY pQueueHead,
	LockType& QueueLock,
	ULONG& QueueCount,
	PLIST_ENTRY entry);

template<typename LockType>
VOID FlushQueueSafe(const PLIST_ENTRY pQueueHead, LockType& QueueLock);
