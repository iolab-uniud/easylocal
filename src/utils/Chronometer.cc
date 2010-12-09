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

#include "Chronometer.hh"
#include <ctime>

#if defined(_MSC_VER) 
// For the MS Visual C++ compiler
#include <windows.h>
union TimeConvert { FILETIME ftValue; __int64 i64Value; };
#endif
#if defined(CPUTIME)
#include <sys/resource.h>
#endif
#if defined(HAVE_GETTIMEOFDAY)
#include <sys/time.h>
#endif 
#include <stdexcept>

#if defined(_MSC_VER)
Chronometer::ClockTypes Chronometer::clock_type = MSWindows;
#else
#if defined(CPUTIME)
Chronometer::ClockTypes Chronometer::clock_type = CpuTime;
#elif defined(HAVE_CLOCK_GETTIME)
Chronometer::ClockTypes Chronometer::clock_type = ClockTime;
#elif defined(HAVE_GETTIMEOFDAY)
Chronometer::ClockTypes Chronometer::clock_type = TimeOfDay;
#endif
#endif

inline Chronometer::TimeValue Chronometer::TimeValue::ReadTime()
{
	TimeValue res;
#if defined(_MSC_VER)
  if (clock_type == MSWindows)
  {
    static FILETIME ftCreation, ftExit, ftKernel, ftUser;
    GetProcessTimes(GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser);
    TimeConvert tcKernel, tcUser;
    tcKernel.ftValue = ftKernel;
    tcUser.ftValue = ftUser;
    res.seconds = (unsigned long)((tcKernel.i64Value + tcUser.i64Value) / 10000000U);
    res.milli_seconds = (unsigned long)(((tcKernel.i64Value + tcUser.i64Value) % 10000000U) / 10000U);
    return res;
  }
#endif
#if defined(CPUTIME)
  if (clock_type == CpuTime)
  {
    static struct rusage time_read; 
    getrusage(RUSAGE_SELF,&time_read);
    res.seconds = time_read.ru_utime.tv_sec + time_read.ru_stime.tv_sec;
    res.milli_seconds = (time_read.ru_utime.tv_usec + time_read.ru_stime.tv_usec) / 1000U;
    getrusage(RUSAGE_CHILDREN,&time_read);
    res.seconds += time_read.ru_utime.tv_sec + time_read.ru_stime.tv_sec;
    res.milli_seconds += (time_read.ru_utime.tv_usec + time_read.ru_stime.tv_usec) / 1000U;
    return res;
  }
#endif
#if defined(HAVE_CLOCK_GETTIME)
  if (clock_type == ClockTime)
  {
     static struct timespec time_read; 
     clock_gettime(CLOCK_REALTIME, &time_read);
    res.seconds = time_read.tv_sec;
    res.milli_seconds = time_read.tv_nsec / 1000000U;	
    return res;
  }
#endif
#if defined(HAVE_GETTIMEOFDAY)
  if (clock_type == TimeOfDay)
  {
    struct timeval time_read;
    gettimeofday(&time_read, NULL);
    res.seconds = time_read.tv_sec;
    res.milli_seconds = time_read.tv_usec / 1000U;
    return res;
  }
#endif
  throw std::runtime_error("ClockType not supported in this implementation");
  return res;
}

Chronometer::Chronometer()
: running(false)
{}

void Chronometer::Reset()
{
	start.Reset();
	end.Reset();
	running = false;
}

void Chronometer::Start()
{
	start = TimeValue::ReadTime();
	running = true;
}


void Chronometer::Stop()
{
	if (running)
	  end = TimeValue::ReadTime();
	running = false;
}

double Chronometer::TotalTime() const
{
	if (!running)
		return (double)end - (double)start;
	else
	{
		TimeValue current = TimeValue::ReadTime();
		return (double)current - (double)start;
	}
}


std::string Chronometer::Now()
{
	time_t curr_time = time(0);
#if !defined(_MSC_VER)
	char *tmp = asctime(localtime(&curr_time));
#else
	char tmp[256];
	struct tm local_time;
	localtime_s(&local_time, &curr_time);
	asctime_s(tmp, 256, &local_time);
#endif
	for (unsigned int i = 0; tmp[i] != '\0'; i++)
		if (tmp[i] == '\n' && tmp[i+1] == '\0')
			tmp[i] = ' ';
	return tmp;
}
