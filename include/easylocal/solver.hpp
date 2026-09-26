#pragma once

#include <easylocal/runner.hpp>

#include <concepts>
#include <cstddef>
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


struct MultiStartParameters
{
    std::size_t starts{1};
};

namespace detail
{
template<class Result, class Cost>
concept multi_start_result =
    requires(const Result& result) {
        { result.cost } -> std::convertible_to<const Cost&>;
    };
}

// Repeatedly initialize and run the same bound Runner, retaining the best
// result according to the bound SolutionManager's cost semantics. `starts`
// denotes the total number of runs (not the number of runs after a first one).
template<
    class RunnerType,
    std::uniform_random_bit_generator RNG = std::mt19937_64>
class MultiStartSolver
{
public:
    using runner_type = RunnerType;
    using rng_type = RNG;
    using instance_type = typename runner_type::instance_type;
    using bound_runner_type = decltype(
        std::declval<runner_type&>().bind(
            std::declval<const instance_type&>()));
    using solution_type = typename bound_runner_type::solution_type;
    using cost_type = typename bound_runner_type::cost_type;

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

    MultiStartSolver(
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

    MultiStartSolver(
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

    MultiStartSolver(
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
    auto solve(const instance_type& instance)
        requires detail::solver_runnable<bound_runner_type, RNG> &&
                 (supports_initial || supports_random) &&
                 requires(bound_runner_type& bound_runner, RNG& rng) {
                     { bound_runner.better(
                         std::declval<const cost_type&>(),
                         std::declval<const cost_type&>()) } ->
                         std::convertible_to<bool>;
                     requires detail::multi_start_result<
                         decltype(detail::run_with_solver_rng(
                             bound_runner,
                             std::declval<solution_type>(),
                             rng)),
                         cost_type>;
                 }
    {
        auto bound_runner = runner_.bind(instance);

        auto best = run_once(bound_runner);
        for (std::size_t start = 1; start < parameters_.starts; ++start)
        {
            auto candidate = run_once(bound_runner);
            if (bound_runner.better(candidate.cost, best.cost))
            {
                best = std::move(candidate);
            }
        }
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
            throw std::invalid_argument{"MultiStartSolver requires at least one start"};
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
                return bound_runner.initial_solution();
            break;
        case initialization::Mode::random:
            if constexpr (supports_random)
                return bound_runner.random_solution(rng_);
            break;
        }
        throw std::logic_error{"unsupported Solver initialization mode"};
    }

    [[nodiscard]]
    auto run_once(bound_runner_type& bound_runner)
    {
        return detail::run_with_solver_rng(
            bound_runner,
            make_initial_solution(bound_runner),
            rng_);
    }

    RunnerType runner_;
    MultiStartParameters parameters_;
    initialization::Mode initialization_mode_;
    RNG rng_;
};

template<class RunnerType, class Initialization, class RNG>
MultiStartSolver(RunnerType, MultiStartParameters, Initialization, RNG)
    -> MultiStartSolver<RunnerType, RNG>;

template<class RunnerType, class Initialization>
[[nodiscard]]
auto make_multi_start_solver(
    RunnerType runner,
    MultiStartParameters parameters,
    Initialization initialization,
    const std::uint64_t seed)
{
    return MultiStartSolver<RunnerType>{
        std::move(runner),
        parameters,
        initialization,
        std::mt19937_64{seed}};
}



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
class TwoStageSolver
{
public:
    using first_runner_type = FirstRunnerType;
    using second_runner_type = SecondRunnerType;
    using rng_type = RNG;

    static_assert(
        detail::hierarchical_solution_manager<
            typename first_runner_type::solution_manager_type>,
        "TwoStageSolver requires a hierarchical cost on its first runner");
    static_assert(
        detail::hierarchical_solution_manager<
            typename second_runner_type::solution_manager_type>,
        "TwoStageSolver requires a hierarchical cost on its second runner");

    using hard_runner_type = decltype(
        std::declval<first_runner_type>().with_hard_cost());
    using instance_type = typename hard_runner_type::instance_type;
    using second_instance_type = typename second_runner_type::instance_type;
    static_assert(std::same_as<instance_type, second_instance_type>);

    using bound_first_runner_type = decltype(
        std::declval<hard_runner_type&>().bind(
            std::declval<const instance_type&>()));
    using bound_second_runner_type = decltype(
        std::declval<second_runner_type&>().bind(
            std::declval<const instance_type&>()));
    using solution_type = typename bound_first_runner_type::solution_type;
    using second_solution_type = typename bound_second_runner_type::solution_type;
    static_assert(std::same_as<solution_type, second_solution_type>);

    static constexpr bool supports_initial =
        detail::bound_runner_with_initial_solution<bound_first_runner_type>;
    static constexpr bool supports_random =
        detail::bound_runner_with_random_solution<bound_first_runner_type, rng_type>;

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
    TwoStageSolver(
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
    auto solve(const instance_type& instance)
        requires detail::solver_runnable<bound_first_runner_type, RNG> &&
                 detail::solver_runnable<bound_second_runner_type, RNG> &&
                 (supports_initial || supports_random) &&
                 requires(bound_first_runner_type& bound_first_runner, RNG& rng) {
                     requires stage_result_with_solution<
                         decltype(detail::run_with_solver_rng(
                             bound_first_runner,
                             std::declval<solution_type>(),
                             rng))>;
                 }
    {
        auto bound_first_runner = hard_runner_.bind(instance);
        auto bound_second_runner = second_runner_.bind(instance);

        auto first_result = detail::run_with_solver_rng(
            bound_first_runner,
            make_initial_solution(bound_first_runner),
            rng_);

        return detail::run_with_solver_rng(
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
TwoStageSolver(FirstRunnerType, SecondRunnerType, Initialization, RNG)
    -> TwoStageSolver<FirstRunnerType, SecondRunnerType, RNG>;

} // namespace easylocal

namespace easylocal::solver
{

template<class Initialization = initialization::Random>
struct LocalSearchConfig
{
    Initialization initialization{initialization::random};
    std::uint64_t seed{0};
};

template<class Initialization = initialization::Random>
struct MultiStartConfig
{
    MultiStartParameters parameters{};
    Initialization initialization{initialization::random};
    std::uint64_t seed{0};
};

template<class Initialization = initialization::Random>
struct TwoStageConfig
{
    Initialization initialization{initialization::random};
    std::uint64_t seed{0};
};

struct local_search
{
    template<class RunnerType, class Initialization>
    [[nodiscard]]
    static auto make(
        RunnerType runner,
        LocalSearchConfig<Initialization> config)
    {
        return LocalSearchSolver<RunnerType>{
            std::move(runner),
            config.initialization,
            std::mt19937_64{config.seed}};
    }
};

struct multistart
{
    template<class RunnerType, class Initialization>
    [[nodiscard]]
    static auto make(
        RunnerType runner,
        MultiStartConfig<Initialization> config)
    {
        return MultiStartSolver<RunnerType>{
            std::move(runner),
            config.parameters,
            config.initialization,
            std::mt19937_64{config.seed}};
    }
};

struct two_stage
{
    template<class FirstRunnerType, class SecondRunnerType, class Initialization>
    [[nodiscard]]
    static auto make(
        FirstRunnerType first_runner,
        SecondRunnerType second_runner,
        TwoStageConfig<Initialization> config)
    {
        return TwoStageSolver<FirstRunnerType, SecondRunnerType>{
            std::move(first_runner),
            std::move(second_runner),
            config.initialization,
            std::mt19937_64{config.seed}};
    }
};

} // namespace easylocal::solver

namespace easylocal
{

template<class Tag, class... Args>
concept solver_factory_tag = requires(Args&&... args) {
    Tag::make(std::forward<Args>(args)...);
};

template<class Tag, class... Args>
    requires solver_factory_tag<Tag, Args...>
[[nodiscard]]
auto make_solver(Args&&... args)
{
    return Tag::make(std::forward<Args>(args)...);
}

} // namespace easylocal
