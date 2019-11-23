// SyncHelpers.h
// Synchronization helpers.

#pragma once

#include <ntddk.h>

 /* ----------------------------------------------------------------------------
	 FastMutex
 */

class FastMutex {
public:
	VOID Init();

	_IRQL_raises_(APC_LEVEL)
	_Acquires_lock_(this->Mutex)
	VOID Lock();

	_IRQL_requires_(APC_LEVEL)
	_Releases_lock_(this->Mutex)
	VOID Unlock();

private:
	FAST_MUTEX Mutex;
};

/* ----------------------------------------------------------------------------
	AutoLock
*/

template<typename TLock>
class AutoLock {
public:
	AutoLock(TLock& lock)
		: _lock(lock)
	{
		lock.Lock();
	}

	_Requires_lock_held_(this->_lock)
		~AutoLock()
	{
		_lock.Unlock();
	}

private:
	TLock& _lock;
};