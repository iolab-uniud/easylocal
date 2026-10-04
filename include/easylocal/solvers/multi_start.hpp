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
                "Independent runs; the best result is kept"));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return starts == 0
            ? config::validation_result::failure("MultiStart requires at least one start")
            : config::validation_result::success();
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

    /// Runs up to `starts` times from fresh solutions and returns the best
    /// result, with the effort of every start.
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
        // The solve's time limit bounds all the starts together.
        const auto deadline = easylocal::detail::solve_deadline(options...);

        auto best = run_once(bound_runner, deadline, options...);
        auto termination = ended_by(best);
        easylocal::detail::search_effort effort;
        effort.add(best);
        for (std::size_t start = 1;
             start < parameters_.starts && !termination.has_value();
             ++start)
        {
            if (easylocal::detail::time_is_up(deadline))
            {
                termination = termination_reason::time_limit_reached;
                break;
            }
            auto candidate = run_once(bound_runner, deadline, options...);
            termination = ended_by(candidate);
            effort.add(candidate);
            if (bound_runner.better(candidate.cost, best.cost))
            {
                best = std::move(candidate);
            }
        }

        effort.assign_to(best);
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

    // One start, with the time left until the solve's deadline.
    template<class... Options>
    [[nodiscard]]
    auto run_once(
        bound_runner_type& bound_runner,
        const std::optional<std::chrono::steady_clock::time_point> deadline,
        const Options&... options)
    {
        return easylocal::detail::run_with_solver_rng(
            bound_runner,
            this->make_initial_solution(bound_runner, rng_),
            rng_,
            easylocal::detail::within_deadline(deadline, options...));
    }

    // Why a start ends the whole solve, if it does.
    template<class Result>
    [[nodiscard]]
    static std::optional<termination_reason> ended_by(const Result& result)
    {
        const auto termination = easylocal::detail::termination_of(result);
        if (termination == termination_reason::cancelled
            || termination == termination_reason::target_reached
            || termination == termination_reason::time_limit_reached)
        {
            return termination;
        }
        return std::nullopt;
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
