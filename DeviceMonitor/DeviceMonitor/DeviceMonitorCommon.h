// DeviceMonitorCommon.h
// Definitions of common types utilized by both kernel driver and client application.

#pragma once

// from ntifs.h
#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)

#define IOCTL_DEVMON_ADD_DEVICE \
	CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DEVMON_REMOVE_DEVICE \
	CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DEVMON_REMOVE_ALL \
	CTL_CODE(0x8000, 0x802, METHOD_NEITHER, FILE_ANY_ACCESS)
