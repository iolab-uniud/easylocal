/*
 *  ConditionVariable.hh
 *  ProvaThreads
 *
 *  Created by Luca Di Gaspero on 25/09/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */


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

#endif
