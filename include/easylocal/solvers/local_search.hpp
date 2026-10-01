#pragma once

#include <easylocal/solvers/initialization.hpp>
#include <easylocal/solvers/solver.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace easylocal::solvers
{

template<class Initialization = initialization::Random>
struct LocalSearchConfig
{
    Initialization initialization{initialization::random};
    std::uint64_t seed{0};
};

// The simplest Solver: bind one Runner to an Instance, construct the initial
// solution according to the selected mode, then run the search. The Solver owns
// the RNG; random initialization and random-aware algorithms consume the same
// explicit stream, preserving deterministic replay from a seed.
//
// The Runner/SolutionManager type determines which initialization modes exist.
// Static tags validate this at compile time; initialization::Mode provides the
// same choice at runtime for CLI/configuration and is validated immediately.
template<
    class RunnerType,
    std::uniform_random_bit_generator RNG = std::mt19937_64>
class LocalSearch
{
public:
    using runner_type = RunnerType;
    using rng_type = RNG;
    using input_type = typename runner_type::input_type;
    using bound_runner_type = decltype(
        std::declval<runner_type&>().bind(
            std::declval<const input_type&>()));
    using solution_type = typename bound_runner_type::solution_type;

    static constexpr bool supports_initial =
        easylocal::detail::bound_runner_with_initial_solution<bound_runner_type>;
    static constexpr bool supports_random =
        easylocal::detail::bound_runner_with_random_solution<bound_runner_type, rng_type>;

    [[nodiscard]]
    static constexpr auto supports(const initialization::Mode mode) noexcept
        -> bool
    {
        switch (mode)
        {
        case initialization::Mode::initial:
            return supports_initial;
        case initialization::Mode::random:
            return supports_random;
        }
        return false;
    }

    LocalSearch(
        RunnerType runner,
        const initialization::Initial,
        RNG rng)
        requires supports_initial
        : runner_{std::move(runner)},
          initialization_mode_{initialization::Mode::initial},
          rng_{std::move(rng)}
    {
    }

    LocalSearch(
        RunnerType runner,
        const initialization::Random,
        RNG rng)
        requires supports_random
        : runner_{std::move(runner)},
          initialization_mode_{initialization::Mode::random},
          rng_{std::move(rng)}
    {
    }

    LocalSearch(
        RunnerType runner,
        const initialization::Mode initialization_mode,
        RNG rng)
        : runner_{std::move(runner)},
          initialization_mode_{initialization_mode},
          rng_{std::move(rng)}
    {
        validate_initialization_mode(initialization_mode_);
    }

    template<class Initialization, class Seed>
        requires std::constructible_from<RNG, Seed> &&
                 ((std::same_as<std::remove_cvref_t<Initialization>, initialization::Initial> &&
                   supports_initial) ||
                  (std::same_as<std::remove_cvref_t<Initialization>, initialization::Random> &&
                   supports_random) ||
                  std::same_as<std::remove_cvref_t<Initialization>, initialization::Mode>)
    LocalSearch(
        RunnerType runner,
        Initialization initialization,
        Seed seed)
        : LocalSearch(
              std::move(runner),
              initialization,
              RNG{std::move(seed)})
    {
    }

    template<class Initialization>
        requires std::constructible_from<RNG, std::uint64_t>
    LocalSearch(
        RunnerType runner,
        LocalSearchConfig<Initialization> config)
        : LocalSearch(
              std::move(runner),
              config.initialization,
              RNG{config.seed})
    {
    }

    [[nodiscard]]
    auto initialization_mode() const noexcept -> initialization::Mode
    {
        return initialization_mode_;
    }

    void initialization_mode(const initialization::Mode mode)
    {
        validate_initialization_mode(mode);
        initialization_mode_ = mode;
    }

    [[nodiscard]]
    auto rng() noexcept -> RNG&
    {
        return rng_;
    }

    [[nodiscard]]
    auto rng() const noexcept -> const RNG&
    {
        return rng_;
    }

    [[nodiscard]]
    auto solve(const input_type& input)
        requires easylocal::detail::solver_runnable<bound_runner_type, RNG> &&
                 (supports_initial || supports_random)
    {
        auto bound_runner = runner_.bind(input);
        auto solution = make_initial_solution(bound_runner);
        return easylocal::detail::run_with_solver_rng(
            bound_runner,
            std::move(solution),
            rng_);
    }

private:
    static void validate_initialization_mode(const initialization::Mode mode)
    {
        if (!supports(mode))
        {
            throw std::invalid_argument{
                mode == initialization::Mode::initial
                    ? "initial solution initialization is not supported by this Solver"
                    : "random solution initialization is not supported by this Solver"};
        }
    }

    [[nodiscard]]
    auto make_initial_solution(const bound_runner_type& bound_runner)
        -> solution_type
    {
        switch (initialization_mode_)
        {
        case initialization::Mode::initial:
            if constexpr (supports_initial)
            {
                return bound_runner.initial_solution();
            }
            break;
        case initialization::Mode::random:
            if constexpr (supports_random)
            {
                return bound_runner.random_solution(rng_);
            }
            break;
        }

        // Constructors and the setter validate runtime-selected modes. This is
        // an invariant guard in case a future initialization mode is added.
        throw std::logic_error{"unsupported Solver initialization mode"};
    }

    RunnerType runner_;
    initialization::Mode initialization_mode_;
    RNG rng_;
};

template<class RunnerType, class Initialization, class RNG>
LocalSearch(RunnerType, Initialization, RNG)
    -> LocalSearch<RunnerType, RNG>;

template<class RunnerType, class Initialization>
LocalSearch(RunnerType, LocalSearchConfig<Initialization>)
    -> LocalSearch<RunnerType>;

} // namespace easylocal::solvers
