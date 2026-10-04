#pragma once

/// \file
/// solvers::LocalSearch: one runner from an initial solution.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/solvers/initialization.hpp>
#include <easylocal/solvers/solver.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <random>
#include <utility>

namespace easylocal::solvers
{

/// The configuration of LocalSearch: the initialization and the RNG seed.
template<class Initialization = initialization::Random>
struct LocalSearchConfig
{
    /// How the initial solution is built: a tag or an initialization::Mode.
    Initialization initialization{initialization::random};
    /// The seed of the solver's RNG.
    std::uint64_t seed{0};
};

/// The simplest Solver: bind one Runner to an Instance, construct the initial
/// solution according to the selected mode, then run the search.
///
/// The Solver owns the RNG; random initialization and random-aware algorithms
/// consume the same explicit stream, preserving deterministic replay from a
/// seed.
///
/// The Runner/SolutionManager type determines which initialization modes exist.
/// Static tags validate this at compile time; initialization::Mode provides the
/// same choice at runtime for CLI/configuration and is validated immediately.
template<class RunnerType, std::uniform_random_bit_generator RNG = std::mt19937_64>
class LocalSearch
    : public easylocal::detail::InitializationSupport<
          easylocal::detail::bound_runner_t<RunnerType>,
          RNG>
{
    using initialization_support = easylocal::detail::InitializationSupport<
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

    /// Whether the runner can build an initial solution (`initial_solution()`).
    using initialization_support::supports_initial;
    /// Whether the runner can build a random solution (`random_solution(rng)`).
    using initialization_support::supports_random;

    /// initialization: initialization::initial or random, rejected at compile
    /// time when the runner does not support it, or a Mode, checked here.
    template<class Initialization>
        requires easylocal::detail::
                     accepted_initialization<Initialization, bound_runner_type, RNG>
    LocalSearch(RunnerType runner, Initialization initialization, RNG rng)
        : initialization_support{initialization},
          runner_{std::move(runner)},
          rng_{std::move(rng)}
    {
    }

    /// From a runner, an initialization, and a seed that constructs the RNG.
    template<class Initialization, class Seed>
        requires std::constructible_from<RNG, Seed>
        && easylocal::detail::
            accepted_initialization<Initialization, bound_runner_type, RNG>
    LocalSearch(RunnerType runner, Initialization initialization, Seed seed)
        : LocalSearch(std::move(runner), initialization, RNG{std::move(seed)})
    {
    }

    /// From a runner and a LocalSearchConfig.
    template<class Initialization>
        requires std::constructible_from<RNG, std::uint64_t>
    LocalSearch(RunnerType runner, LocalSearchConfig<Initialization> config)
        : LocalSearch(std::move(runner), config.initialization, RNG{config.seed})
    {
    }

    /// The RNG, which feeds initialization and runs.
    [[nodiscard]]
    RNG& rng() noexcept
    {
        return rng_;
    }

    /// The RNG, which feeds initialization and runs.
    [[nodiscard]]
    const RNG& rng() const noexcept
    {
        return rng_;
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
        auto solution = this->make_initial_solution(bound_runner, rng_);
        return easylocal::detail::run_with_solver_rng(
            bound_runner,
            std::move(solution),
            rng_,
            options...);
    }

    /// Its runner's parameters (search.*, cost.*, neighborhood.*).
    [[nodiscard]]
    config::parameter_set configuration()
    {
        config::parameter_set parameters;
        config::add_configuration(parameters, {}, runner_);
        return parameters;
    }

private:
    RunnerType runner_;
    RNG rng_;
};

/// `LocalSearch{runner, initialization, rng}` deduces the runner and RNG types.
template<class RunnerType, class Initialization, class RNG>
LocalSearch(RunnerType, Initialization, RNG)
    -> LocalSearch<RunnerType, RNG>;

/// `LocalSearch{runner, config}` deduces the runner type, with the default RNG.
template<class RunnerType, class Initialization>
LocalSearch(RunnerType, LocalSearchConfig<Initialization>)
    -> LocalSearch<RunnerType>;

} // namespace easylocal::solvers
