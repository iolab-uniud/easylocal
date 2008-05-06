/*
 *  Synchronize.hh
 *
 *  Created by Luca Di Gaspero on 27/09/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#ifdef HAVE_PTHREAD

#ifndef _MUTEX_HH
#define _MUTEX_HH

#include <EasyLocal.conf.hh>
#if defined(HAVE_CONFIG_H)
#include <config.hh>
#endif


#ifdef _MSC_VER
#define _AFXDLL
#else
#include <pthread.h>

class Mutex 
{
  pthread_mutex_t mutex;
public:
  Mutex();
  ~Mutex();
  void Lock();
  void Unlock();  
  bool TryLock();
};

#endif

#include <EasyLocal.conf.hh>
#if !defined(_CONDITIONVARIABLE_HH)
#define _CONDITIONVARIABLE_HH

#if defined(HAVE_CONFIG_H)
#include <config.hh>
#endif

#include <exception>
#include <stdexcept>

class TimeoutExpired : public std::exception
{};

#if defined(_MSC_VER)
#include <afxmt.h>
#else
#include <pthread.h> 
#endif

class ConditionVariable {
#if defined(_MSC_VER)
  CEvent event;
#else 
  pthread_mutex_t event_mutex;
  pthread_cond_t event;
  pthread_mutexattr_t attr;
#endif 
public:
  ConditionVariable(); 
	~ConditionVariable(); 
  void Wait();
  float WaitTimeout(float timeout) throw (TimeoutExpired, std::logic_error);
	void Signal();
	void Broadcast();	
};

#ifdef HAVE_PTHREAD

#ifndef _RWLOCKVARIABLE_H
#define _RWLOCKVARIABLE_H

#include <EasyLocal.conf.hh>

#include <pthread.h>

template <typename T>
class RWLockVariable 
{
	mutable pthread_rwlock_t rw_lock;
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
RWLockVariable<T>& RWLockVariable<T>::operator=(const RWLockVariable<T>& rw)
{
	write(rw.read());
	
	return *this;
}

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

#endif

#endif



#endif



#endif
#endif

