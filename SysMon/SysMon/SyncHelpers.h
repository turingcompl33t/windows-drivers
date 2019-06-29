/*
 * SyncHelpers.h
 * Synchronization helpers.
 */

#pragma once

/* ----------------------------------------------------------------------------
	FastMutex
*/

class FastMutex {
public:
	void Init();

	void Lock();
	void Unlock();

private:
	FAST_MUTEX _mutex;
};

/* ----------------------------------------------------------------------------
	AutoLock
*/

template<typename TLock>
class AutoLock {
public:
	AutoLock(TLock& lock) : _lock(lock)
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
