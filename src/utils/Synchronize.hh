/*
 *  Synchronize.hh
 *
 *  Created by Luca Di Gaspero on 27/09/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#if !defined(_SYNCHRONIZE_HH)
#define _SYNCHRONIZE_HH

#include <EasyLocal.conf.hh>
#if defined(HAVE_CONFIG_H)
#include <config.hh>
#endif
#include <exception>
#include <stdexcept>

#if defined(_MSC_VER)
#include <windows.h>
#include <utils/Chronometer.hh>
#else
#include <pthread.h>
#endif

class TimeoutExpired : public std::exception
{};

class ConditionVariable {
#if defined(_MSC_VER)
  CRITICAL_SECTION event_mutex;
	CONDITION_VARIABLE event;
  Chronometer chrono;
#else 
  pthread_mutex_t event_mutex;
  pthread_cond_t event;
  pthread_mutexattr_t attr;
#endif 
public:
  ConditionVariable(); 
	~ConditionVariable(); 
  void Wait();
  double WaitTimeout(double timeout);
	void Signal();
	void Broadcast();	
};

class Mutex 
{
#if defined(_MSC_VER)
  CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
  public:
    Mutex();
    ~Mutex();
    void Lock();
    void Unlock();  
    bool TryLock();
  };

template <typename T>
class RWLockVariable 
{
#if defined(_MSC_VER)
  SWRLOCK rw_lock;
#else
	mutable pthread_rwlock_t rw_lock;
#endif  
	T value;
	void write(const T& v);
  T read() const;
public:
  RWLockVariable();
	~RWLockVariable();	
	RWLockVariable<T>& operator=(const RWLockVariable<T>& rw);
  RWLockVariable<T>& operator=(const T& v);
  operator T () const;
};

#if !defined(_MSC_VER)
template <typename T>
RWLockVariable<T>::RWLockVariable()
{
	pthread_rwlock_init(&rw_lock, NULL);
}

template <typename T>
RWLockVariable<T>::~RWLockVariable()
{
	pthread_rwlock_destroy(&rw_lock);
}

template <typename T>
T RWLockVariable<T>::read() const
{
	pthread_rwlock_rdlock(&rw_lock);
	T v = value;
	pthread_rwlock_unlock(&rw_lock);
	return v;
}

template <typename T>
void RWLockVariable<T>::write(const T& v)
{
	pthread_rwlock_wrlock(&rw_lock);
	value = v;
	pthread_rwlock_unlock(&rw_lock);
}
#else // Visual C++ 

template <typename T>
RWLockVariable<T>::RWLockVariable()
{
	InitializeSRWLock(&rw_lock);
}

template <typename T>
RWLockVariable<T>::~RWLockVariable()
{
	DeleteSRWLock(&rw_lock);
}

template <typename T>
T RWLockVariable<T>::read() const
{
	AcquireSRWLockShared(&rw_lock);
	T v = value;
	ReleaseSRWLockShared(&rw_lock);
	return v;
}

template <typename T>
void RWLockVariable<T>::write(const T& v)
{
	AcquireSRWLockExclusive(&rw_lock);
	value = v;
	ReleaseSRWLockExclusive(&rw_lock);
}

#endif

// Common

template <typename T>
RWLockVariable<T>& RWLockVariable<T>::operator=(const T& v)
{	
	write(v);
	
	return *this;
}


template <typename T>
RWLockVariable<T>::operator T () const
{
	return read();
}

template <typename T>
RWLockVariable<T>& RWLockVariable<T>::operator=(const RWLockVariable<T>& rw)
{
	write(rw.read());
	
	return *this;
}

#endif // !defined(_SYNCHRONIZE_HH)
