#pragma once

#include <ntddk.h>

// identifies the type of device
// (not so relevant for software drivers, like this one)
#define PRIORITY_BOOSTER_DEVICE 0x8000

// CTL_CODE is simply a macro that computes a control code, 
// which is a 32-bit value that specifies I/O information when 
// using the DeviceIoControl() method of interacting with a driver's device object
// CTL_CODE takes the following arguments:
//	- device type
//	- function 
//	- method
//	- access
#define IOCTL_PRIORITY_BOOSTER_SET_PRIORITY CTL_CODE(PRIORITY_BOOSTER_DEVICE, 0x800, METHOD_NEITHER, FILE_ANY_ACCESS)

// struct defines the format of the data the driver expects from clients
struct ThreadData {
	ULONG ThreadId;
	int Priority;
};
