/*
 * SysMon.h
 * Kernel driver that supports monitoring of system events via user-mode client.
 */

#pragma once

#include "SyncHelpers.h"

// for pool allocation tags 
#define DRIVER_TAG 'nmys'

template<typename T>
struct FullItem {
	LIST_ENTRY Entry;
	T          Data;
};

struct Globals {
	LIST_ENTRY ItemsHead;
	int        ItemCount;
	FastMutex  Mutex;
};
