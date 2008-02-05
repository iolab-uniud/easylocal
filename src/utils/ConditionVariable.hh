/*
 *  ConditionVariable.hh
 *  ProvaThreads
 *
 *  Created by Luca Di Gaspero on 25/09/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_PTHREAD

#ifndef _CONDITIONVARIABLE_HH
#define _CONDITIONVARIABLE_HH

#include <pthread.h>
#include <exception>
#include <stdexcept>

class TimeoutExpired : public std::exception
{};

class ConditionVariable {
  pthread_mutex_t event_mutex;
  pthread_cond_t event;
	pthread_mutexattr_t attr;
public:
  ConditionVariable(); 
	~ConditionVariable(); 
  void Wait();
  float WaitTimeout(float timeout) throw (TimeoutExpired, std::logic_error);
	void Signal();
	void Broadcast();	
};

#endif

#endif
