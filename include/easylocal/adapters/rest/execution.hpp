#pragma once

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace easylocal::rest
{

[[nodiscard]] inline auto default_worker_count() noexcept -> std::size_t
{
    const auto available = std::thread::hardware_concurrency();
    if (available <= 1)
    {
        return 1;
    }
    return static_cast<std::size_t>(available - 1);
}

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

public:
    explicit execution_pool(
        std::size_t workers = default_worker_count(),
        std::size_t queue_capacity = 64)
        : queue_capacity_{std::max<std::size_t>(queue_capacity, 1)}
    {
        workers = std::max<std::size_t>(workers, 1);
        workers_.reserve(workers);
        for (std::size_t index = 0; index < workers; ++index)
        {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    execution_pool(const execution_pool&) = delete;
    auto operator=(const execution_pool&) -> execution_pool& = delete;
    execution_pool(execution_pool&&) = delete;
    auto operator=(execution_pool&&) -> execution_pool& = delete;

    ~execution_pool()
    {
        {
            const std::lock_guard lock{mutex_};
            stopping_ = true;
        }
        ready_.notify_all();
    }

    template<class Function>
        requires std::invocable<Function&>
    [[nodiscard]] auto try_submit(Function&& function) -> bool
    {
        {
            const std::lock_guard lock{mutex_};
            if (stopping_ || queue_.size() >= queue_capacity_)
            {
                return false;
            }
            queue_.emplace_back(std::forward<Function>(function));
        }
        ready_.notify_one();
        return true;
    }

    [[nodiscard]] auto worker_count() const noexcept -> std::size_t
    {
        return workers_.size();
    }

    [[nodiscard]] auto queue_capacity() const noexcept -> std::size_t
    {
        return queue_capacity_;
    }

private:
    void worker_loop() noexcept
    {
        while (true)
        {
            queued_task task;
            {
                std::unique_lock lock{mutex_};
                ready_.wait(lock, [this] {
                    return stopping_ || !queue_.empty();
                });

                if (stopping_ && queue_.empty())
                {
                    return;
                }

                task = std::move(queue_.front());
                queue_.pop_front();
            }

            // A failed job must not kill the worker.  Application-specific
            // task state is responsible for recording/reporting exceptions.
            try
            {
                task();
            }
            catch (...)
            {
            }
        }
    }

    std::size_t queue_capacity_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<queued_task> queue_;
    bool stopping_{};
    std::vector<std::jthread> workers_;
};

} // namespace easylocal::rest
