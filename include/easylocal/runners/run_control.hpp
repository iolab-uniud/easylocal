#pragma once

/// \file
/// run_control: what the caller of a run controls while it runs, cancellation
/// through a std::stop_token, progress reports (evaluations, iterations,
/// budget) to an observer, and the best costs found to another.

#include <atomic>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <type_traits>
#include <utility>

namespace easylocal
{

namespace detail
{

// A distinct address for each cost type: the type an observer of the best
// costs receives.
template<class Cost>
inline constexpr char cost_type_tag{};

} // namespace detail

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
/// cancels it, an observer of its progress, and an observer of the best costs
/// it finds.
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

    /// The stop token through which the caller cancels the run.
    [[nodiscard]] std::stop_token stop_token() const noexcept
    {
        return stop_token_;
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

    /// The same control, with observer(progress) called with the progress
    /// instead; the observer must outlive the run.
    template<class Observer>
        requires std::invocable<Observer&, const run_progress&>
    [[nodiscard]]
    run_control observing_progress(Observer& observer) const noexcept
    {
        run_control control{stop_token_, observer};
        control.best_cost_type_ = best_cost_type_;
        control.best_cost_state_ = best_cost_state_;
        control.best_cost_observer_ = best_cost_observer_;
        return control;
    }

    /// Calls observer(cost) with each better cost of type Cost the run finds,
    /// from its first one; the observer must outlive the run.
    ///
    /// A run reports a cost better, by its cost semantics, than every one it
    /// reported before, and a solver's runs (the starts of MultiStart, the
    /// stages of a pipeline) one better than those of the runs before.
    /// The costs of another type, such as those of a pipeline stage on
    /// another cost, are not reported.
    template<class Cost, class Observer>
        requires std::invocable<Observer&, const Cost&>
    run_control& observe_best_cost(Observer& observer) noexcept
    {
        best_cost_type_ = &detail::cost_type_tag<Cost>;
        best_cost_state_ = std::addressof(observer);
        best_cost_observer_ = [](void* state, const void* cost) {
            std::invoke(*static_cast<Observer*>(state), *static_cast<const Cost*>(cost));
        };
        return *this;
    }

    /// Whether an observer receives the best costs of type Cost.
    template<class Cost>
    [[nodiscard]]
    bool observes_best_cost() const noexcept
    {
        return best_cost_type_ == &detail::cost_type_tag<Cost>;
    }

    /// Passes cost to the observer of the best costs, if it observes the costs
    /// of type Cost; the run decides that the cost is a better one.
    template<class Cost>
    void report_best_cost(const Cost& cost) const
    {
        if (observes_best_cost<Cost>())
            best_cost_observer_(best_cost_state_, std::addressof(cost));
    }

private:
    using observer_type = void (*)(void*, const run_progress&);
    using best_cost_observer_type = void (*)(void*, const void*);

    std::stop_token stop_token_{};
    void* observer_state_{};
    observer_type observer_{};
    const char* best_cost_type_{};
    void* best_cost_state_{};
    best_cost_observer_type best_cost_observer_{};
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

/// The best cost of a run, shared with another thread: the run's observer of
/// the best costs stores each one, a frontend loads a copy while the run goes
/// on.
///
/// A mutex guards the cost, which may be any copyable type.
template<std::copy_constructible Cost>
class shared_best_cost
{
public:
    /// Stores the best cost the run reports.
    void store(const Cost& cost)
    {
        const std::lock_guard lock{mutex_};
        cost_ = cost;
    }

    /// A copy of the cost last stored; empty before the first one.
    [[nodiscard]]
    std::optional<Cost> load() const
    {
        const std::lock_guard lock{mutex_};
        return cost_;
    }

private:
    mutable std::mutex mutex_;
    std::optional<Cost> cost_;
};

} // namespace easylocal
