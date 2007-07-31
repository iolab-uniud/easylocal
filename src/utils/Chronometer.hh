#ifndef CRONOMETER_HH_
#define CRONOMETER_HH_

#include <sys/resource.h>


/** A class for measuring CPU execution times: it relies on
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
    struct rusage time_read;
    long secs, microsecs, p_secs, p_microsecs, t_secs, t_microsecs;
};


#endif /*CRONOMETER_HH_*/
