/*
 *  Mutex.hh
 *
 *  Created by Luca Di Gaspero on 27/09/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#ifdef _HAVE_EASYLOCALCONFIG
#include <EasyLocalConfig.hh>
#endif

#ifdef HAVE_PTHREAD

#ifndef _MUTEX_HH
#define _MUTEX_HH

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

#endif
#endif

