#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

namespace EasyLocal {
  namespace Experimental {
    template <typename T>
    class ThreadSafeQueue
    {
    public:
      /** Destructor */
      ~ThreadSafeQueue(void)
      {
        invalidate();
      }
      
      /** Attempt to get the first value in the queue.
       @return true if a value was successfully written to the out parameter, false otherwise
       */
      bool TryPop(T& out)
      {
        std::lock_guard<std::mutex> lock{m_mutex};
        if(m_queue.empty() || !m_valid)
          return false;
        out = std::move(m_queue.front());
        m_queue.pop();
        return true;
      }
      
      /**
       * Get the first value in the queue.
         Will block until a value is available unless clear is called or the instance is destructed.
       @return true if a value was successfully written to the out parameter, false otherwise.
       */
    }
  }
}
