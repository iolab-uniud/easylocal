#pragma once

/// \file
/// execution_pool: the worker threads the REST adapter runs searches on, with
/// a bounded queue of pending runs.

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

namespace easylocal::rest
{

/// The default number of worker threads: one less than the hardware threads,
/// and at least 1.
[[nodiscard]] inline std::size_t default_worker_count() noexcept
{
    const auto available = std::thread::hardware_concurrency();
    if (available <= 1)
    {
        return 1;
    }
    return static_cast<std::size_t>(available - 1);
}

/// A fixed set of worker threads that run tasks taken from a bounded queue.
///
/// A task that throws does not stop its worker: the exception is ignored, and
/// the task is responsible for recording it. A task whose stop token is
/// stopped before a worker takes it never runs, and leaves its place in the
/// queue. Destruction refuses new tasks, lets the workers finish the queued
/// ones, and joins them.
class execution_pool
{
private:
#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
    using queued_task = std::move_only_function<void()>;
#else
    // std::move_only_function is a C++23 library feature, but some otherwise
    // C++23-capable standard libraries (notably Apple libc++ shipped with
    // current Xcode releases) do not provide it yet. std::packaged_task gives
    // us the same move-only, one-shot callable semantics for the queue.
    using queued_task = std::packaged_task<void()>;
#endif

    // A queued task, dropped once its stop token is stopped.
    struct queued_entry
    {
        queued_task task;
        std::stop_token cancel;
    };

public:
    /// From the number of worker threads and the capacity of the queue of
    /// pending tasks, both at least 1.
    explicit execution_pool(
        std::size_t workers = default_worker_count(),
        std::size_t queue_capacity = 64)
        : queue_capacity_{std::max<std::size_t>(queue_capacity, 1)}
    {
        workers = std::max<std::size_t>(workers, 1);
        try
        {
            workers_.reserve(workers);
            for (std::size_t index = 0; index < workers; ++index)
                workers_.emplace_back([this] { worker_loop(); });
        }
        catch (...)
        {
            // The workers already started must end, or joining them would
            // wait forever.
            stop();
            throw;
        }
    }

    execution_pool(const execution_pool&) = delete;
    execution_pool& operator=(const execution_pool&) = delete;
    execution_pool(execution_pool&&) = delete;
    execution_pool& operator=(execution_pool&&) = delete;

    ~execution_pool()
    {
        stop();
    }

    /// Queues a task, and returns whether it was accepted: false when the queue
    /// is full or the pool is being destroyed.
    ///
    /// Once cancel is stopped the task is dropped without running, if no
    /// worker has taken it yet; the tasks dropped do not count against the
    /// capacity.
    template<class Function>
        requires std::invocable<Function&>
    [[nodiscard]] bool try_submit(Function&& function, std::stop_token cancel = {})
    {
        {
            const std::lock_guard lock{mutex_};
            std::erase_if(queue_, [](const queued_entry& entry) {
                return entry.cancel.stop_requested();
            });
            if (stopping_ || queue_.size() >= queue_capacity_)
            {
                return false;
            }
            queue_.push_back(
                queued_entry{
                    .task = queued_task{std::forward<Function>(function)},
                    .cancel = std::move(cancel),
                });
        }
        ready_.notify_one();
        return true;
    }

    /// The number of worker threads.
    [[nodiscard]] std::size_t worker_count() const noexcept
    {
        return workers_.size();
    }

    /// The maximum number of tasks waiting in the queue.
    [[nodiscard]] std::size_t queue_capacity() const noexcept
    {
        return queue_capacity_;
    }

private:
    // Refuses new tasks and wakes the workers, which end once the queue is
    // empty.
    void stop() noexcept
    {
        {
            const std::lock_guard lock{mutex_};
            stopping_ = true;
        }
        ready_.notify_all();
    }

    void worker_loop() noexcept
    {
        while (true)
        {
            queued_entry entry;
            {
                std::unique_lock lock{mutex_};
                ready_.wait(lock, [this] {
                    return stopping_ || !queue_.empty();
                });

                if (stopping_ && queue_.empty())
                {
                    return;
                }

                entry = std::move(queue_.front());
                queue_.pop_front();
            }
            if (entry.cancel.stop_requested())
                continue;

            // A failed job must not kill the worker.  Application-specific
            // task state is responsible for recording/reporting exceptions.
            try
            {
                entry.task();
            }
            catch (...)
            {
            }
        }
    }

    std::size_t queue_capacity_;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<queued_entry> queue_;
    bool stopping_{};
    std::vector<std::jthread> workers_;
};

} // namespace easylocal::rest
