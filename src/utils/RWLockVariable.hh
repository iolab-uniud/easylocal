/*
 *  RWLockVariable.hh
 *
 *  Created by Luca Di Gaspero on 25/09/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#ifdef _HAVE_EASYLOCALCONFIG
#include <EasyLocalConfig.hh>
#endif

#ifdef HAVE_PTHREAD

#ifndef _RWLOCKVARIABLE_H
#define _RWLOCKVARIABLE_H

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
