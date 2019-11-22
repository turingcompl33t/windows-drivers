// AsyncIoCommon.h
// Common header for driver and client application.

#pragma once

// from ntddk.h
#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)

#define ASYNCIO_DEVICE 0x8000

#define IOCTL_ASYNCIO_QUERY_QUEUE_COUNTS        CTL_CODE(ASYNCIO_DEVICE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ASYNCIO_CANCEL_ALL_PENDING_IRPS   CTL_CODE(ASYNCIO_DEVICE, 0x801, METHOD_NEITHER, FILE_ANY_ACCESS)

constexpr auto MAX_READ_SIZE  = 12;
constexpr auto MAX_WRITE_SIZE = 12;

// marshalling queue count queries
typedef struct _MARSHAL_HELPER
{
	ULONG u1;
	ULONG u2;
} MARSHAL_HELPER, * PMARSHAL_HELPER;
