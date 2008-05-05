/*
 *  Mutex.cpp
 *  ProvaThreads
 *
 *  Created by Luca Di Gaspero on 27/09/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include "Mutex.hh"

#ifdef HAVE_PTHREAD

#include <errno.h>

Mutex::Mutex()
{
  pthread_mutex_init(&mutex, NULL);
}

Mutex::~Mutex()
{
  pthread_mutex_destroy(&mutex);
}

void Mutex::Lock()
{
  pthread_mutex_lock(&mutex);
}

void Mutex::Unlock()
{
  pthread_mutex_unlock(&mutex);
}

bool Mutex::TryLock()
{
  return pthread_mutex_trylock(&mutex) == EBUSY;
}

#endif

