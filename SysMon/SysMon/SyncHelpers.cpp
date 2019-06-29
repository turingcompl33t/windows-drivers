/*
 * SyncHelpers.cpp
 */

#include "SyncHelpers.h"

/* ----------------------------------------------------------------------------
	Mutex
*/

void Mutex::Init()
{
	KeInitializeMutex(&_mutex);
}

void Mutex::Lock()
{
	KeWaitForSingleObject(&_mutex, Executive, KernelMode, false, nullptr);
}

void Mutex::Unlock()
{
	KeReleaseMutex(&_mutex, false);
}

/* ----------------------------------------------------------------------------
	FastMutex 
*/

void FastMutex::Init()
{
	ExInitializeFastMutex(&_mutex);
}

void FastMutex::Lock()
{
	ExAcquireFastMutex(&_mutex);
}

void FastMutex::Unlock()
{
	ExReleaseFastMutex(&_mutex);
}
