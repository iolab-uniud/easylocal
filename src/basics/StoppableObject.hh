#ifndef STOPPABLEOBJECT_HH_
#define STOPPABLEOBJECT_HH_

#include "EasyLocalException.hh"

#ifdef EASYLOCAL_PTHREADS
#include <pthread.h>
#include <stack>
#include <sys/time.h> 
#include <sys/resource.h>
#endif

class StoppableObject
{
    friend class EasyLocalSystemObjects;
public:
    virtual void RaiseTimeout();
protected:
    StoppableObject();
    virtual ~StoppableObject();
    void ResetTimeout();
    bool Timeout();
    void SetTimeout(unsigned int t);
    volatile bool timeout_raised;
    unsigned int timeout;
private:
    double end_time;
#ifdef EASYLOCAL_PTHREADS
    pthread_mutex_t timeout_mutex;
    pthread_mutex_t terminate_mutex;
    volatile bool terminated;
    pthread_cond_t terminate_cond;
    static void _cpulimit_elapsed(int signum);
protected:
    void Starting();
    void Terminating();
    static std::stack<StoppableObject*> running_objects;
    static bool sa_handler_set;
    struct itimerval timer, old_timer;
public:
    bool WaitTermination(double timeout);
    bool Terminated();
    void SetTimer() throw (EasyLocalException);
#endif
};

#endif /*STOPPABLEOBJECT_HH_*/
