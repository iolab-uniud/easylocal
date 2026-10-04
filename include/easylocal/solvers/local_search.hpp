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

template<class Initialization = initialization::Random>
struct LocalSearchConfig
{
    Initialization initialization{initialization::random};
    std::uint64_t seed{0};
};

/// The simplest Solver: bind one Runner to an Instance, construct the initial
/// solution according to the selected mode, then run the search. The Solver owns
/// the RNG; random initialization and random-aware algorithms consume the same
/// explicit stream, preserving deterministic replay from a seed.
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
    using runner_type = RunnerType;
    using rng_type = RNG;
    using input_type = typename runner_type::input_type;
    using bound_runner_type = easylocal::detail::bound_runner_t<RunnerType>;
    using solution_type = typename bound_runner_type::solution_type;

    using initialization_support::supports_initial;
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

    template<class Initialization, class Seed>
        requires std::constructible_from<RNG, Seed>
        && easylocal::detail::
            accepted_initialization<Initialization, bound_runner_type, RNG>
    LocalSearch(RunnerType runner, Initialization initialization, Seed seed)
        : LocalSearch(std::move(runner), initialization, RNG{std::move(seed)})
    {
    }

    template<class Initialization>
        requires std::constructible_from<RNG, std::uint64_t>
    LocalSearch(RunnerType runner, LocalSearchConfig<Initialization> config)
        : LocalSearch(std::move(runner), config.initialization, RNG{config.seed})
    {
    }

    [[nodiscard]]
    RNG& rng() noexcept
    {
        return rng_;
    }

    [[nodiscard]]
    const RNG& rng() const noexcept
    {
        return rng_;
    }

    /// Solves from one initial solution. The optional trailing run options
    /// (easylocal::with(control, tracer), .stop_at(target)) go to the run.
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

template<class RunnerType, class Initialization, class RNG>
LocalSearch(RunnerType, Initialization, RNG)
    -> LocalSearch<RunnerType, RNG>;

template<class RunnerType, class Initialization>
LocalSearch(RunnerType, LocalSearchConfig<Initialization>)
    -> LocalSearch<RunnerType>;

} // namespace easylocal::solvers
