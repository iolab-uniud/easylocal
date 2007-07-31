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
    getrusage(RUSAGE_SELF,&time_read);
    secs = time_read.ru_utime.tv_sec;
    p_secs = 0;
    microsecs = time_read.ru_utime.tv_usec;
    p_microsecs = 0;
    running = true;
}

void Chronometer::Partial()
{
    getrusage(RUSAGE_SELF,&time_read);
    p_secs = time_read.ru_utime.tv_sec - secs;
    t_secs += p_secs;
    p_microsecs = time_read.ru_utime.tv_usec - microsecs;
    t_microsecs += p_microsecs;
    running = false;
}

void Chronometer::Stop()
{
    if (running)
    {
        getrusage(RUSAGE_SELF,&time_read);
        p_secs = time_read.ru_utime.tv_sec - secs;
        t_secs += p_secs;
        p_microsecs = time_read.ru_utime.tv_usec - microsecs;
        t_microsecs += p_microsecs;
    }
    running = false;
}

double Chronometer::TotalTime() const
{
    if (!running)
        return t_secs+t_microsecs/1E6;
    else
    {
        getrusage(RUSAGE_SELF,(struct rusage*)&time_read);
        return (t_secs + time_read.ru_utime.tv_sec - secs)+(t_microsecs + time_read.ru_utime.tv_usec - microsecs)/1E6;
    }
}

double Chronometer::PartialTime() const
{
    if (!running)
        return p_secs+p_microsecs/1E6;
    else
    {
        getrusage(RUSAGE_SELF,(struct rusage*)&time_read);
        return (p_secs + time_read.ru_utime.tv_sec - secs)+(p_microsecs + time_read.ru_utime.tv_usec - microsecs)/1E6;
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

