// SyncHelpers.cpp
// Synchronization helpers.

#include "SyncHelpers.h"

#pragma warning( disable : 28166 ) // C28166 changes IRQL and does not restore (doesn't like dtors)
#pragma warning( disable : 28167 ) // C28167 changes IRQL and does not restore (doesn't like dtors)

 /* ----------------------------------------------------------------------------
	 FastMutex
 */

void FastMutex::Init()
{
	ExInitializeFastMutex(&Mutex);
}

_Use_decl_annotations_
void FastMutex::Lock()
{
	ExAcquireFastMutex(&Mutex);
}

_Use_decl_annotations_
void FastMutex::Unlock()
{
	ExReleaseFastMutex(&Mutex);
}