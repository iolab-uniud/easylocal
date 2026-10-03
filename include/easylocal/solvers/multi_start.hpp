#pragma once

#include <easylocal/solvers/initialization.hpp>
#include <easylocal/solvers/solver.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace easylocal::solvers
{

struct MultiStartParameters
{
    std::size_t starts{1};
};

template<class Initialization = initialization::Random>
struct MultiStartConfig
{
    MultiStartParameters parameters{};
    Initialization initialization{initialization::random};
    std::uint64_t seed{0};
};

// Repeatedly initialize and run the same bound Runner, retaining the best
// result according to the bound runner's cost semantics. `starts`
// denotes the total number of runs (not the number of runs after a first one).
template<
    class RunnerType,
    std::uniform_random_bit_generator RNG = std::mt19937_64>
class MultiStart
{
public:
    using runner_type = RunnerType;
    using rng_type = RNG;
    using input_type = typename runner_type::input_type;
    using bound_runner_type = decltype(
        std::declval<runner_type&>().bind(
            std::declval<const input_type&>()));
    using solution_type = typename bound_runner_type::solution_type;
    using cost_type = typename bound_runner_type::cost_type;

    static constexpr bool supports_initial =
        easylocal::detail::bound_runner_with_initial_solution<bound_runner_type>;
    static constexpr bool supports_random =
        easylocal::detail::bound_runner_with_random_solution<bound_runner_type, rng_type>;

    [[nodiscard]]
    static constexpr bool supports(const initialization::Mode mode) noexcept
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

    MultiStart(
        RunnerType runner,
        MultiStartParameters parameters,
        const initialization::Initial,
        RNG rng)
        requires supports_initial
        : runner_{std::move(runner)},
          parameters_{parameters},
          initialization_mode_{initialization::Mode::initial},
          rng_{std::move(rng)}
    {
        validate_parameters();
    }

    MultiStart(
        RunnerType runner,
        MultiStartParameters parameters,
        const initialization::Random,
        RNG rng)
        requires supports_random
        : runner_{std::move(runner)},
          parameters_{parameters},
          initialization_mode_{initialization::Mode::random},
          rng_{std::move(rng)}
    {
        validate_parameters();
    }

    MultiStart(
        RunnerType runner,
        MultiStartParameters parameters,
        const initialization::Mode initialization_mode,
        RNG rng)
        : runner_{std::move(runner)},
          parameters_{parameters},
          initialization_mode_{initialization_mode},
          rng_{std::move(rng)}
    {
        validate_parameters();
        validate_initialization_mode(initialization_mode_);
    }

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

    [[nodiscard]]
    initialization::Mode initialization_mode() const noexcept
    {
        return initialization_mode_;
    }

    void initialization_mode(const initialization::Mode mode)
    {
        validate_initialization_mode(mode);
        initialization_mode_ = mode;
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

    // Runs up to `starts` times from fresh solutions and returns the best
    // result, with the effort of every start. The optional trailing run
    // options go to every run; cancellation, or a run that reaches the target,
    // ends the solve (termination cancelled / target_reached, else completed).
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

        auto best = run_once(bound_runner, options...);
        auto termination = ended_by(best);
        easylocal::detail::search_effort effort;
        effort.add(best);
        for (std::size_t start = 1;
             start < parameters_.starts && !termination.has_value();
             ++start)
        {
            auto candidate = run_once(bound_runner, options...);
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

    void validate_parameters() const
    {
        if (parameters_.starts == 0)
        {
            throw std::invalid_argument{"MultiStart requires at least one start"};
        }
    }

    [[nodiscard]]
    solution_type make_initial_solution(const bound_runner_type& bound_runner)
    {
        switch (initialization_mode_)
        {
        case initialization::Mode::initial:
            if constexpr (supports_initial)
                return bound_runner.initial_solution();
            break;
        case initialization::Mode::random:
            if constexpr (supports_random)
                return bound_runner.random_solution(rng_);
            break;
        }
        throw std::logic_error{"unsupported Solver initialization mode"};
    }

    template<class... Options>
    [[nodiscard]]
    auto run_once(bound_runner_type& bound_runner, const Options&... options)
    {
        return easylocal::detail::run_with_solver_rng(
            bound_runner,
            make_initial_solution(bound_runner),
            rng_,
            options...);
    }

    // Why a start ends the whole solve, if it does.
    template<class Result>
    [[nodiscard]]
    static std::optional<termination_reason> ended_by(const Result& result)
    {
        const auto termination = easylocal::detail::termination_of(result);
        if (termination == termination_reason::cancelled ||
            termination == termination_reason::target_reached)
        {
            return termination;
        }
        return std::nullopt;
    }

    RunnerType runner_;
    MultiStartParameters parameters_;
    initialization::Mode initialization_mode_;
    RNG rng_;
};

template<class RunnerType, class Initialization, class RNG>
MultiStart(RunnerType, MultiStartParameters, Initialization, RNG)
    -> MultiStart<RunnerType, RNG>;

template<class RunnerType, class Initialization>
MultiStart(RunnerType, MultiStartConfig<Initialization>)
    -> MultiStart<RunnerType>;

} // namespace easylocal::solvers
