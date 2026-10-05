#pragma once

/// \file
/// solvers::MultiStart: the same runner from several independent initial
/// solutions, keeping the best result.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/solvers/initialization.hpp>
#include <easylocal/solvers/solver.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>

namespace easylocal::solvers
{

/// The parameters of MultiStart.
struct MultiStartParameters
{
    /// The total number of runs, at least 1 (not the runs after the first one).
    std::size_t starts{1};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"starts", &MultiStartParameters::starts>(
                "Independent runs; the best result is kept",
                config::range(1, easylocal::unlimited)));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

/// Repeatedly initializes and runs the same bound Runner, and returns the best
/// result according to the bound runner's cost semantics.
///
/// `starts` denotes the total number of runs (not the number of runs after a
/// first one). The Solver owns the RNG, seeded with 0 unless `.seed(n)` says
/// otherwise; `.initialization(tag)` chooses how each start is built,
/// `initialization::automatic` by default (random when the SolutionManager
/// builds random solutions), a tag the SolutionManager does not support being
/// rejected at compile time. Requires a runner whose SolutionManager has
/// initial_solution() or random_solution(rng), and a cost with better().
template<class RunnerType, std::uniform_random_bit_generator RNG = std::mt19937_64>
class MultiStart
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
    /// The cost of the runner.
    using cost_type = typename bound_runner_type::cost_type;

    /// Whether the runner can build an initial solution (`initial_solution()`).
    using start_type::supports_initial;
    /// Whether the runner can build a random solution (`random_solution(rng)`).
    using start_type::supports_random;

    /// The same solver, with its RNG seeded with `seed`: this solver on an
    /// lvalue, the moved solver on a temporary. Each solve() continues the
    /// stream, so the seed reproduces the sequence of solves.
    using start_type::seed;
    /// The same solver, building its initial solutions as `initialization`
    /// says: initialization::initial, random or automatic, rejected at compile
    /// time when the runner does not support it. This solver on an lvalue, the
    /// moved solver on a temporary.
    using start_type::initialization;
    /// The RNG, which feeds initialization and runs.
    using start_type::rng;

    /// From a runner and the parameters, with an RNG seeded with 0.
    ///
    /// Throws `std::invalid_argument` when the parameters are not valid.
    explicit MultiStart(RunnerType runner, MultiStartParameters parameters = {})
        requires std::constructible_from<RNG, std::uint64_t>
        : MultiStart(std::move(runner), parameters, RNG{std::uint64_t{0}})
    {
    }

    /// From a runner, the parameters and the RNG it owns.
    ///
    /// Throws `std::invalid_argument` when the parameters are not valid.
    MultiStart(RunnerType runner, MultiStartParameters parameters, RNG rng)
        : start_type{std::move(rng)}, runner_{std::move(runner)}, parameters_{parameters}
    {
        validate_parameters();
    }

    /// Runs up to `starts` times from fresh solutions and returns the best
    /// result, with the effort of every start and, with a cost::pareto cost,
    /// the front merged from every start.
    ///
    /// The optional trailing run options go to every run; cancellation, a run
    /// that reaches the target or the time limit, or the solve's budget spent,
    /// ends the solve, and its termination says why (else it is the last
    /// start's).
    template<class... Options>
        requires easylocal::detail::solve_options<Options...>
    [[nodiscard]]
    auto solve(const input_type& input, const Options&... options)
        requires easylocal::detail::solver_runnable<bound_runner_type, RNG, Options...> &&
                 (supports_initial || supports_random) &&
                 requires(bound_runner_type& bound_runner, RNG& rng) {
                     { bound_runner.better(
                         std::declval<const cost_type&>(),
                         std::declval<const cost_type&>()) } ->
                         std::convertible_to<bool>;
                     requires easylocal::search_result_for<
                         decltype(easylocal::detail::run_with_solver_rng(
                             bound_runner,
                             std::declval<solution_type>(),
                             rng,
                             std::declval<const Options&>()...)),
                         solution_type,
                         cost_type>;
                 }
    {
        auto bound_runner = runner_.bind(input);
        // The solve's time limit and evaluation budget bound all the starts
        // together.
        auto budget = easylocal::detail::solve_budget::of(options...);
        easylocal::detail::solve_progress progress{
            easylocal::detail::control_of(options...)};
        budget.progress = &progress;
        auto outcome = easylocal::detail::run_attempts(
            bound_runner,
            parameters_.starts,
            budget,
            {},
            [&](const std::size_t start, const easylocal::detail::solve_budget& left) {
                auto solution = this->make_initial_solution(bound_runner);
                easylocal::detail::emit_run_context({}, 0, start, options...);
                return easylocal::detail::run_with_solver_rng(
                    bound_runner,
                    std::move(solution),
                    this->rng_,
                    left.options_for_run(options...));
            },
            [](const auto& result) { return easylocal::detail::ends_runs(result); },
            options...);
        outcome.effort.assign_to(outcome.best);
        return std::move(outcome.best);
    }

    /// Its own parameters (starts) and its runner's (search.*, cost.*,
    /// neighborhood.*), with paths relative to the solver.
    [[nodiscard]]
    config::parameter_set configuration() &
    {
        config::parameter_set parameters;
        parameters.add(parameters_);
        config::add_configuration(parameters, {}, runner_);
        return parameters;
    }

    /// Deleted: the set of a temporary would refer to it after it is gone.
    /// Configure the object that will run, after its last copy.
    config::parameter_set configuration() const&& = delete;

private:
    void validate_parameters() const
    {
        if (const auto validation = parameters_.validate(); !validation)
        {
            throw std::invalid_argument{std::string{validation.message}};
        }
    }

    RunnerType runner_;
    MultiStartParameters parameters_;
};

/// `MultiStart{runner}` deduces the runner type, with the default RNG.
template<class RunnerType>
MultiStart(RunnerType) -> MultiStart<RunnerType>;

/// `MultiStart{runner, parameters}` deduces the runner type, with the default
/// RNG.
template<class RunnerType>
MultiStart(RunnerType, MultiStartParameters) -> MultiStart<RunnerType>;

/// `MultiStart{runner, parameters, rng}` deduces the runner and RNG types.
template<class RunnerType, std::uniform_random_bit_generator RNG>
MultiStart(RunnerType, MultiStartParameters, RNG) -> MultiStart<RunnerType, RNG>;

} // namespace easylocal::solvers
