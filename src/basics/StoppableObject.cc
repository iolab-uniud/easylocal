#include "StoppableObject.hh"

#include "EasyLocalSystemObjects.hh"
#ifdef EASYLOCAL_PTHREADS
#include <cerrno>
#include <cmath>
#include <sys/time.h> 
#include <sys/resource.h>
#include <signal.h>
#endif

#ifdef EASYLOCAL_PTHREADS
std::stack<StoppableObject*> StoppableObject::running_objects;

bool StoppableObject::sa_handler_set = false;
#endif

StoppableObject::StoppableObject()
        : timeout_raised(false), timeout(0)
{
#ifdef EASYLOCAL_PTHREADS
    pthread_mutex_init(&timeout_mutex, NULL);
    pthread_mutex_init(&terminate_mutex, NULL);
    terminated = false;
    pthread_cond_init(&terminate_cond, NULL);
#endif
}

StoppableObject::~StoppableObject()
{
#ifdef EASYLOCAL_PTHREADS
    pthread_mutex_destroy(&timeout_mutex);
    pthread_mutex_destroy(&terminate_mutex);
    pthread_cond_destroy(&terminate_cond);
#endif
}

void StoppableObject::RaiseTimeout()
{
#ifdef EASYLOCAL_PTHREADS
    pthread_mutex_lock(&timeout_mutex);
#ifdef DEBUG_PTHREADS
    std::cerr << "Raised timeout for " << this << "--- " << this->timeout << std::endl;
#endif
#endif
    timeout_raised = true;
#ifdef EASYLOCAL_PTHREADS
    pthread_mutex_unlock(&timeout_mutex);
#endif
}

void StoppableObject::ResetTimeout()
{
#ifdef EASYLOCAL_PTHREADS
    pthread_mutex_lock(&timeout_mutex);
#endif
    timeout_raised = false;
#ifdef EASYLOCAL_PTHREADS
    pthread_mutex_unlock(&timeout_mutex);
#endif 
}

#ifdef EASYLOCAL_PTHREADS
void StoppableObject::Starting()
{
    pthread_mutex_lock(&terminate_mutex);
    terminated = false;
    pthread_mutex_unlock(&terminate_mutex);
    pthread_mutex_lock(&timeout_mutex);
    timeout_raised = false;
    pthread_mutex_unlock(&timeout_mutex);
#ifdef DEBUG_PTHREADS
    std::cerr << "Starting " << pthread_self() << std::endl;
#endif
}

void StoppableObject::Terminating()
{
    pthread_mutex_lock(&terminate_mutex);
    terminated = true;
    pthread_mutex_unlock(&terminate_mutex);
    pthread_cond_broadcast(&terminate_cond);
#ifdef DEBUG_PTHREADS
    std::cerr << "Terminating " << pthread_self() << std::endl;
#endif
}

bool StoppableObject::Terminated()
{
    return terminated;
}

bool StoppableObject::WaitTermination(double timeout)
{
    if (timeout > 0.0)
    {
        struct timespec ts;
        struct timeval tp;
        int rc = 0, seconds, nseconds;

        seconds = floor(timeout);
        nseconds = floor((timeout - seconds)*1E6);

        pthread_mutex_lock(&terminate_mutex);
        while (!terminated && rc != ETIMEDOUT)
        {
            gettimeofday(&tp, NULL);
            ts.tv_sec  = tp.tv_sec;
            ts.tv_nsec = tp.tv_usec * 1000;
            ts.tv_sec += seconds;
            ts.tv_nsec += nseconds;
#ifdef DEBUG_PTHREADS
            std::cerr << "Waiting for " << timeout << std::endl;
#endif
            rc = pthread_cond_timedwait(&terminate_cond, &terminate_mutex, &ts);
#ifdef DEBUG_PTHREADS
            std::cerr << "Returned by timedwait " << rc << " " << pthread_self() << std::endl;
#endif
        }
        pthread_mutex_unlock(&terminate_mutex);

        return rc != ETIMEDOUT;
    }
    else
    {
        pthread_mutex_lock(&terminate_mutex);
        while (!terminated)
        {
            pthread_cond_wait(&terminate_cond, &terminate_mutex);
        }
        pthread_mutex_unlock(&terminate_mutex);

        return true;
    }
}


void StoppableObject::SetTimer() throw (EasyLocalException)
{
    struct sigaction sa;
    sa.sa_handler = &StoppableObject::_cpulimit_elapsed;
    sigaction(SIGVTALRM, &sa, NULL);
    sa_handler_set = true;

    getitimer(ITIMER_VIRTUAL, &timer);
#ifdef DEBUG_PTHREADS
    std::cerr << timer.it_interval.tv_sec << " " << timer.it_interval.tv_usec
    << "---" << timer.it_value.tv_sec << " " << timer.it_value.tv_usec << std::endl;
#endif
    timer.it_interval = timer.it_value;
    timer.it_value.tv_sec = this->timeout;
    timer.it_value.tv_usec = 0;
    setitimer(ITIMER_VIRTUAL, &timer, &old_timer);
#ifdef DEBUG_PTHREADS
    std::cerr << old_timer.it_interval.tv_sec << " " << old_timer.it_interval.tv_usec
    << "---" << old_timer.it_value.tv_sec << " " << old_timer.it_value.tv_usec << std::endl;
#endif
    StoppableObject::running_objects.push(this);
}

void StoppableObject::_cpulimit_elapsed(int signum)
{
#ifdef DEBUG_PTHREADS
    std::cerr << "CPU LIMIT CALLED " << pthread_self() << std::endl;
#endif
    assert(signum == SIGVTALRM);
    StoppableObject* so;
    if (StoppableObject::running_objects.empty())
    {
        std::cerr << "ERROR: empty running objects stack" << std::endl;
        exit(-1);
    }
    so = StoppableObject::running_objects.top();
    StoppableObject::running_objects.pop();
    if (so)
    {
        so->RaiseTimeout();
        //setitimer(ITIMER_VIRTUAL, &(so->old_timer), NULL);
    }
    else
        std::cerr << "ERROR: no object set for handling cpu limit" << std::endl;
}

#endif

void StoppableObject::SetTimeout(unsigned int t)
{
    timeout = t;
}

bool StoppableObject::Timeout()
{
    return timeout_raised;
}
