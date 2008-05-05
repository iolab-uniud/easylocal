/*
 *  ConditionVariable.cpp
 *
 *  Created by Luca Di Gaspero on 25/09/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include "ConditionVariable.hh"

#ifdef HAVE_PTHREAD

#ifdef _MSC_VER
#define _AFXDLL
#else
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/errno.h>
#endif
#include <stdexcept>
#include <cmath>
#include <iostream>
#include <sstream>

ConditionVariable::ConditionVariable()
#ifdef _MSC_VER
: event(FALSE, FALSE)
{}
#else
{
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
  pthread_mutex_init(&event_mutex, &attr);
  pthread_cond_init(&event, NULL);
}
#endif

ConditionVariable::~ConditionVariable() 
{
#ifndef _MSC_VER
  pthread_mutex_destroy(&event_mutex);
  pthread_cond_destroy(&event);
  pthread_mutexattr_destroy(&attr);
#endif 
}

void ConditionVariable::Wait() 
{
#ifdef _MSC_VER
  // FIXME: to be handled correctly
#else 
  int l_ret_code = pthread_mutex_lock(&event_mutex);
  int c_ret_code = pthread_cond_wait(&event, &event_mutex);  
  l_ret_code = pthread_mutex_unlock(&event_mutex);
  if (c_ret_code == EINVAL)
    throw std::logic_error("Invalid event mutex");
#endif
}

#ifdef CPUTIME
float ConditionVariable::WaitTimeout(float timeout) throw (TimeoutExpired, std::logic_error)
{
	const long NANOSEC_PER_MICROSEC = 1000;
	const long MICROSEC_PER_SEC = 1000000;
  const long NANOSEC_PER_SEC = 1000000000;
  if (timeout <= 0.0)
    throw std::runtime_error("Error: trying to use a timeout value less or equal than zero");
  struct timespec ts_end;	
  struct rusage start, now;
  float elapsed_time, time_left = timeout;
  bool timeout_expired = false, run_terminated = false;
  getrusage(RUSAGE_SELF, &start);
  do 
    {
      pthread_mutex_lock(&event_mutex);
#ifdef HAVE_CLOCK_GETTIME 
      clock_gettime(CLOCK_REALTIME, &ts_end);
#else
#ifdef HAVE_GETTIMEOFDAY
      struct timeval tv_now;
      gettimeofday(&tv_now, NULL);
      ts_end.tv_sec = tv_now.tv_sec;
      ts_end.tv_nsec = tv_now.tv_usec * NANOSEC_PER_MICROSEC;
#else
#error "No gettime function is present, please configure the software with the --disable-threading option"
#endif
#endif
			ts_end.tv_sec += (time_t)floor(time_left);
			ts_end.tv_nsec += (time_t)(timeout - floor(time_left)) * NANOSEC_PER_SEC;
			if (ts_end.tv_nsec > NANOSEC_PER_SEC)
			{
				ts_end.tv_sec += 1;
				ts_end.tv_nsec %= NANOSEC_PER_SEC;
			}
      int c_ret_code = pthread_cond_timedwait(&event, &event_mutex, &ts_end);  
      pthread_mutex_unlock(&event_mutex);
      switch (c_ret_code)
			{
				case ETIMEDOUT: 
					timeout_expired = true;
					break;
				case EINVAL:
				{
					std::ostringstream oss;
					oss << "Invalid timeout " << timeout << " (" << ts_end.tv_sec << ":" << ts_end.tv_nsec << ")" << " or invalid mutex";
					throw std::logic_error(oss.str());
					break;
				}				
				case EPERM:
					throw std::runtime_error("The event_mutex was not owned by the thread");
					break;
				default:
					run_terminated = true;
			} 
      // 
      getrusage(RUSAGE_SELF, &now);
      //std::cerr << "Sec: " << now.ru_utime.tv_sec << ' ' << start.ru_utime.tv_sec << std::endl;
      elapsed_time = (now.ru_utime.tv_sec - start.ru_utime.tv_sec) + (now.ru_utime.tv_usec - start.ru_utime.tv_usec) / (double)MICROSEC_PER_SEC;
      //std::cerr << "Elapsed time: " << elapsed_time << std::endl;
      if (elapsed_time < timeout)
			{
				time_left = timeout - elapsed_time;
				timeout_expired = false;
			}
      else
			{
				time_left = 0.0;
				timeout_expired = true;
			}
    }
  while (!run_terminated && !timeout_expired);
  if (!run_terminated)
    throw TimeoutExpired();
	
  return time_left;
}
#else
float ConditionVariable::WaitTimeout(float timeout) throw (TimeoutExpired, std::logic_error)
{
#ifndef _MSC_VER
	const long NANOSEC_PER_MICROSEC = 1000;
  const long NANOSEC_PER_SEC = 1000000000;
  if (timeout <= 0.0)
    throw std::logic_error("Error: trying to use a timeout value less or equal than zero");
  struct timespec ts_end, ts_now;
  pthread_mutex_lock(&event_mutex);
#ifdef HAVE_CLOCK_GETTIME
  clock_gettime(CLOCK_REALTIME, &ts_end);
#else 
#ifdef HAVE_GETTIMEOFDAY
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  ts_end.tv_sec = tv_now.tv_sec;
  ts_end.tv_nsec = tv_now.tv_usec * NANOSEC_PER_MICROSEC;
#else
#error "No gettime function is present, please configure the software with the --disable-threading option"
#endif
#endif
  ts_end.tv_sec += (time_t)floor(timeout);
  ts_end.tv_nsec += (time_t)(timeout - floor(timeout)) * NANOSEC_PER_SEC;
	if (ts_end.tv_nsec > NANOSEC_PER_SEC)
	{
		ts_end.tv_sec += 1;
		ts_end.tv_nsec %= NANOSEC_PER_SEC;
	}
  int c_ret_code = pthread_cond_timedwait(&event, &event_mutex, &ts_end);  
  pthread_mutex_unlock(&event_mutex);
  switch (c_ret_code)
    {
    case ETIMEDOUT: 
      throw TimeoutExpired();
      break;
    case EINVAL:
      {
				std::ostringstream oss;
				oss << "Invalid timeout " << timeout << " (" << ts_end.tv_sec << ":" << ts_end.tv_nsec << ")" << " or invalid mutex";
				throw std::logic_error(oss.str());
				break;
      }
    case EPERM:
      throw std::logic_error("The event_mutex was not owned by the thread");
      break;
    } 
#ifdef HAVE_CLOCK_GETTIME
  clock_gettime(CLOCK_REALTIME, &ts_now);
#else 
#ifdef HAVE_GETTIMEOFDAY
  gettimeofday(&tv_now, NULL);
  ts_now.tv_sec = tv_now.tv_sec;
  ts_now.tv_nsec = tv_now.tv_usec * NANOSEC_PER_MICROSEC;
#endif
#endif
  return (ts_end.tv_sec - ts_now.tv_sec) + (ts_end.tv_nsec - ts_now.tv_nsec) / (double)NANOSEC_PER_SEC;
#else
	return 0;
#endif
}
#endif

void ConditionVariable::Signal() 
{
#ifndef _MSC_VER
  pthread_mutex_lock(&event_mutex);
  pthread_cond_signal(&event);
  pthread_mutex_unlock(&event_mutex);
#endif
}

void ConditionVariable::Broadcast() 
{
#ifndef _MSC_VER
  pthread_mutex_lock(&event_mutex);
  pthread_cond_broadcast(&event);
  pthread_mutex_unlock(&event_mutex);
#endif
}

#endif
