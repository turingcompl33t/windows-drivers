/*
 * SyncHelpers.cpp
 * Synchronization helpers.
 */

#include "pch.h"
#include "SyncHelpers.h"

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
