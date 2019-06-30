/*
 * SysMonCommon.cpp
 * Common structures shared between driver and client code. 
 */

#pragma once

// item type enumeration
enum class ItemType : short {
	None, 
	ProcessCreate, 
	ProcessExit, 
	ThreadCreate, 
	ThreadExit
};

// common item header
struct ItemHeader {
	ItemType      Type;
	USHORT        Size;
	LARGE_INTEGER Time;
};

// item to encapsulate process creation data 
struct ProcessCreateInfo : ItemHeader {
	ULONG   ProcessId;
	ULONG   ParentProcessId;
	USHORT  CommandLineLength;
	USHORT  CommandLineOffset;
};

// item to encapsuate process exit data 
struct ProcessExitInfo : ItemHeader {
	ULONG ProcessId;
};

// item to encapsulate data for both thread creation and exit
struct ThreadCreateExitInfo : ItemHeader {
	ULONG ThreadId;
	ULONG ProcessId;
};
