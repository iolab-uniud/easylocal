#pragma once

/// \file
/// Common Solver infrastructure.
///
/// A Solver orchestrates one or more Runners from an Input to a final solution;
/// built-in solvers live in easylocal::solvers.

#include <easylocal/runners/pareto_archive.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/trace/events.hpp>
#include <easylocal/trace/tracer.hpp>

#include <algorithm>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace easylocal
{

namespace detail
{

// The runner bound to an Input, as a solver binds it.
template<class Runner>
using bound_runner_t = decltype(std::declval<Runner&>().bind(
    std::declval<const typename Runner::input_type&>()));

template<class BoundRunner>
concept bound_runner_with_initial_solution =
    requires(const BoundRunner& bound_runner) {
        { bound_runner.initial_solution() } ->
            std::same_as<typename BoundRunner::solution_type>;
    };

template<class BoundRunner, class RNG>
concept bound_runner_with_random_solution =
    requires(const BoundRunner& bound_runner, RNG& rng) {
        { bound_runner.random_solution(rng) } ->
            std::same_as<typename BoundRunner::solution_type>;
    };

template<class BoundRunner, class RNG, class... Options>
concept runner_with_rng =
    requires(
        BoundRunner& bound_runner,
        typename BoundRunner::solution_type solution,
        RNG& rng,
        const Options&... options)
    {
        bound_runner.run(std::move(solution), rng, options...);
    };

template<class BoundRunner, class... Options>
concept runner_without_rng =
    requires(
        BoundRunner& bound_runner,
        typename BoundRunner::solution_type solution,
        const Options&... options)
    {
        bound_runner.run(std::move(solution), options...);
    };

template<class BoundRunner, class RNG, class... Options>
concept solver_runnable =
    runner_with_rng<BoundRunner, RNG, Options...> ||
    runner_without_rng<BoundRunner, Options...>;

// The trailing arguments of Solver::solve(): nothing, or one run_options
// (easylocal::with(control, tracer), optionally .stop_at(target)).
template<class... Options>
concept solve_options =
    sizeof...(Options) == 0 ||
    (sizeof...(Options) == 1 && (is_run_options_v<std::remove_cvref_t<Options>> && ...));

template<class BoundRunner, class RNG, class... Options>
[[nodiscard]]
auto run_with_solver_rng(
    BoundRunner& bound_runner,
    typename BoundRunner::solution_type solution,
    RNG& rng,
    const Options&... options)
    requires solver_runnable<BoundRunner, RNG, Options...>
{
    if constexpr (runner_with_rng<BoundRunner, RNG, Options...>)
    {
        return bound_runner.run(std::move(solution), rng, options...);
    }
    else
    {
        return bound_runner.run(std::move(solution), options...);
    }
}

// True when the caller asked the solve to stop.
template<class... Options>
[[nodiscard]]
bool stop_requested(const Options&... options) noexcept
{
    return ((options.control != nullptr && options.control->stop_requested()) || ...);
}

// Sends the place of the next run (its stage and attempt) to the tracer of the
// run options, if they have one.
template<class... Options>
void emit_run_context(
    [[maybe_unused]] const std::string_view stage,
    [[maybe_unused]] const std::size_t stage_index,
    [[maybe_unused]] const std::size_t attempt,
    const Options&... options)
{
    if constexpr (sizeof...(Options) > 0)
    {
        const trace::event::run_context context{
            .stage = stage,
            .stage_index = stage_index,
            .attempt = attempt,
        };
        (
            [&context](const auto& run) {
                if (run.tracer != nullptr)
                    trace::emit(*run.tracer, context);
            }(options),
            ...);
    }
}

// The effort of several runs, for results that report it (search_result
// does; a custom result may not).
struct search_effort
{
    std::size_t evaluations{};
    std::size_t iterations{};

    template<class Result>
    void add(const Result& result) noexcept
    {
        if constexpr (requires { evaluations += result.evaluations; })
            evaluations += result.evaluations;
        if constexpr (requires { iterations += result.iterations; })
            iterations += result.iterations;
    }

    template<class Result>
    void assign_to(Result& result) const noexcept
    {
        if constexpr (requires { result.evaluations = evaluations; })
            result.evaluations = evaluations;
        if constexpr (requires { result.iterations = iterations; })
            result.iterations = iterations;
    }
};

// The caller's run control, from the run options of a solve (none without).
template<class... Options>
[[nodiscard]]
const run_control* control_of(const Options&... options) noexcept
{
    if constexpr (sizeof...(Options) == 0)
        return nullptr;
    else
        return (options.control, ...);
}

// The progress of a solve, as the caller's observer sees it: each run gets the
// caller's stop token with an observer that adds the evaluations and the
// iterations of the runs before it, so that the progress of MultiStart's
// starts and a pipeline's stages never goes back. Without an observer, the
// runs get the caller's control itself. It refers to itself: it is neither
// copied nor moved.
class solve_progress
{
public:
    explicit solve_progress(const run_control* caller) : caller_{caller}
    {
        if (caller_ != nullptr && caller_->observes_progress())
            control_.emplace(caller_->stop_token(), forwarder_);
    }

    solve_progress(const solve_progress&) = delete;
    solve_progress& operator=(const solve_progress&) = delete;
    solve_progress(solve_progress&&) = delete;
    solve_progress& operator=(solve_progress&&) = delete;
    ~solve_progress() = default;

    // The control of the next run.
    [[nodiscard]]
    const run_control* control() const noexcept
    {
        return control_.has_value() ? &*control_ : caller_;
    }

    // Counts the effort of a run that ended.
    template<class Result>
    void add(const Result& result) noexcept
    {
        before_.add(result);
    }

private:
    struct forwarder
    {
        const solve_progress* self;

        void operator()(const run_progress& progress) const
        {
            self->report(progress);
        }
    };

    void report(const run_progress& progress) const
    {
        caller_->report(
            run_progress{
                .evaluations = before_.evaluations + progress.evaluations,
                .iterations = before_.iterations + progress.iterations,
                .evaluation_limit = progress.evaluation_limit.has_value()
                    ? std::optional<
                          std::size_t>{before_.evaluations + *progress.evaluation_limit}
                    : std::nullopt,
            });
    }

    const run_control* caller_;
    search_effort before_;
    forwarder forwarder_{this};
    std::optional<run_control> control_;
};

// What is left of a solve's limits: its deadline and its evaluations (none:
// not bounded), and the progress of the solve, if it reports one. A solve
// gives each run what is left, and counts what the run used.
struct solve_budget
{
    std::optional<std::chrono::steady_clock::time_point> deadline;
    std::optional<std::size_t> evaluations;
    solve_progress* progress{};

    // The budget of a solve starting now, from its run options.
    template<class... Options>
    [[nodiscard]]
    static solve_budget of(const Options&... options)
    {
        solve_budget budget;
        (budget.take(options), ...);
        return budget;
    }

    // This budget, at most `limits`: the earlier deadline, the fewer
    // evaluations.
    [[nodiscard]]
    solve_budget within(const solve_budget& limits) const
    {
        solve_budget tighter = *this;
        if (limits.deadline
            && (!tighter.deadline || *limits.deadline < *tighter.deadline))
            tighter.deadline = limits.deadline;
        if (limits.evaluations
            && (!tighter.evaluations || *limits.evaluations < *tighter.evaluations))
        {
            tighter.evaluations = limits.evaluations;
        }
        return tighter;
    }

    [[nodiscard]]
    bool time_is_up() const
    {
        return deadline.has_value() && std::chrono::steady_clock::now() >= *deadline;
    }

    [[nodiscard]]
    bool evaluations_spent() const noexcept
    {
        return evaluations.has_value() && *evaluations == 0;
    }

    // Why the budget is spent, if it is.
    [[nodiscard]]
    std::optional<termination_reason> spent() const
    {
        if (evaluations_spent())
            return termination_reason::evaluation_budget_exhausted;
        if (time_is_up())
            return termination_reason::time_limit_reached;
        return std::nullopt;
    }

    // Counts the evaluations a run used, for results that report them.
    template<class Result>
    void consume(const Result& result) noexcept
    {
        if constexpr (requires { std::size_t{result.evaluations}; })
        {
            if (evaluations)
                *evaluations -= (std::min)(*evaluations, std::size_t{result.evaluations});
        }
    }

    // The run options of a run within the budget: the solve's own (or none),
    // with the time and the evaluations left as their limits, and the
    // control of the solve's progress.
    template<class... Options>
    [[nodiscard]]
    auto options_for_run(const Options&... options) const
    {
        auto limited = [&] {
            if constexpr (sizeof...(Options) == 0)
                return run_options<trace::null_tracer>{};
            else
                return (options, ...);
        }();
        if (deadline)
        {
            limited.time_limit = (std::max)(std::chrono::steady_clock::duration::zero(),
                *deadline - std::chrono::steady_clock::now());
        }
        limited.evaluation_limit = evaluations ? limit{*evaluations} : unlimited;
        if (progress != nullptr)
            limited.control = progress->control();
        return limited;
    }

private:
    template<class Options>
    void take(const Options& options)
    {
        if (options.time_limit)
            deadline = deadline_after(*options.time_limit);
        if (!options.evaluation_limit.is_unlimited())
            evaluations = options.evaluation_limit;
    }
};

// The front of several runs, for results that carry one (with a cost::pareto
// cost): every run's front merged into one archive, which becomes the front of
// the solver's result. Other results have nothing to merge.
template<class Result>
class merged_front
{
public:
    explicit merged_front(pareto_archive_parameters) noexcept {}

    template<class BoundRunner>
    void add(const BoundRunner&, const Result&) noexcept
    {
    }

    void assign_to(Result&) const noexcept {}
};

template<class Result>
    requires requires(const Result& result) {
        result.front.front().solution;
        result.front.front().cost;
    }
class merged_front<Result>
{
    using point_type = typename decltype(Result::front)::value_type;

public:
    explicit merged_front(const pareto_archive_parameters parameters)
        : archive_{parameters}
    {
    }

    template<class BoundRunner>
    void add(const BoundRunner& bound_runner, const Result& result)
    {
        for (const auto& point : result.front)
        {
            archive_.offer(
                point.solution,
                point.cost,
                bound_runner,
                same_solution_of(bound_runner));
        }
    }

    void assign_to(Result& result) const
    {
        result.front = archive_.sorted();
    }

private:
    pareto_archive<
        decltype(std::declval<point_type&>().solution),
        decltype(std::declval<point_type&>().cost)>
        archive_;
};

// What the archive keeps, from the run options of a solve (the defaults
// without them).
template<class... Options>
[[nodiscard]]
pareto_archive_parameters front_parameters_of(const Options&... options) noexcept
{
    if constexpr (sizeof...(Options) == 0)
        return {};
    else
        return (options.front, ...);
}

template<class Result>
void set_termination(Result& result, const termination_reason reason) noexcept
{
    if constexpr (requires { result.termination = reason; })
    {
        result.termination = reason;
    }
}

template<class Result>
[[nodiscard]]
std::optional<termination_reason> termination_of(const Result& result) noexcept
{
    if constexpr (requires { { result.termination } -> std::convertible_to<termination_reason>; })
    {
        return result.termination;
    }
    else
    {
        return std::nullopt;
    }
}

// Why a run ends the runs that would follow it (MultiStart's starts, the
// attempts of a pipeline stage), if it does: it was cancelled, reached the
// target or ran out of time.
template<class Result>
[[nodiscard]]
std::optional<termination_reason> ends_runs(const Result& result) noexcept
{
    const auto termination = termination_of(result);
    if (termination == termination_reason::cancelled
        || termination == termination_reason::target_reached
        || termination == termination_reason::time_limit_reached)
    {
        return termination;
    }
    return std::nullopt;
}

// What the attempts of run_attempts did: the best run, with the front of all
// of them; how many ran; their effort; and why they stopped, if a run or the
// budget says so.
template<class Result>
struct attempts_outcome
{
    Result best;
    std::size_t attempts{};
    search_effort effort;
    std::optional<termination_reason> termination;
};

// Up to `count` runs, run_attempt(index, budget) each, keeping the best by the
// bound runner's cost semantics: MultiStart's starts and a pipeline stage's
// attempts.
//
// The runs share `budget`, the solve's, tightened by `limits`, their own; both
// count what each run uses. They stop at a run for which ends(result) gives a
// reason, at a cancellation, or once the budget is spent. The termination is
// the reason that stopped them, else the last run's; the best result has it,
// and the front merged from every run.
template<class BoundRunner, class RunAttempt, class Ends, class... Options>
[[nodiscard]]
auto run_attempts(
    const BoundRunner& bound_runner,
    const std::size_t count,
    solve_budget& budget,
    const solve_budget& limits,
    const RunAttempt& run_attempt,
    const Ends& ends,
    const Options&... options)
{
    auto attempt_budget = budget.within(limits);
    auto best = run_attempt(std::size_t{0}, std::as_const(attempt_budget));
    using result_type = decltype(best);
    search_effort effort;
    merged_front<result_type> front{front_parameters_of(options...)};
    const auto account = [&](const result_type& result) {
        effort.add(result);
        front.add(bound_runner, result);
        attempt_budget.consume(result);
        budget.consume(result);
        if (budget.progress != nullptr)
            budget.progress->add(result);
    };
    account(best);
    auto termination = ends(best);
    auto last_termination = termination_of(best);
    std::size_t attempts = 1;
    for (; attempts < count && !termination; ++attempts)
    {
        if (stop_requested(options...))
        {
            termination = termination_reason::cancelled;
            break;
        }
        if (attempt_budget.spent())
            break;
        auto candidate = run_attempt(attempts, std::as_const(attempt_budget));
        account(candidate);
        termination = ends(candidate);
        last_termination = termination_of(candidate);
        if (bound_runner.better(candidate.cost, best.cost))
            best = std::move(candidate);
    }
    // The time or the evaluations ran out, before an attempt or during the
    // last one; or else the end of the last attempt.
    if (!termination)
        termination = attempt_budget.spent();
    if (!termination)
        termination = last_termination;
    if (termination)
        set_termination(best, *termination);
    front.assign_to(best);
    return attempts_outcome<result_type>{
        .best = std::move(best),
        .attempts = attempts,
        .effort = effort,
        .termination = termination,
    };
}

} // namespace detail

/// Constructs a Solver from its arguments, e.g.
/// `make_solver<solvers::MultiStart>(runner, solvers::MultiStartParameters{...})`.
///
/// The Solver class template is its own key; its arguments are deduced.
template<template<class...> class Solver, class... Args>
    requires requires(Args&&... args) { Solver{std::forward<Args>(args)...}; }
[[nodiscard]]
auto make_solver(Args&&... args)
{
    return Solver{std::forward<Args>(args)...};
}

} // namespace easylocal
