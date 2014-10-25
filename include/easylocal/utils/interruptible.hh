#if !defined(_INTERRUPTIBLE_HH_)
#define _INTERRUPTIBLE_HH_

#include <iostream>
#include <future>
#include <chrono>
#include <atomic>
#include <thread>
#include <condition_variable>


namespace EasyLocal {

  namespace Core {
  
    /* Hack for simulating sleep_for when the compiler is not defininig it (e.g., macports g++ >= 4.6). */
#if HAVE_THIS_THREAD_SLEEP_FOR
    template <class Rep, class Period>
    void sleep_for(std::chrono::duration<Rep,Period> sleep_duration)
    {
      std::this_thread::sleep_for(sleep_duration);
    }
#else
    template <class Rep, class Period>
    void sleep_for(std::chrono::duration<Rep,Period> sleep_duration)
    {
      std::condition_variable c;
      std::mutex m;
      std::unique_lock<std::mutex> l(m);
      c.wait_for(l, sleep_duration);
    }
#endif


    /** A mixin class to add timeouts to anything. */
    template <typename Rtype, typename ... Args>
    class Interruptible
    {
    public:
  
      /** Constructor, sets timeout_expired to false to avoid problems when classes are called without threads. */
      Interruptible() : timeout_expired(false) {}

      /** Runs this interruptible synchronously for a specified number of milliseconds.
      @param timeout a duration in milliseconds
      @param args the list of arguments to pass (possibly empty)
      */
      Rtype SyncRun(std::chrono::milliseconds timeout, Args... args)
      {
        timeout_expired = false;
        std::future<Rtype> result = std::async(std::launch::async, this->MakeFunction(), std::ref(args) ...);
    
        // If timeout is greater than zero
        if (timeout.count() != 0)
        {
          result.wait_for(timeout);
          if (result.wait_for(std::chrono::milliseconds::zero()) != std::future_status::ready)
          {
            timeout_expired = true;
            AtTimeoutExpired();
          }
        }
        else
        {
          result.wait();
        }
        return result.get();
      }
  
      /** Runs this interruptible asynchronously for a specified number of milliseconds.
      @param timeout a duration in milliseconds
      @param args the list of arguments to pass (possibly empty)
      */
      std::shared_future<Rtype> AsyncRun(std::chrono::milliseconds timeout, Args... args)
      {
        timeout_expired = false;
        std::shared_future<Rtype> result = std::async(std::launch::async, this->MakeFunction(), std::ref(args) ...);
    
        // If timeout is greater than zero, launch stopper thread
        if (timeout.count() != 0)
        {
          std::thread t([this, timeout, result]()
          {
            sleep_for(timeout);
            // If the function has not returned a value already
            if (result.wait_for(std::chrono::milliseconds::zero()) == std::future_status::ready) {
              timeout_expired = true;
              AtTimeoutExpired();
            }
        
          });
          t.detach();
        }
        return result;
      }
  
      /** Interrupt execution. */
      inline void Interrupt() {
        timeout_expired = true;
      }
  
    protected:
  
      /** Checks if timeout has expired. */
      inline const std::atomic<bool>& TimeoutExpired() { return timeout_expired; }

      /** Produces a function object to be launched in a separate thread. */
      inline virtual std::function<Rtype(Args& ...)> MakeFunction() {
        // Default behavior
        return [](Args& ...) -> Rtype {
          return Rtype();
        };
      }
  
      /** Called when timeout is expired. */
      inline virtual void AtTimeoutExpired() { }
  
    private:
  
      /** Atomic flags. */
      std::atomic<bool> timeout_expired;
    };
  }
}

#endif
