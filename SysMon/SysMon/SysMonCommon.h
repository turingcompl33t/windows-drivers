/*
 * SysMonCommon.cpp
 */

#pragma once

#include <ntddk.h>

enum class ItemType : short {
	None, 
	ProcessCreate, 
	ProcessExit
};

struct ItemHeader {
	ItemType      Type;
	USHORT        Size;
	LARGE_INTEGER Time;
};

struct ProcessExitInfo : ItemHeader {
	ULONG ProcessId;
};
