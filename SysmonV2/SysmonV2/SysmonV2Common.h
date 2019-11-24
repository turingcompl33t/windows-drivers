// SysmonV2Common.h
// Common utilities for kernel driver and client application.

#pragma once

// from ntddk.h
#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)

#define SYSMONV2_DEVICE 0x8000

#define IOCTL_SYSMONV2_QUERY_PROCESS_EVENTS CTL_CODE(SYSMONV2_DEVICE, 0x800, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)
#define IOCTL_SYSMONV2_QUERY_THREAD_EVENTS CTL_CODE(SYSMONV2_DEVICE, 0x801, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)


enum class ItemType : USHORT
{
	None,
	ProcessCreate,
	ProcessExit,
	ThreadCreate,
	ThreadExit
};

// common header shared by all item types
struct ItemHeader
{
	ItemType      Type;  // type of event
	ULONG         Size;  // size of the record, in bytes
	LARGE_INTEGER Time;  // event timestamp
};

struct ProcessCreateItem : ItemHeader
{
	ULONG  ProcessId;
	ULONG  ParentProcessId;
	USHORT CommandLineLength;
	USHORT CommandLineOffset;
};

struct ProcessExitItem : ItemHeader
{
	ULONG ProcessId;
};

struct ThreadCreateItem : ItemHeader
{
	ULONG ThreadId;
	ULONG ProcessId;
};

struct ThreadExitItem : ItemHeader
{
	ULONG ThreadId;
	ULONG ProcessId;
};


