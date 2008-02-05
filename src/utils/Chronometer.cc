#include "Chronometer.hh"

#include <ctime>

Chronometer::Chronometer()
: running(false), secs(0), microsecs(0), p_secs(0), p_microsecs(0), t_secs(0), t_microsecs(0)
{}

void Chronometer::Reset()
{
  t_secs = p_secs = 0;
  p_microsecs = t_microsecs = 0;
  running = false;
}

void Chronometer::Start()
{
#ifdef CPUTIME
  getrusage(RUSAGE_SELF,&time_read);
  secs = time_read.ru_utime.tv_sec;
  microsecs = time_read.ru_utime.tv_usec;
#else
#ifdef HAVE_CLOCK_GETTIME
  clock_gettime(CLOCK_REALTIME, &time_read);
  secs = time_read.tv_sec;
  microsecs = time_read.tv_nsec / 1000;
#else
#ifdef HAVE_GETTIMEOFDAY
  gettimeofday(&time_read, NULL);
  secs = time_read.tv_sec;
  microsecs = time_read.tv_usec;
#endif
#endif
#endif
  p_secs = 0;
  p_microsecs = 0;
  running = true;
}

void Chronometer::Partial()
{
  long r_secs, r_microsecs;
#ifdef CPUTIME
  getrusage(RUSAGE_SELF,&time_read);
  r_secs = time_read.ru_utime.tv_sec;
  r_microsecs = time_read.ru_utime.tv_usec;
#else
#ifdef HAVE_CLOCK_GETTIME
  clock_gettime(CLOCK_REALTIME, &time_read);
  r_secs = time_read.tv_sec;
  r_microsecs = time_read.tv_nsec / 1000;
#else
#ifdef HAVE_GETTIMEOFDAY
  gettimeofday(&time_read, NULL);
  r_secs = time_read.tv_sec;
  r_microsecs = time_read.tv_usec;
#endif
#endif
#endif
  p_secs = r_secs - secs;
  t_secs += p_secs;
  p_microsecs = r_microsecs - microsecs;
  t_microsecs += p_microsecs;
  running = false;
}

void Chronometer::Stop()
{
  if (running)
    Partial();
  running = false;
}

double Chronometer::TotalTime() const
{
  if (!running)
    return t_secs + t_microsecs / 1.0E6;
  else
  {
    long r_secs, r_microsecs;
#ifdef CPUTIME
    getrusage(RUSAGE_SELF, &time_read);
    r_secs = time_read.ru_utime.tv_sec;
    r_microsecs = time_read.ru_utime.tv_usec;
#else
#ifdef HAVE_CLOCK_GETTIME
    clock_gettime(CLOCK_REALTIME, &time_read);
    r_secs = time_read.tv_sec;
    r_microsecs = time_read.tv_nsec / 1000;
#else
#ifdef HAVE_GETTIMEOFDAY
    gettimeofday(&time_read, NULL);
    r_secs = time_read.tv_sec;
    r_microsecs = time_read.tv_usec;
#endif
#endif
#endif    
    return (t_secs + r_secs - secs) + (t_microsecs + r_microsecs - microsecs) / 1.0E6;
  }
}

double Chronometer::PartialTime() const
{
  if (!running)
    return p_secs + p_microsecs / 1.0E6;
  else
  {
    long r_secs, r_microsecs;
#ifdef CPUTIME
    getrusage(RUSAGE_SELF, &time_read);
    r_secs = time_read.ru_utime.tv_sec;
    r_microsecs = time_read.ru_utime.tv_usec;
#else
#ifdef HAVE_CLOCK_GETTIME
    clock_gettime(CLOCK_REALTIME, &time_read);
    r_secs = time_read.tv_sec;
    r_microsecs = time_read.tv_nsec / 1000;
#else
#ifdef HAVE_GETTIMEOFDAY
    gettimeofday(&time_read, NULL);
    r_secs = time_read.tv_sec;
    r_microsecs = time_read.tv_usec;
#endif
#endif
#endif 
    return (p_secs + r_secs - secs) + (p_microsecs + r_microsecs - microsecs) / 1.0E6;
  }
}

const char* Chronometer::Now()
{
    time_t curr_time = time(0);
    char *tmp = asctime(localtime(&curr_time));
    for (unsigned int i = 0; tmp[i] != '\0'; i++)
        if (tmp[i] == '\n' && tmp[i+1] == '\0')
            tmp[i] = ' ';
    return tmp;
}
