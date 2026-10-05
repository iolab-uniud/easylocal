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
#include <optional>
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

/// The configuration of MultiStart: its parameters, the initialization and the
/// seed of the RNG.
template<class Initialization = initialization::Random>
struct MultiStartConfig
{
    /// The parameters.
    MultiStartParameters parameters{};
    /// How each initial solution is built: a tag or an initialization::Mode.
    Initialization initialization{initialization::random};
    /// The seed of the solver's RNG.
    std::uint64_t seed{0};
};

/// Repeatedly initialize and run the same bound Runner, retaining the best
/// result according to the bound runner's cost semantics.
///
/// `starts` denotes the total number of runs (not the number of runs after a
/// first one).
template<class RunnerType, std::uniform_random_bit_generator RNG = std::mt19937_64>
class MultiStart
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
    /// The cost of the runner.
    using cost_type = typename bound_runner_type::cost_type;

    /// Whether the runner can build an initial solution (`initial_solution()`).
    using initialization_support::supports_initial;
    /// Whether the runner can build a random solution (`random_solution(rng)`).
    using initialization_support::supports_random;

    /// initialization: initialization::initial or random, rejected at compile
    /// time when the runner does not support it, or a Mode, checked here before
    /// the parameters.
    template<class Initialization>
        requires easylocal::detail::
                     accepted_initialization<Initialization, bound_runner_type, RNG>
    MultiStart(
        RunnerType runner,
        MultiStartParameters parameters,
        Initialization initialization,
        RNG rng)
        : initialization_support{initialization},
          runner_{std::move(runner)},
          parameters_{parameters},
          rng_{std::move(rng)}
    {
        validate_parameters();
    }

    /// From a runner and a MultiStartConfig.
    ///
    /// Throws `std::invalid_argument` when the parameters are not valid.
    template<class Initialization>
        requires std::constructible_from<RNG, std::uint64_t>
    MultiStart(
        RunnerType runner,
        MultiStartConfig<Initialization> config)
        : MultiStart(
              std::move(runner),
              config.parameters,
              config.initialization,
              RNG{config.seed})
    {
    }

    /// The RNG, which feeds initialization and runs.
    template<class Self>
    [[nodiscard]]
    auto& rng(this Self&& self) noexcept
    {
        return self.rng_;
    }

    /// Runs up to `starts` times from fresh solutions and returns the best
    /// result, with the effort of every start and, with a cost::pareto cost,
    /// the front merged from every start.
    ///
    /// The optional trailing run options go to every run; cancellation, or a
    /// run that reaches the target, ends the solve (termination cancelled /
    /// target_reached, else completed).
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

        auto best = run_once(bound_runner, budget, options...);
        budget.consume(best);
        auto termination = easylocal::detail::ends_runs(best);
        easylocal::detail::search_effort effort;
        effort.add(best);
        easylocal::detail::merged_front<decltype(best)> front;
        front.add(bound_runner, best);
        for (std::size_t start = 1;
             start < parameters_.starts && !termination.has_value();
             ++start)
        {
            if (easylocal::detail::stop_requested(options...))
            {
                termination = termination_reason::cancelled;
                break;
            }
            if (budget.spent())
                break;
            auto candidate = run_once(bound_runner, budget, options...);
            budget.consume(candidate);
            termination = easylocal::detail::ends_runs(candidate);
            effort.add(candidate);
            front.add(bound_runner, candidate);
            if (bound_runner.better(candidate.cost, best.cost))
            {
                best = std::move(candidate);
            }
        }
        // The time or the evaluations ran out, before a start or during the
        // last one.
        if (!termination)
            termination = budget.spent();

        effort.assign_to(best);
        front.assign_to(best);
        easylocal::detail::set_termination(
            best,
            termination.value_or(termination_reason::completed));
        return best;
    }

    /// Its own parameters (starts) and its runner's (search.*, cost.*,
    /// neighborhood.*), with paths relative to the solver.
    [[nodiscard]]
    config::parameter_set configuration()
    {
        config::parameter_set parameters;
        parameters.add(parameters_);
        config::add_configuration(parameters, {}, runner_);
        return parameters;
    }

private:
    void validate_parameters() const
    {
        if (const auto validation = parameters_.validate(); !validation)
        {
            throw std::invalid_argument{std::string{validation.message}};
        }
    }

    // One start, with what is left of the solve's time and evaluations.
    template<class... Options>
    [[nodiscard]]
    auto run_once(
        bound_runner_type& bound_runner,
        const easylocal::detail::solve_budget& budget,
        const Options&... options)
    {
        return easylocal::detail::run_with_solver_rng(
            bound_runner,
            this->make_initial_solution(bound_runner, rng_),
            rng_,
            budget.options_for_run(options...));
    }

    RunnerType runner_;
    MultiStartParameters parameters_;
    RNG rng_;
};

/// `MultiStart{runner, parameters, initialization, rng}` deduces the runner and
/// RNG types.
template<class RunnerType, class Initialization, class RNG>
MultiStart(RunnerType, MultiStartParameters, Initialization, RNG)
    -> MultiStart<RunnerType, RNG>;

/// `MultiStart{runner, config}` deduces the runner type, with the default RNG.
template<class RunnerType, class Initialization>
MultiStart(RunnerType, MultiStartConfig<Initialization>)
    -> MultiStart<RunnerType>;

} // namespace easylocal::solvers
