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

#include <utils/Synchronize.hh>

#if !defined(_MSC_VER)

#if defined(HAVE_PTHREAD)

#if !defined(HAVE_CLOCK_GETTIME) && !defined(HAVE_GETTIMEOFDAY)
#error "No gettime function is present, please configure the software with the --disable-threading option"
#endif

#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/errno.h>
#include <stdexcept>
#include <cmath>
#include <iostream>
#include <sstream>

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

ConditionVariable::ConditionVariable()
{
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
  pthread_mutex_init(&event_mutex, &attr);
  pthread_cond_init(&event, NULL);
}

ConditionVariable::~ConditionVariable() 
{
  pthread_mutex_destroy(&event_mutex);
  pthread_cond_destroy(&event);
  pthread_mutexattr_destroy(&attr);
}

void ConditionVariable::Wait() 
{
  int l_ret_code = pthread_mutex_lock(&event_mutex);
  int c_ret_code = pthread_cond_wait(&event, &event_mutex);  
  l_ret_code = pthread_mutex_unlock(&event_mutex);
  if (c_ret_code == EINVAL)
    throw std::logic_error("Invalid event mutex");
}

struct timespec compute_end_time(double time_left) {
  struct timespec ts_end;	
  const long NANOSEC_PER_MICROSEC = 1000;
  const long NANOSEC_PER_SEC = 1000000000;  
#if defined(HAVE_CLOCK_GETTIME)
  clock_gettime(CLOCK_REALTIME, &ts_end);
#elif defined(HAVE_GETTIMEOFDAY)
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  ts_end.tv_sec = tv_now.tv_sec;
  ts_end.tv_nsec = tv_now.tv_usec * NANOSEC_PER_MICROSEC;
#endif
//  std::cerr << "Now: " << ts_end.tv_sec << ' ' << ts_end.tv_nsec << std::endl;
  ts_end.tv_sec += (time_t)floor(time_left);
  ts_end.tv_nsec += (time_t)((time_left - floor(time_left)) * NANOSEC_PER_SEC);
  if (ts_end.tv_nsec > NANOSEC_PER_SEC)
  {
    ts_end.tv_sec += ts_end.tv_nsec / NANOSEC_PER_SEC;
    ts_end.tv_nsec %= NANOSEC_PER_SEC;
  }  
//  std::cerr << "End: " << ts_end.tv_sec << ' ' << ts_end.tv_nsec << std::endl;
  
  return ts_end;
}

#if defined(CPUTIME)

double ConditionVariable::WaitTimeout(double timeout) 
{
  const long MICROSEC_PER_SEC = 1000000;
  if (timeout <= 0.0)
    throw std::runtime_error("Error: trying to use a timeout value less or equal than zero");
  struct timespec ts_end;	
  struct rusage start, now;
  double elapsed_time, time_left = timeout;
  bool run_terminated = false;
  getrusage(RUSAGE_SELF, &start);
  do 
  {
    pthread_mutex_lock(&event_mutex);
    ts_end = compute_end_time(time_left);
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
        oss << "Invalid timeout " << time_left << " (" << ts_end.tv_sec << ":" << ts_end.tv_nsec << ")" << " or invalid mutex";
        throw std::logic_error(oss.str());
        break;
      }				
      case EPERM:
        throw std::runtime_error("The event_mutex was not owned by the thread");
        break;
      default:
        run_terminated = true;
    } 
    getrusage(RUSAGE_SELF, &now);
 //   std::cerr << "Sec: " << now.ru_utime.tv_sec << ' ' << start.ru_utime.tv_sec << std::endl;
    elapsed_time = (now.ru_utime.tv_sec - start.ru_utime.tv_sec) + (now.ru_utime.tv_usec - start.ru_utime.tv_usec) / (double)MICROSEC_PER_SEC;
 //   std::cerr << "Elapsed time: " << elapsed_time << std::endl;
    if (elapsed_time < timeout)
      time_left = timeout - elapsed_time;
    else
      throw TimeoutExpired();
 //   std::cerr << "Time left: " << time_left << std::endl;
  }
  while (!run_terminated);
  
  return time_left;
}

#else // no CPUTIME

double ConditionVariable::WaitTimeout(double timeout) 
{
  const long NANOSEC_PER_MICROSEC = 1000;
  const long NANOSEC_PER_SEC = 1000000000;
  if (timeout <= 0.0)
    throw std::logic_error("Error: trying to use a timeout value less or equal than zero");
  struct timespec ts_end, ts_now;
  pthread_mutex_lock(&event_mutex);
  ts_end = compute_end_time(timeout);
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
#if defined(HAVE_CLOCK_GETTIME)
  clock_gettime(CLOCK_REALTIME, &ts_now);
#elif defined(HAVE_GETTIMEOFDAY)
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  ts_now.tv_sec = tv_now.tv_sec;
  ts_now.tv_nsec = tv_now.tv_usec * NANOSEC_PER_MICROSEC;
#endif
  return (ts_end.tv_sec - ts_now.tv_sec) + (ts_end.tv_nsec - ts_now.tv_nsec) / (double)NANOSEC_PER_SEC;
}

#endif

void ConditionVariable::Signal() 
{
  pthread_mutex_lock(&event_mutex);
  pthread_cond_signal(&event);
  pthread_mutex_unlock(&event_mutex);
}

void ConditionVariable::Broadcast() 
{
  pthread_mutex_lock(&event_mutex);
  pthread_cond_broadcast(&event);
  pthread_mutex_unlock(&event_mutex);
}

#elif defined(_MSC_VER) // Visual C++ versions
#if defined(CPUTIME)
#warning "CPUTIME is not currently supported on windows platforms, using Wall-clock time instead"
#endif

Mutex::Mutex()
{
  InitializeCriticalSection(&mutex);
}

Mutex::~Mutex()
{
  DeleteCriticalSection(&mutex);
}

void Mutex::Lock()
{
  EnterCriticalSection(&mutex);
}

void Mutex::Unlock()
{
  LeaveCriticalSection(&mutex);
}

bool Mutex::TryLock()
{
  return TryEnterCriticalSection(&mutex);
}


ConditionVariable::ConditionVariable()
{
  InitializeCriticalSection(&event_mutex);
  InitializeConditionVariable(&event);  
}

ConditionVariable::~ConditionVariable()
{
  DeleteConditionVariable(&event);
  DeleteCriticalSection(&event_mutex);
}

void ConditionVariable::Wait() 
{
  EnterCriticalSection(&event_mutex);
  SleepConditionVariableCS(&event, &event_mutex, INFINITE);
  LeaveCriticalSection(&event_mutex);
}

double ConditionVariable::WaitTimeout(double timeout) 
{
  if (timeout <= 0.0)
    throw std::logic_error("Error: trying to use a timeout value less or equal than zero");
  chrono.Reset();
  chrono.Start();
  EnterCriticalSection(&event_mutex);
  if (!SleepConditionVariableCS(&event, &event_mutex, timeout * 1000.0))
    throw TimeoutExpired(); // FIXME: verify whether this behavior is correct
  LeaveCriticalSection(&event_mutex);
  chrono.Stop();
  return (timeout - chrono.TotalTime() < 0.0) ? 0.0 : (timeout - chrono.TotalTime());
}

void ConditionVariable::Signal() 
{  
  WakeConditionVariable(&event);
}

void ConditionVariable::Broadcast() 
{
  WakeAllConditionVariable(&event);
}



#endif

#endif // !defined(_MSC_VER)
