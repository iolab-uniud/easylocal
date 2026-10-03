#pragma once

// solvers::TwoStage: for hierarchical costs, a first runner on the hard cost
// until it reaches zero, then a second one on the whole cost.

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
struct TwoStageConfig
{
    Initialization initialization{initialization::random};
    std::uint64_t seed{0};
};

// Two-stage optimization for hierarchical costs. The first runner is
// automatically projected onto the hard branch and stops as soon as the hard
// cost reaches zero; the second runner continues from its solution on the
// complete hierarchical cost. This keeps hard-only construction as framework
// machinery while leaving the two search algorithms independently configurable.
namespace detail
{

// The first stage's bound runner: the first runner projected onto the hard
// cost. Without a hierarchical cost the first runner itself, so that
// TwoStage's static_assert reports the problem.
template<
    class FirstRunner,
    bool = easylocal::detail::hierarchical_solution_manager<
        typename FirstRunner::solution_manager_type>>
struct two_stage_first_bound
{
    using type = easylocal::detail::bound_runner_t<
        decltype(std::declval<FirstRunner>().with_hard_cost())>;
};

template<class FirstRunner>
struct two_stage_first_bound<FirstRunner, false>
{
    using type = easylocal::detail::bound_runner_t<FirstRunner>;
};

} // namespace detail

template<
    class FirstRunnerType,
    class SecondRunnerType,
    std::uniform_random_bit_generator RNG = std::mt19937_64>
class TwoStage
    : public easylocal::detail::InitializationSupport<
          typename detail::two_stage_first_bound<FirstRunnerType>::type,
          RNG>
{
    using initialization_support = easylocal::detail::InitializationSupport<
        typename detail::two_stage_first_bound<FirstRunnerType>::type,
        RNG>;

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

    using bound_first_runner_type = easylocal::detail::bound_runner_t<hard_runner_type>;
    using bound_second_runner_type =
        easylocal::detail::bound_runner_t<second_runner_type>;
    using solution_type = typename bound_first_runner_type::solution_type;
    using second_solution_type = typename bound_second_runner_type::solution_type;
    static_assert(std::same_as<solution_type, second_solution_type>);

    using initialization_support::supports_initial;
    using initialization_support::supports_random;

    // initialization: initialization::initial or random, rejected at compile
    // time when the first runner does not support it, or a Mode, checked here.
    template<class Initialization>
        requires easylocal::detail::
                     accepted_initialization<Initialization, bound_first_runner_type, RNG>
    TwoStage(
        FirstRunnerType first_runner,
        SecondRunnerType second_runner,
        Initialization initialization,
        RNG rng)
        : initialization_support{initialization},
          hard_runner_{std::move(first_runner).with_hard_cost()},
          second_runner_{std::move(second_runner)},
          rng_{std::move(rng)}
    {
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
    RNG& rng() noexcept
    {
        return rng_;
    }

    [[nodiscard]]
    const RNG& rng() const noexcept
    {
        return rng_;
    }

    using hard_cost_type = typename bound_first_runner_type::cost_type;

    // Stage 1 runs on the hard cost until it reaches cost::zero<hard_cost_type>();
    // stage 2 continues from its solution on the full cost. The optional
    // trailing run options (easylocal::with(control, tracer), .stop_at(target))
    // go to both stages, except the target, which applies to stage 2. After a
    // cancellation in stage 1, stage 2 only evaluates the solution and stops.
    // The result is stage 2's, with the effort of both stages.
    template<class... Options>
        requires easylocal::detail::solve_options<Options...>
    [[nodiscard]]
    auto solve(const input_type& input, const Options&... options)
        requires easylocal::cost::has_zero<hard_cost_type> &&
                 easylocal::detail::solver_runnable<
                     bound_first_runner_type,
                     RNG,
                     decltype(easylocal::detail::with_target(
                         std::declval<hard_cost_type>(),
                         std::declval<const Options&>()...))> &&
                 easylocal::detail::solver_runnable<bound_second_runner_type, RNG, Options...> &&
                 (supports_initial || supports_random) &&
                 requires(bound_first_runner_type& bound_first_runner, RNG& rng) {
                     requires easylocal::search_result_for<
                         decltype(easylocal::detail::run_with_solver_rng(
                             bound_first_runner,
                             std::declval<solution_type>(),
                             rng,
                             easylocal::detail::with_target(
                                 std::declval<hard_cost_type>(),
                                 std::declval<const Options&>()...))),
                         solution_type,
                         hard_cost_type>;
                 }
    {
        auto bound_first_runner = hard_runner_.bind(input);
        auto bound_second_runner = second_runner_.bind(input);

        auto first_result = easylocal::detail::run_with_solver_rng(
            bound_first_runner,
            this->make_initial_solution(bound_first_runner, rng_),
            rng_,
            easylocal::detail::with_target(
                easylocal::cost::zero<hard_cost_type>(),
                options...));

        easylocal::detail::search_effort effort;
        effort.add(first_result);

        auto result = easylocal::detail::run_with_solver_rng(
            bound_second_runner,
            std::move(first_result.solution),
            rng_,
            options...);

        effort.add(result);
        effort.assign_to(result);
        return result;
    }

    // The parameters of the two runners, under "first" and "second".
    [[nodiscard]]
    config::parameter_set configuration()
    {
        config::parameter_set parameters;
        config::add_configuration(parameters, "first", hard_runner_);
        config::add_configuration(parameters, "second", second_runner_);
        return parameters;
    }

private:
    hard_runner_type hard_runner_;
    SecondRunnerType second_runner_;
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
