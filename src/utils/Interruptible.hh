#ifndef EasyLocal_Interruptible_hh
#define EasyLocal_Interruptible_hh

#include <iostream>
#include <future>
#include <chrono>
#include <atomic>
#include <thread>

/* hack for simulating sleep_for when the compiler is not defininig it (e.g., macports g++ >= 4.6) */
#if HAVE_THIS_THREAD_SLEEP_FOR
namespace _easylocal {
  template <class Rep, class Period>
  void sleep_for(std::chrono::duration<Rep,Period> sleep_duration)
  {
    std::this_thread::sleep_for(sleep_duration);
  }
};
#else
#include <condition_variable>
namespace _easylocal {
  template <class Rep, class Period>
  void sleep_for(std::chrono::duration<Rep,Period> sleep_duration)
  {
    std::condition_variable c;
    std::mutex m;
    std::unique_lock<std::mutex> l(m);
    c.wait_for(l, sleep_duration);
  }
};
#endif


/** A mixin class to add timeouts to anything. */
template <typename Rtype, typename ... Args>
class Interruptible {
  
public:
  
  /** Runs this interruptible synchronously for a specified number of milliseconds.
   @param timeout a duration in milliseconds
   @param args the list of arguments to pass (possibly empty)
   */
  Rtype SyncRun(std::chrono::milliseconds timeout, Args... args)
  {
    timeout_expired = false;
    std::future<Rtype> result = std::async(std::launch::async, this->MakeFunction(), std::ref(args) ...);
    result.wait_for(timeout);
    if (result.wait_for(std::chrono::milliseconds::zero()) != std::future_status::ready)
      timeout_expired = true;
    return result.get();
  }
  
  /** Runs this interruptible asynchronously for a specified number of milliseconds.
   @param timeout a duration in milliseconds
   @param args the list of arguments to pass (possibly empty)
   */
  std::shared_future<Rtype> AsyncRun(std::chrono::milliseconds timeout, Args... args)
  {
    timeout_expired = false;
    result = std::async(std::launch::async, this->MakeFunction(), std::ref(args) ...);
    std::thread t([this, timeout]() {
      _easylocal::sleep_for(timeout);
      // If the function has not returned a value already
      if(!this->HasReturned())
        this->timeout_expired = true;
      
    });
    t.detach();
    return result;
  }
  
protected:
  
  /** Checks if timeout has expired. */
  inline const std::atomic<bool>& TimeoutExpired() { return timeout_expired; }
  
  /** Checks if function has returned (naturally). */
  inline bool HasReturned() { return result.wait_for(std::chrono::milliseconds::zero()) == std::future_status::ready;  }
  
  /** Produces a function object to be launched in a separate thread. */
  inline virtual std::function<Rtype(Args& ...)> MakeFunction() {
    // Default behavior
    return [](Args& ...) -> Rtype {
      return Rtype();
    };
  }
  
private:
  
  /** Atomic flags. */
  std::atomic<bool> timeout_expired;
  
  /** For async. */
  std::shared_future<Rtype> result;
};

#endif
