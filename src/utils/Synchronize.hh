// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2008 Andrea Schaerf, Luca Di Gaspero. 
//
// EasyLocalpp is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// EasyLocalpp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with EasyLocalpp. If not, see <http://www.gnu.org/licenses/>.

// The multi-threading capabilities do not work under Visual C++ yet.
// FIXME: they have to be fixed in a future release.
#if !defined(_MSC_VER) 
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
  T tmp = rw.read();
	write(tmp);
	
	return *this;
}

#endif // !defined(_SYNCHRONIZE_HH)

#endif // !defined(_MSC_VER)
