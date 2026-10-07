#pragma once

/// \file
/// solvers::LocalSearch: one runner from an initial solution.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/solvers/initialization.hpp>
#include <easylocal/solvers/solver.hpp>

#include <concepts>
#include <cstdint>
#include <random>
#include <utility>

namespace easylocal::solvers
{

/// The simplest Solver: binds one Runner to an Input, builds an initial
/// solution, then runs the search once and returns its result.
///
/// The Solver owns the RNG, seeded with 0 unless `.seed(n)` says otherwise;
/// random initialization and random-aware algorithms consume the same stream,
/// so a seed reproduces the solve. `.initialization(tag)` chooses the initial
/// solution, `initialization::automatic` by default (random when the
/// SolutionManager builds random solutions); a tag the SolutionManager does not
/// support is rejected at compile time. Requires a runner whose SolutionManager
/// has initial_solution() or random_solution(rng).
template<class RunnerType, std::uniform_random_bit_generator RNG = std::mt19937_64>
class LocalSearch
    : public easylocal::detail::solver_start<
          easylocal::detail::bound_runner_t<RunnerType>,
          RNG>
{
    using start_type = easylocal::detail::solver_start<
        easylocal::detail::bound_runner_t<RunnerType>,
        RNG>;

public:
    /// The runner it binds to each Input.
    using runner_type = RunnerType;
    /// The random number generator it owns.
    using rng_type = RNG;
    /// The Input of the runner.
    using input_type = typename runner_type::input_type;
    /// The runner bound to an Input.
    using bound_runner_type = easylocal::detail::bound_runner_t<RunnerType>;
    /// The solution of the runner.
    using solution_type = typename bound_runner_type::solution_type;
    /// The cost of the runner, the cost of the result.
    using cost_type = typename bound_runner_type::cost_type;

    /// Whether the runner can build an initial solution (`initial_solution()`).
    using start_type::supports_initial;
    /// Whether the runner can build a random solution (`random_solution(rng)`).
    using start_type::supports_random;

    /// The same solver, with its RNG seeded with `seed`: this solver on an
    /// lvalue, the moved solver on a temporary. Each solve() continues the
    /// stream, so the seed reproduces the sequence of solves.
    using start_type::seed;
    /// The same solver, building its initial solution as `initialization`
    /// says: initialization::initial, random or automatic, rejected at compile
    /// time when the runner does not support it. This solver on an lvalue, the
    /// moved solver on a temporary.
    using start_type::initialization;
    /// The RNG, which feeds initialization and runs.
    using start_type::rng;

    /// From a runner, with an RNG seeded with 0.
    explicit LocalSearch(RunnerType runner)
        requires std::constructible_from<RNG, std::uint64_t>
        : LocalSearch(std::move(runner), RNG(std::uint64_t{0}))
    {
    }

    /// From a runner and the RNG it owns.
    LocalSearch(RunnerType runner, RNG rng)
        : start_type{std::move(rng)}, runner_{std::move(runner)}
    {
    }

    /// Solves from one initial solution.
    ///
    /// The optional trailing run options (easylocal::with(control, tracer),
    /// .stop_at(target)) go to the run.
    template<class... Options>
        requires easylocal::detail::solve_options<Options...>
    [[nodiscard]]
    auto solve(const input_type& input, const Options&... options)
        requires easylocal::detail::solver_runnable<bound_runner_type, RNG, Options...> &&
                 (supports_initial || supports_random)
    {
        auto bound_runner = runner_.bind(input);
        auto solution = this->make_initial_solution(bound_runner);
        easylocal::detail::emit_run_context({}, 0, 0, options...);
        return easylocal::detail::run_with_solver_rng(
            bound_runner,
            std::move(solution),
            this->rng_,
            options...);
    }

    /// Its runner's parameters (search.*, cost.*, neighborhood.*).
    [[nodiscard]]
    config::parameter_set configuration() &
    {
        config::parameter_set parameters;
        config::add_configuration(parameters, {}, runner_);
        return parameters;
    }

    /// Deleted: the set of a temporary would refer to it after it is gone.
    /// Configure the object that will run, after its last copy.
    config::parameter_set configuration() const&& = delete;

private:
    RunnerType runner_;
};

/// `LocalSearch{runner}` deduces the runner type, with the default RNG.
template<class RunnerType>
LocalSearch(RunnerType) -> LocalSearch<RunnerType>;

/// `LocalSearch{runner, rng}` deduces the runner and RNG types.
template<class RunnerType, std::uniform_random_bit_generator RNG>
LocalSearch(RunnerType, RNG) -> LocalSearch<RunnerType, RNG>;

} // namespace easylocal::solvers
