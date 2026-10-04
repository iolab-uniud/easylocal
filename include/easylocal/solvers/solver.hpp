#pragma once

/// \file
/// Common Solver infrastructure.
///
/// A Solver orchestrates one or more Runners from an Input to a final solution;
/// built-in solvers live in easylocal::solvers.

#include <easylocal/runners/runner.hpp>

#include <algorithm>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <optional>
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

// The run options with `target` as target cost.
template<class Target, class... Options>
[[nodiscard]]
auto with_target(Target target, const Options&... options)
{
    if constexpr (sizeof...(Options) == 0)
    {
        return easylocal::stop_at(std::move(target));
    }
    else
    {
        return (options.stop_at(std::move(target)), ...);
    }
}

// The deadline of a solve: its start plus the time limit of its options; none
// without a limit.
template<class... Options>
[[nodiscard]]
std::optional<std::chrono::steady_clock::time_point> solve_deadline(
    const Options&... options)
{
    std::optional<std::chrono::steady_clock::time_point> deadline;
    ((options.time_limit
             ? static_cast<void>(deadline = deadline_after(*options.time_limit))
             : static_cast<void>(0)),
        ...);
    return deadline;
}

// The earlier of two deadlines; none when neither is set.
[[nodiscard]]
inline std::optional<std::chrono::steady_clock::time_point> earliest(
    const std::optional<std::chrono::steady_clock::time_point> first,
    const std::optional<std::chrono::steady_clock::time_point> second)
{
    if (!first)
        return second;
    if (!second)
        return first;
    return std::min(*first, *second);
}

// Whether a deadline has passed.
[[nodiscard]]
inline bool time_is_up(
    const std::optional<std::chrono::steady_clock::time_point> deadline)
{
    return deadline.has_value() && std::chrono::steady_clock::now() >= *deadline;
}

// The run options of a run within a solve: the solve's own (or none), with the
// time left until `deadline` as their time limit.
template<class... Options>
[[nodiscard]]
auto within_deadline(
    const std::optional<std::chrono::steady_clock::time_point> deadline,
    const Options&... options)
{
    auto timed = [&] {
        if constexpr (sizeof...(Options) == 0)
            return run_options<trace::null_tracer>{};
        else
            return (options, ...);
    }();
    if (deadline)
    {
        timed.time_limit = std::max(
            std::chrono::steady_clock::duration::zero(),
            *deadline - std::chrono::steady_clock::now());
    }
    return timed;
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
        {
            evaluations += result.evaluations;
        }
        if constexpr (requires { iterations += result.iterations; })
        {
            iterations += result.iterations;
        }
    }

    template<class Result>
    void assign_to(Result& result) const noexcept
    {
        if constexpr (requires { result.evaluations = evaluations; })
        {
            result.evaluations = evaluations;
        }
        if constexpr (requires { result.iterations = iterations; })
        {
            result.iterations = iterations;
        }
    }
};

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

} // namespace detail

/// Constructs a Solver from its arguments, e.g.
/// make_solver<solvers::MultiStart>(runner, solvers::MultiStartConfig{...}).
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
