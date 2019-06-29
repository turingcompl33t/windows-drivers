/*
 * SysMon.h
 */

#pragma once

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
