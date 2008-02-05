#ifndef CHRONOMETER_HH_
#define CHRONOMETER_HH_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <sys/time.h>
#include <sys/resource.h>

/** A class for measuring execution times: it relies on
      standard C functions which are encapsulated. */

class Chronometer
{
public:
    Chronometer();
    void Reset();
    void Start();
    void Partial();
    void Stop();
    double TotalTime() const;
    double PartialTime() const;
    static const char* Now();
private:
    bool running;
#ifdef CPUTIME
    mutable struct rusage time_read;
#else
#ifdef HAVE_CLOCK_GETTIME
    mutable struct timespec time_read;
#else
#ifdef HAVE_GETTIMEOFDAY
    mutable struct timeval time_read;
#else
#error "No gettime function is present, please configure the software with the --disable-threading option"
#endif
#endif
#endif
    long secs, microsecs, p_secs, p_microsecs, t_secs, t_microsecs;
};

#endif /*CHRONOMETER_HH_*/
