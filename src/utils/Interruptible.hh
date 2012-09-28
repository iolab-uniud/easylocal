#ifndef EasyLocal_Interruptible_hh
#define EasyLocal_Interruptible_hh

#include <iostream>
#include <future>
#include <chrono>
#include <atomic>

/** A mixin class to add timeouts to anything. */
template <typename Rtype, typename ... Args>
class Interruptible {
  
public:
  
  /** Runs this interruptible synchronously for a specified number of milliseconds.
   @param timeout a duration in milliseconds
   @param args the list of arguments to pass (possibly empty)
   */
  Rtype SyncRun(std::chrono::milliseconds timeout, Args ... args)
  {
    timeout_expired = false;
    std::future<Rtype> result = std::async(std::launch::async, MakeFunction(), args ...);
    result.wait_for(timeout);
    if (result.wait_for(std::chrono::milliseconds::zero()) != std::future_status::ready)
      timeout_expired = true;
    return result.get();
  }
  
  /** Runs this interruptible asynchronously for a specified number of milliseconds.
   @param timeout a duration in milliseconds
   @param args the list of arguments to pass (possibly empty)
   */
  std::shared_future<Rtype> AsyncRun(std::chrono::milliseconds timeout, Args ... args)
  {
    timeout_expired = false;
    std::shared_future<Rtype> result = std::async(std::launch::async, MakeFunction(), args ...);
    std::thread t([this, timeout, result]() {
      std::this_thread::sleep_for(timeout);
      // If the function has not returned a value already
      if(result.wait_for(std::chrono::milliseconds::zero()) != std::future_status::ready)
        this->timeout_expired = true;
      
    });
    t.detach();
    return result;
  }
  
protected:
  
  inline const std::atomic<bool>& TimeoutExpired() { return timeout_expired; }
  
  /** Produces a function object to be launched in a separate thread. */
  virtual std::function<Rtype(Args ...)> MakeFunction() = 0;
  
private:
  
  /** Atomic flags. */
  std::atomic<bool> timeout_expired;
};

using namespace std;
using namespace std::chrono;

/** Test class. */
class Test : public Interruptible<long int, double, double> {
protected:
  
  std::function<long int(double,double)> MakeFunction() {
    return [this](double a, double b) -> long int {
      long int iterations = 0, r = -1;
      while(!TimeoutExpired() && iterations < (long int) b)
        r = ++iterations;
      if (TimeoutExpired())
        r = 0;
      return r;
    };
  }
};

#endif
