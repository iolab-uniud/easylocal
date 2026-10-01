#pragma once

#include <easylocal/runners/runner.hpp>

#include <concepts>
#include <utility>

// Common Solver infrastructure. A Solver orchestrates one or more Runners from
// an Input to a final solution; built-in solvers live in easylocal::solvers.
namespace easylocal
{

namespace detail
{

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

template<class BoundRunner, class RNG>
concept runner_with_rng =
    requires(
        BoundRunner& bound_runner,
        typename BoundRunner::solution_type solution,
        RNG& rng)
    {
        bound_runner.run(std::move(solution), rng);
    };

template<class BoundRunner>
concept runner_without_rng =
    requires(
        BoundRunner& bound_runner,
        typename BoundRunner::solution_type solution)
    {
        bound_runner.run(std::move(solution));
    };

template<class BoundRunner, class RNG>
concept solver_runnable =
    runner_with_rng<BoundRunner, RNG> || runner_without_rng<BoundRunner>;

template<class BoundRunner, class RNG>
[[nodiscard]]
auto run_with_solver_rng(
    BoundRunner& bound_runner,
    typename BoundRunner::solution_type solution,
    RNG& rng)
    requires solver_runnable<BoundRunner, RNG>
{
    if constexpr (runner_with_rng<BoundRunner, RNG>)
    {
        return bound_runner.run(std::move(solution), rng);
    }
    else
    {
        return bound_runner.run(std::move(solution));
    }
}

} // namespace detail

// Constructs a Solver from its arguments, e.g.
// make_solver<solvers::MultiStart>(runner, solvers::MultiStartConfig{...}).
// The Solver class template is its own key; its arguments are deduced.
template<template<class...> class Solver, class... Args>
    requires requires(Args&&... args) { Solver{std::forward<Args>(args)...}; }
[[nodiscard]]
auto make_solver(Args&&... args)
{
    return Solver{std::forward<Args>(args)...};
}

} // namespace easylocal
