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
struct TwoStageConfig
{
    Initialization initialization{initialization::random};
    std::uint64_t seed{0};
};

template<class FirstResult>
concept stage_result_with_solution =
    requires(FirstResult& result) {
        result.solution;
    };

// Two-stage optimization for hierarchical costs. The first runner is
// automatically projected onto the hard branch; the second runner sees the
// complete hierarchical cost. This keeps hard-only construction as framework
// machinery while leaving the two search algorithms independently configurable.
template<
    class FirstRunnerType,
    class SecondRunnerType,
    std::uniform_random_bit_generator RNG = std::mt19937_64>
class TwoStage
{
public:
    using first_runner_type = FirstRunnerType;
    using second_runner_type = SecondRunnerType;
    using rng_type = RNG;

    static_assert(
        easylocal::detail::hierarchical_solution_manager<
            typename first_runner_type::solution_manager_type>,
        "TwoStage requires a hierarchical cost on its first runner");
    static_assert(
        easylocal::detail::hierarchical_solution_manager<
            typename second_runner_type::solution_manager_type>,
        "TwoStage requires a hierarchical cost on its second runner");

    using hard_runner_type = decltype(
        std::declval<first_runner_type>().with_hard_cost());
    using input_type = typename hard_runner_type::input_type;
    using second_input_type = typename second_runner_type::input_type;
    static_assert(std::same_as<input_type, second_input_type>);

    using bound_first_runner_type = decltype(
        std::declval<hard_runner_type&>().bind(
            std::declval<const input_type&>()));
    using bound_second_runner_type = decltype(
        std::declval<second_runner_type&>().bind(
            std::declval<const input_type&>()));
    using solution_type = typename bound_first_runner_type::solution_type;
    using second_solution_type = typename bound_second_runner_type::solution_type;
    static_assert(std::same_as<solution_type, second_solution_type>);

    static constexpr bool supports_initial =
        easylocal::detail::bound_runner_with_initial_solution<bound_first_runner_type>;
    static constexpr bool supports_random =
        easylocal::detail::bound_runner_with_random_solution<bound_first_runner_type, rng_type>;

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

    template<class Initialization>
    TwoStage(
        FirstRunnerType first_runner,
        SecondRunnerType second_runner,
        Initialization initialization,
        RNG rng)
        requires (
            (std::same_as<std::remove_cvref_t<Initialization>, initialization::Initial> &&
             supports_initial) ||
            (std::same_as<std::remove_cvref_t<Initialization>, initialization::Random> &&
             supports_random) ||
            std::same_as<std::remove_cvref_t<Initialization>, initialization::Mode>)
        : hard_runner_{std::move(first_runner).with_hard_cost()},
          second_runner_{std::move(second_runner)},
          initialization_mode_{initialization_to_mode(initialization)},
          rng_{std::move(rng)}
    {
        validate_initialization_mode(initialization_mode_);
    }

    template<class Initialization>
        requires std::constructible_from<RNG, std::uint64_t>
    TwoStage(
        FirstRunnerType first_runner,
        SecondRunnerType second_runner,
        TwoStageConfig<Initialization> config)
        : TwoStage(
              std::move(first_runner),
              std::move(second_runner),
              config.initialization,
              RNG{config.seed})
    {
    }

    // The same runner configuration for both stages; the first stage is
    // projected onto the hard cost branch.
    template<class Initialization>
        requires std::same_as<FirstRunnerType, SecondRunnerType> &&
                 std::copy_constructible<FirstRunnerType> &&
                 std::constructible_from<RNG, std::uint64_t>
    TwoStage(
        const FirstRunnerType& runner,
        TwoStageConfig<Initialization> config)
        : TwoStage(runner, runner, config)
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
    auto rng() noexcept -> RNG& { return rng_; }

    [[nodiscard]]
    auto rng() const noexcept -> const RNG& { return rng_; }

    [[nodiscard]]
    auto solve(const input_type& input)
        requires easylocal::detail::solver_runnable<bound_first_runner_type, RNG> &&
                 easylocal::detail::solver_runnable<bound_second_runner_type, RNG> &&
                 (supports_initial || supports_random) &&
                 requires(bound_first_runner_type& bound_first_runner, RNG& rng) {
                     requires stage_result_with_solution<
                         decltype(easylocal::detail::run_with_solver_rng(
                             bound_first_runner,
                             std::declval<solution_type>(),
                             rng))>;
                 }
    {
        auto bound_first_runner = hard_runner_.bind(input);
        auto bound_second_runner = second_runner_.bind(input);

        auto first_result = easylocal::detail::run_with_solver_rng(
            bound_first_runner,
            make_initial_solution(bound_first_runner),
            rng_);

        return easylocal::detail::run_with_solver_rng(
            bound_second_runner,
            std::move(first_result.solution),
            rng_);
    }

private:
    static constexpr auto initialization_to_mode(const initialization::Initial)
        -> initialization::Mode
    {
        return initialization::Mode::initial;
    }

    static constexpr auto initialization_to_mode(const initialization::Random)
        -> initialization::Mode
    {
        return initialization::Mode::random;
    }

    static constexpr auto initialization_to_mode(const initialization::Mode mode)
        -> initialization::Mode
    {
        return mode;
    }

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
    auto make_initial_solution(const bound_first_runner_type& bound_first_runner)
        -> solution_type
    {
        switch (initialization_mode_)
        {
        case initialization::Mode::initial:
            if constexpr (supports_initial)
                return bound_first_runner.initial_solution();
            break;
        case initialization::Mode::random:
            if constexpr (supports_random)
                return bound_first_runner.random_solution(rng_);
            break;
        }
        throw std::logic_error{"unsupported Solver initialization mode"};
    }

    hard_runner_type hard_runner_;
    SecondRunnerType second_runner_;
    initialization::Mode initialization_mode_;
    RNG rng_;
};

template<class FirstRunnerType, class SecondRunnerType, class Initialization, class RNG>
TwoStage(FirstRunnerType, SecondRunnerType, Initialization, RNG)
    -> TwoStage<FirstRunnerType, SecondRunnerType, RNG>;

template<class FirstRunnerType, class SecondRunnerType, class Initialization>
TwoStage(FirstRunnerType, SecondRunnerType, TwoStageConfig<Initialization>)
    -> TwoStage<FirstRunnerType, SecondRunnerType>;

template<class RunnerType, class Initialization>
TwoStage(RunnerType, TwoStageConfig<Initialization>)
    -> TwoStage<RunnerType, RunnerType>;

} // namespace easylocal::solvers
