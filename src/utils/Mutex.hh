/*
 *  Mutex.hh
 *
 *  Created by Luca Di Gaspero on 27/09/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_PTHREAD

#ifndef _MUTEX_HH
#define _MUTEX_HH

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

#endif
