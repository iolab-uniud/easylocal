#pragma once

#include <easylocal/runner.hpp>

#include <concepts>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace easylocal
{

namespace initialization
{

// Static tags are useful when initialization is fixed by the program: an
// unsupported choice is then rejected at compile time.
struct Initial
{
};

struct Random
{
};

inline constexpr Initial initial{};
inline constexpr Random random{};

// Mode is the runtime-facing counterpart, suitable for CLI/configuration.
// Unsupported runtime selections are rejected explicitly; there is never an
// implicit fallback from one initialization mode to another.
enum class Mode
{
    initial,
    random,
};

} // namespace initialization

namespace detail
{

template<class BoundRunner>
concept bound_runner_with_initial_solution =
    requires(const BoundRunner& bound_runner) {
        { bound_runner.initial_solution() } ->
            std::same_as<typename BoundRunner::solution_type>;
    };

template<class BoundRunner, class RNG>
concept bound_runner_with_random_solution =
    requires(const BoundRunner& bound_runner, RNG& rng) {
        { bound_runner.random_solution(rng) } ->
            std::same_as<typename BoundRunner::solution_type>;
    };

template<class BoundRunner, class RNG>
concept runner_with_rng =
    requires(
        BoundRunner& bound_runner,
        typename BoundRunner::solution_type solution,
        RNG& rng)
    {
        bound_runner.run(std::move(solution), rng);
    };

template<class BoundRunner>
concept runner_without_rng =
    requires(
        BoundRunner& bound_runner,
        typename BoundRunner::solution_type solution)
    {
        bound_runner.run(std::move(solution));
    };

template<class BoundRunner, class RNG>
concept solver_runnable =
    runner_with_rng<BoundRunner, RNG> || runner_without_rng<BoundRunner>;

template<class BoundRunner, class RNG>
[[nodiscard]]
auto run_with_solver_rng(
    BoundRunner& bound_runner,
    typename BoundRunner::solution_type solution,
    RNG& rng)
    requires solver_runnable<BoundRunner, RNG>
{
    if constexpr (runner_with_rng<BoundRunner, RNG>)
    {
        return bound_runner.run(std::move(solution), rng);
    }
    else
    {
        return bound_runner.run(std::move(solution));
    }
}

} // namespace detail

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
class LocalSearchSolver
{
public:
    using runner_type = RunnerType;
    using rng_type = RNG;
    using instance_type = typename runner_type::instance_type;
    using bound_runner_type = decltype(
        std::declval<runner_type&>().bind(
            std::declval<const instance_type&>()));
    using solution_type = typename bound_runner_type::solution_type;

    static constexpr bool supports_initial =
        detail::bound_runner_with_initial_solution<bound_runner_type>;
    static constexpr bool supports_random =
        detail::bound_runner_with_random_solution<bound_runner_type, rng_type>;

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

    LocalSearchSolver(
        RunnerType runner,
        const initialization::Initial,
        RNG rng)
        requires supports_initial
        : runner_{std::move(runner)},
          initialization_mode_{initialization::Mode::initial},
          rng_{std::move(rng)}
    {
    }

    LocalSearchSolver(
        RunnerType runner,
        const initialization::Random,
        RNG rng)
        requires supports_random
        : runner_{std::move(runner)},
          initialization_mode_{initialization::Mode::random},
          rng_{std::move(rng)}
    {
    }

    LocalSearchSolver(
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
    LocalSearchSolver(
        RunnerType runner,
        Initialization initialization,
        Seed seed)
        : LocalSearchSolver(
              std::move(runner),
              initialization,
              RNG{std::move(seed)})
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
    auto solve(const instance_type& instance)
        requires detail::solver_runnable<bound_runner_type, RNG> &&
                 (supports_initial || supports_random)
    {
        auto bound_runner = runner_.bind(instance);
        auto solution = make_initial_solution(bound_runner);
        return detail::run_with_solver_rng(
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
LocalSearchSolver(RunnerType, Initialization, RNG)
    -> LocalSearchSolver<RunnerType, RNG>;

template<class RunnerType, class Initialization>
[[nodiscard]]
auto make_local_search_solver(
    RunnerType runner,
    Initialization initialization,
    const std::uint64_t seed)
{
    return LocalSearchSolver<RunnerType>{
        std::move(runner),
        initialization,
        std::mt19937_64{seed}};
}

} // namespace easylocal
