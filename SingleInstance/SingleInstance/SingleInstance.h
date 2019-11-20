// SingleInstance.h
// A simple driver that only allows a single open device instance, system-wide.

#pragma once

#include <ntddk.h>

/* ----------------------------------------------------------------------------
	FastMutex
*/

class FastMutex {
public:
	void Init()
	{
		ExInitializeFastMutex(&_mutex);
	}

	_Acquires_lock_(this->_mutex)
	void Lock()
	{
		ExAcquireFastMutex(&_mutex);
	}

	_Releases_lock_(this->_mutex)
	void Unlock()
	{
		ExReleaseFastMutex(&_mutex);
	}

private:
	FAST_MUTEX _mutex;
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

	~AutoLock()
	{
		_lock.Unlock();
	}

private:
	TLock& _lock;
};

/* ----------------------------------------------------------------------------
	GlobalState
*/

struct GlobalState
{
	BOOLEAN   Flag;
	FastMutex Mutex;
};
