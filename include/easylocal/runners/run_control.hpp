#pragma once

/// \file
/// run_control: what the caller of a run controls while it runs, cancellation
/// through a std::stop_token and progress reports (evaluations, iterations,
/// budget) to an observer.

#include <atomic>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <type_traits>
#include <utility>

namespace easylocal
{

/// The progress of a run, as reported to the observer of its run_control.
struct run_progress
{
    /// Solutions and moves evaluated so far.
    std::size_t evaluations{};
    /// Iterations so far, as the algorithm counts them.
    std::size_t iterations{};
    /// The evaluation budget, when the run has one.
    std::optional<std::size_t> evaluation_limit;
};

/// What the caller of a run controls while it runs: a std::stop_token that
/// cancels it, and an observer of its progress.
///
/// It is passed to a run with with(control); a default one neither stops nor
/// observes.
class run_control
{
public:
    /// Neither cancellation nor observer.
    run_control() noexcept = default;

    /// Cancellation through stop_token.
    explicit run_control(std::stop_token stop_token) noexcept
        : stop_token_{std::move(stop_token)}
    {
    }

    /// Cancellation through stop_token, and observer(progress) called with
    /// the progress of the run; the observer must outlive the run.
    template<class Observer>
        requires std::invocable<Observer&, const run_progress&>
    run_control(std::stop_token stop_token, Observer& observer) noexcept
        : stop_token_{std::move(stop_token)},
          observer_state_{std::addressof(observer)},
          observer_{[](void* state, const run_progress& progress) {
              std::invoke(*static_cast<Observer*>(state), progress);
          }}
    {
    }

    /// Whether the caller asked the run to stop.
    [[nodiscard]] bool stop_requested() const noexcept
    {
        return stop_token_.stop_requested();
    }

    /// Whether the caller can ask the run to stop.
    [[nodiscard]] bool stop_possible() const noexcept
    {
        return stop_token_.stop_possible();
    }

    /// Whether an observer receives the progress.
    [[nodiscard]] bool observes_progress() const noexcept
    {
        return observer_ != nullptr;
    }

    /// Passes progress to the observer, if any.
    void report(const run_progress& progress) const
    {
        if (observer_ != nullptr)
        {
            observer_(observer_state_, progress);
        }
    }

private:
    using observer_type = void (*)(void*, const run_progress&);

    std::stop_token stop_token_{};
    void* observer_state_{};
    observer_type observer_{};
};

/// The progress of a run, shared with another thread: the run's observer stores
/// it, a frontend loads a copy while the run goes on.
///
/// Each counter is read on its own (relaxed), so a copy may mix two reports.
class shared_run_progress
{
public:
    /// Stores the progress the run reports.
    void store(const run_progress& progress) noexcept
    {
        evaluations_.store(progress.evaluations, std::memory_order_relaxed);
        iterations_.store(progress.iterations, std::memory_order_relaxed);
        evaluation_limit_.store(
            progress.evaluation_limit.value_or(0),
            std::memory_order_relaxed);
        has_evaluation_limit_.store(
            progress.evaluation_limit.has_value(),
            std::memory_order_relaxed);
    }

    /// A copy of the progress last stored.
    [[nodiscard]]
    run_progress load() const noexcept
    {
        return run_progress{
            .evaluations = evaluations_.load(std::memory_order_relaxed),
            .iterations = iterations_.load(std::memory_order_relaxed),
            .evaluation_limit = has_evaluation_limit_.load(std::memory_order_relaxed)
                ? std::optional<std::size_t>{evaluation_limit_.load(
                      std::memory_order_relaxed)}
                : std::nullopt,
        };
    }

private:
    std::atomic<std::size_t> evaluations_{};
    std::atomic<std::size_t> iterations_{};
    std::atomic<std::size_t> evaluation_limit_{};
    std::atomic_bool has_evaluation_limit_{};
};

} // namespace easylocal
