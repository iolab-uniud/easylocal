#include <utils/Chronometer.hh>

#include <ctime>

#if defined(_MSC_VER) 
// For the MS Visual C++ compiler
#include <windows.h>
union TimeConvert { FILETIME ftValue; __int64 i64Value; };
#elif defined(CPUTIME)
#include <sys/resource.h>
#elif defined(HAVE_GETTIMEOFDAY)
#include <sys/time.h>
#endif 

inline Chronometer::TimeValue Chronometer::TimeValue::ReadTime()
{
	TimeValue res;
#if defined(_MSC_VER)
	static FILETIME ftCreation, ftExit, ftKernel, ftUser;
	GetProcessTimes(GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser);
	TimeConvert tcKernel, tcUser;
	tcKernel.ftValue = ftKernel;
	tcUser.ftValue = ftUser;
	res.seconds = (unsigned long)((tcKernel.i64Value + tcUser.i64Value) / 10000000U);
	res.milli_seconds = (unsigned long)(((tcKernel.i64Value + tcUser.i64Value) % 10000000U) / 10000U);
#elif defined(CPUTIME)
	static struct rusage time_read; 
	getrusage(RUSAGE_SELF,&time_read);
	res.seconds = time_read.ru_utime.tv_sec;
	res.milli_seconds = time_read.ru_utime.tv_usec / 1000U;
#elif defined(HAVE_CLOCK_GETTIME)
	clock_gettime(CLOCK_REALTIME, &time_read);
  res.seconds = time_read.tv_sec;
  res.milli_seconds = time_read.tv_nsec / 1000000U;	
#elif defined(HAVE_GETTIMEOFDAY)
	struct timeval time_read;
  gettimeofday(&time_read, NULL);
  res.seconds = time_read.tv_sec;
  res.milli_seconds = time_read.tv_usec / 1000U;
#endif
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
