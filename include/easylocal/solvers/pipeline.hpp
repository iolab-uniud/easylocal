#pragma once

/// \file
/// solvers::Pipeline: runners in sequence over the same Input and Solution,
/// each stage starting from the solution of the previous one.
///
///     using namespace easylocal::solvers;
///     auto feasible = stage("feasible", descent) & until_feasible();
///     auto solver = (feasible & attempts(10)) | stage("descent", descent)
///         | stage("anneal", annealing);
///     auto result = solver.seed(7).solve(input);
///
/// The same pipeline, spelled out:
///
///     auto feasible = stage("feasible", descent).until_feasible();
///     auto solver = pipeline(feasible.with_attempts(10))
///                       .then(stage("descent", descent))
///                       .then(stage("anneal", annealing));

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/config/parameters.hpp>
#include <easylocal/cost/concepts.hpp>
#include <easylocal/cost/text.hpp>
#include <easylocal/helpers/detail/cost_layer.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/solvers/initialization.hpp>
#include <easylocal/solvers/solver.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::solvers
{

/// The parameters of a pipeline stage.
struct StageParameters
{
    /// The runs of the stage, at least 1: it runs again while its target is not
    /// reached, and keeps the best run.
    std::size_t attempts{1};
    /// The seconds the stage may run, all its attempts together; inf: no
    /// limit of its own (the solve's still applies).
    double timeout{std::numeric_limits<double>::infinity()};
    /// The evaluations the stage may make, all its attempts together;
    /// unlimited: no budget of its own (the solve's still applies).
    limit max_evaluations{unlimited};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"attempts", &StageParameters::attempts>(
                "Runs of the stage while its target is not reached; the best is kept",
                config::range(1, easylocal::unlimited)),
            config::field<"timeout", &StageParameters::timeout>(
                "Seconds the stage may run, its attempts together; inf: no limit",
                config::range(0.0, easylocal::unlimited)),
            config::field<"max_evaluations", &StageParameters::max_evaluations>(
                "Evaluations the stage may make, its attempts together",
                config::range(0, easylocal::unlimited)));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

template<class Runner>
class pipeline_stage;

template<std::uniform_random_bit_generator RNG, class... Stages>
class Pipeline;

/// A stage of a pipeline: a named runner, with an optional target cost, a
/// number of attempts, and an optional time limit and evaluation budget.
///
/// A stage is built by stage(name, runner) and refined by with_target(),
/// with_attempts(), until_feasible(), with_timeout() and
/// with_max_evaluations(), each returning the refined stage, or with `&` and
/// the modifiers target(), attempts(), until_feasible(), timeout() and
/// max_evaluations().
template<class Runner>
class pipeline_stage
{
public:
    /// The runner, unbound.
    using runner_type = Runner;
    /// The runner bound to an Input.
    using bound_runner_type = easylocal::detail::bound_runner_t<Runner>;
    /// The Input of the runner.
    using input_type = typename Runner::input_type;
    /// The solution of the runner.
    using solution_type = typename bound_runner_type::solution_type;
    /// The cost of the runner, in which the stage's target is written.
    using cost_type = typename bound_runner_type::cost_type;

    /// The stage `name` running `runner`, once, with no target.
    pipeline_stage(std::string name, Runner runner)
        : name_{std::move(name)}, runner_{std::move(runner)}
    {
    }

    /// The same stage, stopping as soon as its best cost reaches `target`.
    [[nodiscard]]
    pipeline_stage with_target(cost_type target) &&
    {
        target_ = std::move(target);
        return std::move(*this);
    }

    /// The same stage, stopping as soon as its best cost reaches `target`.
    [[nodiscard]]
    pipeline_stage with_target(cost_type target) const&
    {
        return pipeline_stage{*this}.with_target(std::move(target));
    }

    /// The same stage, run up to `count` times while it does not reach its
    /// target, keeping the best run.
    ///
    /// Throws `std::invalid_argument` when `count` is 0.
    [[nodiscard]]
    pipeline_stage with_attempts(const std::size_t count) &&
    {
        parameters_.attempts = count;
        if (const auto valid = parameters_.validate(); !valid)
            throw std::invalid_argument{std::string{valid.message}};
        return std::move(*this);
    }

    /// The same stage, run up to `count` times while it does not reach its
    /// target, keeping the best run.
    ///
    /// Throws `std::invalid_argument` when `count` is 0.
    [[nodiscard]]
    pipeline_stage with_attempts(const std::size_t count) const&
    {
        return pipeline_stage{*this}.with_attempts(count);
    }

    /// The same stage, stopped once `limit` has passed since it started, all
    /// its attempts together.
    ///
    /// Throws `std::invalid_argument` when the limit is negative.
    template<class Rep, class Period>
    [[nodiscard]]
    pipeline_stage with_timeout(const std::chrono::duration<Rep, Period> limit) &&
    {
        const auto steady = easylocal::detail::steady_time_limit(limit);
        parameters_.timeout = steady == (std::chrono::steady_clock::duration::max)()
            ? std::numeric_limits<double>::infinity()
            : std::chrono::duration<double>{steady}.count();
        return std::move(*this);
    }

    /// The same stage, stopped once `limit` has passed since it started, all
    /// its attempts together.
    ///
    /// Throws `std::invalid_argument` when the limit is negative.
    template<class Rep, class Period>
    [[nodiscard]]
    pipeline_stage with_timeout(const std::chrono::duration<Rep, Period> limit) const&
    {
        return pipeline_stage{*this}.with_timeout(limit);
    }

    /// The same stage, stopped once `seconds` have passed since it started.
    ///
    /// Throws `std::invalid_argument` when the number is negative or not
    /// finite.
    [[nodiscard]]
    pipeline_stage with_timeout(const double seconds) &&
    {
        return std::move(*this).with_timeout(
            easylocal::detail::steady_time_limit(seconds));
    }

    /// The same stage, stopped once `seconds` have passed since it started.
    ///
    /// Throws `std::invalid_argument` when the number is negative or not
    /// finite.
    [[nodiscard]]
    pipeline_stage with_timeout(const double seconds) const&
    {
        return pipeline_stage{*this}.with_timeout(seconds);
    }

    /// The same stage, stopped once it has made `count` evaluations, all its
    /// attempts together.
    [[nodiscard]]
    pipeline_stage with_max_evaluations(const std::size_t count) &&
    {
        parameters_.max_evaluations = count;
        return std::move(*this);
    }

    /// The same stage, stopped once it has made `count` evaluations, all its
    /// attempts together.
    [[nodiscard]]
    pipeline_stage with_max_evaluations(const std::size_t count) const&
    {
        return pipeline_stage{*this}.with_max_evaluations(count);
    }

    /// The same stage, whose attempts after the first start from a new
    /// solution built as `initialization` says, rather than from the solution
    /// the stage received: initialization::initial, random or automatic,
    /// rejected at compile time when the runner does not support it.
    template<class Initialization>
        requires easylocal::detail::accepted_initialization<
            Initialization,
            bound_runner_type,
            std::mt19937_64>
    [[nodiscard]]
    pipeline_stage with_restart(const Initialization initialization) &&
    {
        restart_ = easylocal::detail::initialization_kind_of(initialization);
        return std::move(*this);
    }

    /// The same stage, whose attempts after the first start from a new
    /// solution built as `initialization` says, rather than from the solution
    /// the stage received: initialization::initial, random or automatic,
    /// rejected at compile time when the runner does not support it.
    template<class Initialization>
        requires easylocal::detail::accepted_initialization<
            Initialization,
            bound_runner_type,
            std::mt19937_64>
    [[nodiscard]]
    pipeline_stage with_restart(const Initialization initialization) const&
    {
        return pipeline_stage{*this}.with_restart(initialization);
    }

    /// The same stage on the hard cost only, until it is zero: a feasible
    /// solution.
    ///
    /// The runner becomes `runner.with_hard_cost()` and the target the zero of
    /// the hard cost; the name, the attempts, the time limit, the evaluation
    /// budget and the restart stay. Requires a runner with a hierarchical cost
    /// (`cost::hierarchical`).
    [[nodiscard]]
    auto until_feasible() &&
    {
        static_assert(
            easylocal::detail::hierarchical_solution_manager<
                typename Runner::solution_manager_type>,
            "until_feasible() requires a stage whose runner has a hierarchical "
            "cost");
        using hard_runner_type = decltype(std::move(runner_).with_hard_cost());
        using hard_cost_type = typename pipeline_stage<hard_runner_type>::cost_type;
        pipeline_stage<hard_runner_type> stage{
            std::move(name_),
            std::move(runner_).with_hard_cost()};
        stage.parameters_ = parameters_;
        stage.target_ = cost::zero<hard_cost_type>();
        stage.restart_ = restart_;
        return stage;
    }

    /// The same stage on the hard cost only, until it is zero: a feasible
    /// solution.
    ///
    /// The runner becomes `runner.with_hard_cost()` and the target the zero of
    /// the hard cost; the name, the attempts, the time limit, the evaluation
    /// budget and the restart stay. Requires a runner with a hierarchical cost
    /// (`cost::hierarchical`).
    [[nodiscard]]
    auto until_feasible() const&
    {
        return pipeline_stage{*this}.until_feasible();
    }

    /// The stage's name, which prefixes its parameters.
    [[nodiscard]]
    const std::string& name() const noexcept
    {
        return name_;
    }

    /// The runner.
    template<class Self>
    [[nodiscard]]
    auto& runner(this Self&& self) noexcept
    {
        return self.runner_;
    }

    /// The target cost, if any.
    [[nodiscard]]
    const std::optional<cost_type>& target() const noexcept
    {
        return target_;
    }

    /// Whether the attempts after the first start from a new solution
    /// (with_restart()).
    [[nodiscard]]
    bool restarts() const noexcept
    {
        return restart_.has_value();
    }

    /// The stage's own parameters (attempts, timeout, max_evaluations).
    template<class Self>
    [[nodiscard]]
    auto& parameters(this Self&& self) noexcept
    {
        return self.parameters_;
    }

private:
    template<class>
    friend class pipeline_stage;
    template<std::uniform_random_bit_generator, class...>
    friend class Pipeline;

    // The limits of the stage started now: its deadline and its evaluations,
    // none when it has no limit of its own.
    [[nodiscard]]
    easylocal::detail::solve_budget limits() const
    {
        easylocal::detail::solve_budget budget;
        if (std::isfinite(parameters_.timeout))
        {
            budget.deadline = easylocal::detail::deadline_after(
                easylocal::detail::steady_time_limit(parameters_.timeout));
        }
        if (!parameters_.max_evaluations.is_unlimited())
            budget.evaluations = parameters_.max_evaluations;
        return budget;
    }

    std::string name_;
    Runner runner_;
    std::optional<cost_type> target_;
    StageParameters parameters_{};
    std::optional<easylocal::detail::initialization_kind> restart_;
};

namespace detail
{

// A complete runner, which a stage binds to its Input.
template<class Runner>
concept stage_runner =
    requires(const Runner& runner, const typename Runner::input_type& input) {
        runner.bind(input);
    };

// An algorithm an app builds from its parameters: a parameters_type, default
// constructible, from which the algorithm is constructed.
template<class Algorithm>
concept stage_algorithm = requires { typename Algorithm::parameters_type; }
    && std::default_initializable<typename Algorithm::parameters_type>
    && std::constructible_from<Algorithm, typename Algorithm::parameters_type>;

} // namespace detail

/// The stage `name` of a pipeline, running `runner`.
template<class Runner>
    requires detail::stage_runner<Runner>
[[nodiscard]]
pipeline_stage<Runner> stage(std::string name, Runner runner)
{
    return {std::move(name), std::move(runner)};
}

/// A stage modifier: the stage stops as soon as its best cost reaches `cost`.
template<class Cost>
struct stage_target
{
    /// Marks a stage option an algorithm stage applies when it is built.
    using stage_option_tag = void;
    /// The target, converted to the stage's cost.
    Cost cost;
};

/// A stage modifier: up to `count` runs while the stage does not reach its
/// target.
struct stage_attempts
{
    /// The runs, at least 1.
    std::size_t count{1};
};

/// A stage modifier: the stage runs on the hard cost until it is zero.
struct stage_until_feasible
{
};

/// A stage modifier: the attempts after the first start from a new solution.
template<class Initialization>
struct stage_restart
{
    /// Marks a stage option an algorithm stage applies when it is built.
    using stage_option_tag = void;
    /// How the new solutions are built: initialization::initial, random or
    /// automatic.
    Initialization initialization;
};

/// The stage stops as soon as its best cost reaches `cost`, written in the
/// stage's own cost: `stage(...) & target(0)`, as `with_target(0)`.
template<class Cost>
[[nodiscard]]
stage_target<Cost> target(Cost cost)
{
    return {std::move(cost)};
}

/// The stage runs up to `count` times while it does not reach its target, and
/// keeps its best run: `stage(...) & attempts(10)`, as `with_attempts(10)`.
[[nodiscard]]
constexpr stage_attempts attempts(const std::size_t count) noexcept
{
    return {count};
}

/// The stage runs on the hard cost only, until it is zero:
/// `stage(...) & until_feasible()`, as `.until_feasible()`.
[[nodiscard]]
constexpr stage_until_feasible until_feasible() noexcept
{
    return {};
}

/// The attempts of the stage after the first start from a new solution built
/// as `initialization` says, rather than from the solution the stage received:
/// `stage(...) & attempts(10) & restart(initialization::random)`, as
/// `with_restart(initialization::random)`.
template<class Initialization>
    requires std::same_as<Initialization, initialization::Initial>
    || std::same_as<Initialization, initialization::Random>
    || std::same_as<Initialization, initialization::Automatic>
[[nodiscard]]
constexpr stage_restart<Initialization> restart(
    const Initialization initialization) noexcept
{
    return {initialization};
}

/// The stage with a target: `stage.with_target(target.cost)`.
///
/// Requires a target that converts to the stage's cost without losing its
/// fractional part: a floating-point target for an integer cost is rejected.
template<class Runner, class Cost>
    requires std::convertible_to<Cost, typename pipeline_stage<Runner>::cost_type>
[[nodiscard]]
pipeline_stage<Runner> operator&(pipeline_stage<Runner> stage, stage_target<Cost> target)
{
    using cost_type = typename pipeline_stage<Runner>::cost_type;
    static_assert(
        !(std::floating_point<Cost> && std::integral<cost_type>),
        "the target of a stage is written in the stage's cost: a floating-point "
        "target would be truncated to its integer cost");
    return std::move(stage).with_target(cost_type(std::move(target.cost)));
}

/// The stage with a number of attempts: `stage.with_attempts(attempts.count)`.
///
/// Throws `std::invalid_argument` when the count is 0.
template<class Runner>
[[nodiscard]]
pipeline_stage<Runner> operator&(
    pipeline_stage<Runner> stage,
    const stage_attempts attempts)
{
    return std::move(stage).with_attempts(attempts.count);
}

/// The stage on the hard cost until it is zero: `stage.until_feasible()`.
///
/// Requires a runner with a hierarchical cost (`cost::hierarchical`).
template<class Runner>
[[nodiscard]]
auto operator&(pipeline_stage<Runner> stage, stage_until_feasible)
{
    return std::move(stage).until_feasible();
}

/// The stage whose attempts after the first start from a new solution:
/// `stage.with_restart(restart.initialization)`.
///
/// Requires a runner whose SolutionManager builds that solution.
template<class Runner, class Initialization>
    requires easylocal::detail::accepted_initialization<
        Initialization,
        easylocal::detail::bound_runner_t<Runner>,
        std::mt19937_64>
[[nodiscard]]
pipeline_stage<Runner> operator&(
    pipeline_stage<Runner> stage,
    const stage_restart<Initialization> restart)
{
    return std::move(stage).with_restart(restart.initialization);
}

namespace detail
{

// Throws std::invalid_argument unless the run options given to a stage carry
// only limits: a control belongs to the solve, which gives it to every stage.
inline void require_stage_limits(const run_options<trace::null_tracer>& options)
{
    if (options.control != nullptr)
    {
        throw std::invalid_argument{
            "a pipeline stage takes a time limit and an evaluation budget, not a "
            "run control: give the control to solve()"};
    }
}

} // namespace detail

/// The stage with a time limit or an evaluation budget: `stage & timeout(10s)`,
/// as `stage.with_timeout(10s)`, and `stage & max_evaluations(5000)`, as
/// `stage.with_max_evaluations(5000)`.
///
/// Throws `std::invalid_argument` when the options carry a run control, which
/// the solve gives to every stage: `stage & with(control)` would drop it.
template<class Runner>
[[nodiscard]]
pipeline_stage<Runner> operator&(
    pipeline_stage<Runner> stage,
    const run_options<trace::null_tracer>& options)
{
    detail::require_stage_limits(options);
    if (options.time_limit)
        stage = std::move(stage).with_timeout(*options.time_limit);
    if (!options.evaluation_limit.is_unlimited())
        stage = std::move(stage).with_max_evaluations(options.evaluation_limit);
    return stage;
}

/// The evaluation budget of a run, `max_evaluations(n)`, here for a stage:
/// `stage & max_evaluations(5000)`.
using easylocal::max_evaluations;
/// The time limit of a run, `timeout(10s)` or `timeout(2.5)` seconds, here for
/// a stage: `stage & timeout(10s)`.
using easylocal::timeout;

/// A stage of a pipeline registered in an app, which runs Algorithm on the
/// app's recipes: its SolutionManager, with the app's cost, and the app's
/// neighborhood or its own.
///
/// stage<Algorithm>(name, parameters[, neighborhood]) builds it, and `&` the
/// stage options, as for a stage of a runner. Registered in an app with
/// easylocal::pipeline, it becomes a stage of the runner of Algorithm on the
/// app's recipes when the pipeline runs, so the app's `cost.*` parameters
/// apply to it. Its parameters are those of the stage (`attempts`, `timeout`,
/// `max_evaluations`), the algorithm's (`search.*`) and those of its own
/// neighborhood (`neighborhood.*`). Requires an algorithm with a
/// default-constructible parameters_type, constructible from it.
template<
    class Algorithm,
    class NeighborhoodSpec = easylocal::detail::unconfigured_t,
    class... Modifiers>
    requires detail::stage_algorithm<Algorithm>
class algorithm_stage
{
public:
    /// The algorithm the stage runs.
    using algorithm_type = Algorithm;
    /// The parameter block of the algorithm.
    using parameters_type = typename Algorithm::parameters_type;
    /// The recipe of its own neighborhood, or unconfigured when it runs on the
    /// app's.
    using neighborhood_spec_type = NeighborhoodSpec;

    /// Whether the stage brings its own neighborhood.
    static constexpr bool has_own_neighborhood =
        !std::same_as<NeighborhoodSpec, easylocal::detail::unconfigured_t>;

    /// The stage `name` running Algorithm with `parameters`, on `neighborhood`
    /// when it has its own, with the options `modifiers` applied in order when
    /// the stage is built, and its own parameters `stage_parameters`.
    algorithm_stage(
        std::string name,
        parameters_type parameters,
        NeighborhoodSpec neighborhood = {},
        std::tuple<Modifiers...> modifiers = {},
        StageParameters stage_parameters = {})
        : name_{std::move(name)},
          algorithm_parameters_{std::move(parameters)},
          neighborhood_{std::move(neighborhood)},
          modifiers_{std::move(modifiers)},
          parameters_{stage_parameters}
    {
    }

    /// The stage's name, which prefixes its parameters.
    [[nodiscard]]
    const std::string& name() const noexcept
    {
        return name_;
    }

    /// The stage's own parameters (attempts, timeout, max_evaluations).
    template<class Self>
    [[nodiscard]]
    auto& parameters(this Self&& self) noexcept
    {
        return self.parameters_;
    }

    /// The parameters of the algorithm, from which each run builds it.
    template<class Self>
    [[nodiscard]]
    auto& algorithm_parameters(this Self&& self) noexcept
    {
        return self.algorithm_parameters_;
    }

    /// The recipe of its own neighborhood.
    template<class Self>
    [[nodiscard]]
    auto& neighborhood(this Self&& self) noexcept
        requires has_own_neighborhood
    {
        return self.neighborhood_;
    }

    /// The same stage with one more option, applied after the others: a
    /// target, until_feasible() or a restart.
    template<class Modifier>
    [[nodiscard]]
    algorithm_stage<Algorithm, NeighborhoodSpec, Modifiers..., Modifier> with_modifier(
        Modifier modifier) &&
    {
        return {
            std::move(name_),
            std::move(algorithm_parameters_),
            std::move(neighborhood_),
            std::tuple_cat(
                std::move(modifiers_),
                std::tuple<Modifier>{std::move(modifier)}),
            parameters_,
        };
    }

    /// The stage of the runner of Algorithm on the recipes `solution_manager`
    /// and `app_neighborhood` (or its own), with its parameters and options.
    ///
    /// Throws `std::invalid_argument` when the algorithm's parameters are not
    /// valid.
    template<class SMSpec, class NHESpec>
    [[nodiscard]]
    auto on(const SMSpec& solution_manager, const NHESpec& app_neighborhood) const
    {
        auto runner = [&] {
            auto partial = easylocal::make_runner<Algorithm>(algorithm_parameters_)
                | solution_manager;
            if constexpr (has_own_neighborhood)
                return std::move(partial) | neighborhood_;
            else
                return std::move(partial) | app_neighborhood;
        }();
        pipeline_stage<decltype(runner)> stage{name_, std::move(runner)};
        stage.parameters() = parameters_;
        return std::apply(
            [&stage](const auto&... modifier) {
                return (std::move(stage) & ... & modifier);
            },
            modifiers_);
    }

private:
    std::string name_;
    parameters_type algorithm_parameters_;
    EASYLOCAL_NO_UNIQUE_ADDRESS NeighborhoodSpec neighborhood_;
    std::tuple<Modifiers...> modifiers_;
    StageParameters parameters_;
};

/// The stage `name` of a pipeline registered in an app, running Algorithm with
/// `parameters` on the app's recipes: `stage<FirstImprovement>("descent")`.
///
/// Requires an algorithm with a default-constructible parameters_type,
/// constructible from it.
template<class Algorithm>
    requires detail::stage_algorithm<Algorithm>
[[nodiscard]]
algorithm_stage<Algorithm> stage(
    std::string name,
    typename Algorithm::parameters_type parameters = {})
{
    return {std::move(name), std::move(parameters)};
}

/// The stage `name` of a pipeline registered in an app, running Algorithm with
/// `parameters` on its own neighborhood, built from the recipe `neighborhood`
/// over the app's SolutionManager: `stage<SA>("anneal", {...},
/// neighborhood<Swap>())`.
///
/// Requires an algorithm with a default-constructible parameters_type,
/// constructible from it, and a neighborhood recipe.
template<class Algorithm, class Spec>
    requires detail::stage_algorithm<Algorithm>
    && easylocal::detail::is_neighborhood_spec_v<std::remove_cvref_t<Spec>>
[[nodiscard]]
algorithm_stage<Algorithm, std::remove_cvref_t<Spec>> stage(
    std::string name,
    typename Algorithm::parameters_type parameters,
    Spec&& neighborhood)
{
    return {std::move(name), std::move(parameters), std::forward<Spec>(neighborhood)};
}

/// The algorithm stage with a number of attempts, as a stage of a runner.
///
/// Throws `std::invalid_argument` when the count is 0.
template<class Algorithm, class Spec, class... Modifiers>
[[nodiscard]]
algorithm_stage<Algorithm, Spec, Modifiers...> operator&(
    algorithm_stage<Algorithm, Spec, Modifiers...> stage,
    const stage_attempts attempts)
{
    stage.parameters().attempts = attempts.count;
    if (const auto valid = stage.parameters().validate(); !valid)
        throw std::invalid_argument{std::string{valid.message}};
    return stage;
}

/// The algorithm stage with a time limit or an evaluation budget, as a stage
/// of a runner: `stage<A>("a") & timeout(10s)`.
///
/// Throws `std::invalid_argument` when the options carry a run control.
template<class Algorithm, class Spec, class... Modifiers>
[[nodiscard]]
algorithm_stage<Algorithm, Spec, Modifiers...> operator&(
    algorithm_stage<Algorithm, Spec, Modifiers...> stage,
    const run_options<trace::null_tracer>& options)
{
    detail::require_stage_limits(options);
    if (options.time_limit)
    {
        stage.parameters().timeout =
            std::chrono::duration<double>{*options.time_limit}.count();
    }
    if (!options.evaluation_limit.is_unlimited())
        stage.parameters().max_evaluations = options.evaluation_limit;
    return stage;
}

/// The algorithm stage with a target, until_feasible() or a restart, applied
/// when the stage is built on the app's recipes, in the order they are given.
///
/// until_feasible() requires the app's cost to be hierarchical
/// (`cost::hierarchical`); a target is written in the stage's cost, and a
/// restart checked against the app's SolutionManager.
template<class Algorithm, class Spec, class... Modifiers, class Modifier>
    requires std::same_as<Modifier, stage_until_feasible>
    || requires { typename Modifier::stage_option_tag; }
[[nodiscard]]
auto operator&(algorithm_stage<Algorithm, Spec, Modifiers...> stage, Modifier modifier)
{
    return std::move(stage).with_modifier(std::move(modifier));
}

/// What one stage of a pipeline did.
struct stage_report
{
    /// The stage's name.
    std::string name;
    /// The runs of the stage.
    std::size_t attempts{};
    /// Solutions and moves evaluated by all its runs.
    std::size_t evaluations{};
    /// Iterations of all its runs.
    std::size_t iterations{};
    /// Why the stage stopped, when its results report it: a run cancelled,
    /// out of time or at the target, the budget spent, or else the end of its
    /// last attempt.
    std::optional<termination_reason> termination;
    /// The cost of its best run, as cost::to_text writes it, or empty when the
    /// cost cannot be written as text.
    std::string cost;
};

/// The result of a pipeline: the last stage's result, with the effort of every
/// stage, and what each stage did.
template<class Result>
struct pipeline_result : Result
{
    /// One report per stage, in order.
    std::vector<stage_report> stages;
};

namespace detail
{

template<class T>
inline constexpr bool is_pipeline_stage_v = false;

template<class Runner>
inline constexpr bool is_pipeline_stage_v<pipeline_stage<Runner>> = true;

// The algorithm of a runner, for the traits of the built-in algorithms.
template<class Runner>
struct runner_algorithm
{
};

template<class Algorithm, class SMSpec, class NHESpec>
struct runner_algorithm<easylocal::Runner<Algorithm, SMSpec, NHESpec>>
{
    using type = Algorithm;
};

// Whether a stage's runner runs an algorithm that declares itself
// deterministic (static constexpr bool deterministic = true): every run from
// the same solution is the same.
template<class Runner>
inline constexpr bool deterministic_runner_v = requires {
    requires runner_algorithm<Runner>::type::deterministic;
};

// Throws std::invalid_argument when a stage would repeat the same run: more
// than one attempt of a deterministic algorithm, every attempt from the same
// solution (fixed_start), without restart().
template<class Stage>
void check_repeated_attempts(const Stage& stage, const bool fixed_start)
{
    if constexpr (is_pipeline_stage_v<Stage>)
    {
        if constexpr (deterministic_runner_v<typename Stage::runner_type>)
        {
            if (fixed_start && stage.parameters().attempts > 1 && !stage.restarts())
            {
                throw std::invalid_argument{
                    "pipeline stage '" + stage.name()
                    + "': its algorithm is deterministic, so its attempts from the "
                      "same solution would repeat the same run; give it "
                      "& restart(initialization::random), or one attempt"};
            }
        }
    }
}

template<class T>
inline constexpr bool is_algorithm_stage_v = false;

template<class Algorithm, class Spec, class... Modifiers>
inline constexpr bool
    is_algorithm_stage_v<algorithm_stage<Algorithm, Spec, Modifiers...>> = true;

// A stage of a pipeline: of a runner, or of an algorithm on an app's recipes.
template<class Stage>
concept any_stage = is_pipeline_stage_v<Stage> || is_algorithm_stage_v<Stage>;

// Adds the parameters of a stage under its name: its own, and its runner's
// (`search.*`, `cost.*`, `neighborhood.*`) or, for an algorithm stage, its
// algorithm's (`search.*`) and its own neighborhood's (`neighborhood.*`).
// Read-only when the stage is const.
template<class Stage>
void add_stage_configuration(config::parameter_set& parameters, Stage& stage)
{
    using stage_type = std::remove_const_t<Stage>;
    parameters.add(stage.name(), stage.parameters());
    if constexpr (is_pipeline_stage_v<stage_type>)
        config::add_configuration(parameters, stage.name(), stage.runner());
    else
    {
        if constexpr (config::parameter_block<typename stage_type::parameters_type>)
            parameters.add(stage.name() + ".search", stage.algorithm_parameters());
        if constexpr (stage_type::has_own_neighborhood)
        {
            config::add_configuration(
                parameters,
                stage.name() + ".neighborhood",
                stage.neighborhood());
        }
    }
}

// Throws std::invalid_argument when two stages have the same name, or one has
// none.
template<class... Stages>
void check_stage_names(const Stages&... stages)
{
    const std::vector<std::string_view> names{std::string_view{stages.name()}...};
    std::set<std::string_view> seen;
    for (const auto name : names)
    {
        if (name.empty())
            throw std::invalid_argument{"a pipeline stage needs a name"};
        if (!seen.insert(name).second)
        {
            throw std::invalid_argument{
                "two pipeline stages are named '" + std::string{name} + "'"};
        }
    }
}

// The stage of a runner a stage is on an app's recipes: itself, or the
// algorithm's stage built on them.
template<class Stage, class SMSpec, class NHESpec>
[[nodiscard]]
auto stage_on(
    const Stage& stage,
    const SMSpec& solution_manager,
    const NHESpec& neighborhood)
{
    if constexpr (is_pipeline_stage_v<Stage>)
        return stage;
    else
        return stage.on(solution_manager, neighborhood);
}

// Why a run ends the attempts of its stage, if it does: it was cancelled, ran
// out of time or reached the target (by its termination, or by its cost for
// results that do not report one).
template<class BoundRunner, class Result, class Target>
[[nodiscard]]
std::optional<termination_reason> ends_stage(
    const BoundRunner& bound_runner,
    const Result& result,
    const std::optional<Target>& target)
{
    if (const auto termination = easylocal::detail::ends_runs(result))
        return termination;
    if (target.has_value() && !bound_runner.better(*target, result.cost))
        return termination_reason::target_reached;
    return std::nullopt;
}

} // namespace detail

/// Runners in sequence over the same Input and Solution: each stage starts from
/// the solution of the previous one, the first from an initial solution.
///
/// A stage runs up to its attempts while it does not reach its target, keeping
/// its best run (with a cost::pareto cost, the front of all its runs); the
/// first stage starts every attempt from a new initial solution, the others
/// from the solution they received, unless a stage restarts them. The result
/// is the last stage's, with the effort of every stage. A caller's target
/// applies to the last stage only, and not even to it when it has its own:
/// the solve then ends target_reached at the stage's own target. Requires
/// stages with the same Input and Solution, and a first stage whose
/// SolutionManager builds an initial or a random solution.
template<std::uniform_random_bit_generator RNG, class... Stages>
class Pipeline
    : public easylocal::detail::solver_start<
          typename std::tuple_element_t<0, std::tuple<Stages...>>::bound_runner_type,
          RNG>
{
    using first_stage_type = std::tuple_element_t<0, std::tuple<Stages...>>;
    using last_stage_type =
        std::tuple_element_t<sizeof...(Stages) - 1, std::tuple<Stages...>>;
    using start_type = easylocal::detail::solver_start<
        typename first_stage_type::bound_runner_type,
        RNG>;

public:
    /// The random number generator it owns.
    using rng_type = RNG;
    /// The Input of the stages.
    using input_type = typename first_stage_type::input_type;
    /// The solution of the stages.
    using solution_type = typename first_stage_type::solution_type;
    /// The cost of the last stage, the cost of the result.
    using cost_type = typename last_stage_type::cost_type;

    static_assert(
        (std::same_as<typename Stages::input_type, input_type> && ...),
        "the stages of a pipeline must have the same Input");
    static_assert(
        (std::same_as<typename Stages::solution_type, solution_type> && ...),
        "the stages of a pipeline must have the same Solution");

    /// Whether the first stage can build an initial solution
    /// (`initial_solution()`).
    using start_type::supports_initial;
    /// Whether the first stage can build a random solution
    /// (`random_solution(rng)`).
    using start_type::supports_random;

    static_assert(
        supports_initial || supports_random,
        "the first stage of a pipeline must build an initial or a random solution");

    /// The number of stages.
    static constexpr std::size_t stage_count = sizeof...(Stages);

    /// The same pipeline, with its RNG seeded with `seed`: this pipeline on an
    /// lvalue, the moved pipeline on a temporary. Each solve() continues the
    /// stream, so the seed reproduces the sequence of solves.
    using start_type::seed;
    /// The same pipeline, building its initial solutions as `initialization`
    /// says: initialization::initial, random or automatic, rejected at compile
    /// time when the first stage does not support it. This pipeline on an
    /// lvalue, the moved pipeline on a temporary.
    using start_type::initialization;
    /// The RNG, which feeds the initial solutions and the runs.
    using start_type::rng;

    /// The stages, with `rng` and initialization::automatic.
    ///
    /// Throws `std::invalid_argument` when two stages have the same name, or
    /// one has none.
    Pipeline(std::tuple<Stages...> stages, RNG rng)
        : Pipeline(
              std::move(stages),
              std::move(rng),
              easylocal::detail::initialization_kind::automatic)
    {
    }

    /// The pipeline with `stage` after its stages, its RNG and initialization
    /// kept: `pipeline | stage`.
    template<class Runner>
    [[nodiscard]]
    Pipeline<RNG, Stages..., pipeline_stage<Runner>> then(pipeline_stage<Runner> stage) &&
    {
        return Pipeline<RNG, Stages..., pipeline_stage<Runner>>{
            std::tuple_cat(
                std::move(stages_),
                std::tuple<pipeline_stage<Runner>>{std::move(stage)}),
            std::move(this->rng_),
            this->kind()};
    }

    /// The pipeline with `stage` after its stages, its RNG and initialization
    /// kept: `pipeline | stage`.
    template<class Runner>
    [[nodiscard]]
    Pipeline<RNG, Stages..., pipeline_stage<Runner>> then(
        pipeline_stage<Runner> stage) const&
    {
        return Pipeline{*this}.then(std::move(stage));
    }

    /// The stage at `Index`.
    template<std::size_t Index, class Self>
    [[nodiscard]]
    auto& stage(this Self&& self) noexcept
    {
        return std::get<Index>(self.stages_);
    }

    /// Runs the stages in order on `input`, and returns the last stage's result
    /// with the effort and the report of every stage.
    ///
    /// The optional trailing run options (easylocal::with(control, tracer),
    /// .stop_at(target)) go to every stage, except the target, which applies to
    /// the last stage when it has none of its own. After a cancellation, or
    /// once the solve's time or evaluations are spent, the stages between the
    /// first and the last are skipped (0 attempts in their report) and the
    /// last evaluates the solution once.
    template<class... Options>
        requires easylocal::detail::solve_options<Options...>
    [[nodiscard]]
    auto solve(const input_type& input, const Options&... options)
    {
        check_attempts(
            easylocal::detail::fixed_start<
                typename first_stage_type::bound_runner_type,
                RNG>(this->kind()));
        return execute(
            input,
            [this](const auto& bound_runner) {
                return this->make_initial_solution(bound_runner);
            },
            this->rng_,
            options...);
    }

    /// Runs the stages in order on `input` from `solution`, with the caller's
    /// RNG, and returns the last stage's result with the effort and the report
    /// of every stage.
    ///
    /// The first stage starts from `solution`, and so do its further attempts;
    /// the pipeline's own RNG and initialization are not used. This is how an
    /// app runs a pipeline registered by name. The run options and the
    /// exceptions are those of solve().
    template<std::uniform_random_bit_generator Rng, class... Options>
        requires easylocal::detail::solve_options<Options...>
    [[nodiscard]]
    auto run(
        const input_type& input,
        const solution_type& solution,
        Rng& rng,
        const Options&... options) const
    {
        check_attempts(true);
        return execute(
            input,
            [&solution](const auto&) { return solution; },
            rng,
            options...);
    }

    /// The parameters of every stage under its name: its runner's and its own
    /// (`<name>.attempts`, `<name>.timeout`, `<name>.max_evaluations`),
    /// read-only when the pipeline is const. The set refers to this pipeline,
    /// which must stay in place while it is used: a temporary pipeline has no
    /// configuration().
    template<class Self>
    [[nodiscard]]
    config::parameter_set configuration(this Self& self)
    {
        config::parameter_set parameters;
        std::apply(
            [&parameters](auto&... stage) {
                (detail::add_stage_configuration(parameters, stage), ...);
            },
            self.stages_);
        return parameters;
    }

private:
    template<std::uniform_random_bit_generator, class...>
    friend class Pipeline;

    Pipeline(
        std::tuple<Stages...> stages,
        RNG rng,
        const easylocal::detail::initialization_kind kind)
        : start_type{std::move(rng), kind}, stages_{std::move(stages)}
    {
        // The names cannot change afterwards: checked once. The stages after
        // the first start every attempt from the solution they receive.
        std::apply(
            [](const auto&... stage) { detail::check_stage_names(stage...); },
            stages_);
        check_attempts(false);
    }

    // Throws std::invalid_argument when a stage of a deterministic algorithm
    // would repeat the same run in its attempts; first_fixed tells whether the
    // first stage's attempts all start from the same solution.
    void check_attempts(const bool first_fixed) const
    {
        [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            (detail::check_repeated_attempts(
                 std::get<Index>(stages_),
                 Index == 0 ? first_fixed : true),
                ...);
        }(std::index_sequence_for<Stages...>{});
    }

    // The stages from the first one, whose attempts start from
    // first_start(bound runner); the result with every stage's effort and
    // report.
    template<class FirstStart, class Rng, class... Options>
    [[nodiscard]]
    auto execute(
        const input_type& input,
        const FirstStart& first_start,
        Rng& rng,
        const Options&... options) const
    {
        std::vector<stage_report> reports;
        reports.reserve(stage_count);
        easylocal::detail::search_effort effort;
        // The solve's time limit and evaluation budget bound all the stages
        // together, and its progress goes on from one stage to the next.
        auto budget = easylocal::detail::solve_budget::of(options...);
        easylocal::detail::solve_progress progress{
            easylocal::detail::control_of(options...)};
        budget.progress = &progress;
        auto last = run_from<0>(
            input,
            std::nullopt,
            first_start,
            rng,
            budget,
            reports,
            effort,
            options...);
        effort.assign_to(last);
        using result_type = pipeline_result<decltype(last)>;
        return result_type{std::move(last), std::move(reports)};
    }

    // The stage at Index from `incoming` (none for the first stage), then the
    // following ones from its solution; the last stage's result. Each stage
    // binds its runner when it starts.
    template<std::size_t Index, class FirstStart, class Rng, class... Options>
    [[nodiscard]]
    auto run_from(
        const input_type& input,
        std::optional<solution_type> incoming,
        const FirstStart& first_start,
        Rng& rng,
        easylocal::detail::solve_budget& budget,
        std::vector<stage_report>& reports,
        easylocal::detail::search_effort& effort,
        const Options&... options) const
    {
        if constexpr (Index > 0 && Index + 1 < stage_count)
        {
            // After a cancellation, or once the solve's budget is spent, a
            // stage between the first and the last is skipped without binding
            // its runner; the last one still evaluates the solution.
            const auto skipped = easylocal::detail::stop_requested(options...)
                ? std::optional{termination_reason::cancelled}
                : budget.spent();
            if (skipped)
            {
                reports.push_back(
                    stage_report{
                        .name = std::get<Index>(stages_).name(),
                        .attempts = 0,
                        .evaluations = 0,
                        .iterations = 0,
                        .termination = skipped,
                        .cost = {},
                    });
                return run_from<Index + 1>(
                    input,
                    std::move(incoming),
                    first_start,
                    rng,
                    budget,
                    reports,
                    effort,
                    options...);
            }
        }
        auto bound_runner = std::get<Index>(stages_).runner().bind(input);
        auto result = run_stage<
            Index>(bound_runner, incoming, first_start, rng, budget, reports, options...);
        effort.evaluations += reports.back().evaluations;
        effort.iterations += reports.back().iterations;
        if constexpr (Index + 1 == stage_count)
            return result;
        else
            return run_from<Index + 1>(
                input,
                std::optional<solution_type>{std::move(result.solution)},
                first_start,
                rng,
                budget,
                reports,
                effort,
                options...);
    }

    // The attempts of the stage at Index, keeping the best, and its report.
    //
    // The first attempt starts from the solution the stage receives (for the
    // first stage, from the pipeline's start); the others from a new solution
    // when the stage restarts them, else as the first.
    template<
        std::size_t Index,
        class BoundRunner,
        class FirstStart,
        class Rng,
        class... Options>
    [[nodiscard]]
    auto run_stage(
        BoundRunner& bound_runner,
        const std::optional<solution_type>& incoming,
        const FirstStart& first_start,
        Rng& rng,
        easylocal::detail::solve_budget& budget,
        std::vector<stage_report>& reports,
        const Options&... options) const
    {
        const auto& stage = std::get<Index>(stages_);
        const auto received = [&]() -> solution_type {
            if constexpr (Index == 0)
                return first_start(bound_runner);
            else
                return *incoming;
        };
        const auto attempt_start = [&](const std::size_t attempt) -> solution_type {
            if (attempt > 0 && stage.restart_)
                return easylocal::detail::make_start_solution(
                    *stage.restart_,
                    bound_runner,
                    rng);
            return received();
        };
        auto outcome = easylocal::detail::run_attempts(
            bound_runner,
            stage.parameters().attempts,
            budget,
            stage.limits(),
            [&](const std::size_t attempt, const easylocal::detail::solve_budget& left) {
                return run_once<Index>(
                    bound_runner,
                    attempt_start(attempt),
                    attempt,
                    rng,
                    left,
                    options...);
            },
            [&](const auto& result) {
                return detail::ends_stage(bound_runner, result, stage.target());
            },
            options...);

        stage_report report{
            .name = stage.name(),
            .attempts = outcome.attempts,
            .evaluations = outcome.effort.evaluations,
            .iterations = outcome.effort.iterations,
            .termination = outcome.termination,
            .cost = {},
        };
        if constexpr (cost::text_readable<typename BoundRunner::cost_type>)
            report.cost = cost::to_text(outcome.best.cost);
        reports.push_back(std::move(report));
        return std::move(outcome.best);
    }

    // The attempt of the stage at Index, with what is left of the stage's
    // budget: its target if it has one, the solve's target for the last stage,
    // none otherwise.
    template<std::size_t Index, class BoundRunner, class Rng, class... Options>
    [[nodiscard]]
    auto run_once(
        BoundRunner& bound_runner,
        solution_type solution,
        const std::size_t attempt,
        Rng& rng,
        const easylocal::detail::solve_budget& budget,
        const Options&... options) const
    {
        const auto& stage = std::get<Index>(stages_);
        easylocal::detail::emit_run_context(stage.name(), Index, attempt, options...);
        const auto timed = budget.options_for_run(options...);
        // One run path: the options typed with the stage's cost, whose target
        // is the stage's own, else the caller's for the last stage, else none.
        using stage_cost = typename BoundRunner::cost_type;
        using tracer_type = typename std::remove_cvref_t<decltype(timed)>::tracer_type;
        run_options<tracer_type, stage_cost> typed{
            .control = timed.control,
            .tracer = timed.tracer,
            .target = std::nullopt,
            .time_limit = timed.time_limit,
            .evaluation_limit = timed.evaluation_limit,
            .front = timed.front,
        };
        if (stage.target().has_value())
            typed.target = *stage.target();
        else if constexpr (Index + 1 == stage_count)
        {
            using target_type =
                typename std::remove_cvref_t<decltype(timed)>::target_type;
            if constexpr (!std::same_as<target_type, no_target>)
            {
                static_assert(
                    std::constructible_from<stage_cost, const target_type&>,
                    "the target cost of the run options must convert to the last "
                    "stage's cost type");
                if (timed.target)
                    typed.target.emplace(*timed.target);
            }
        }
        // A stage on another cost than the caller's recorder (until_feasible())
        // gives it only its events without a cost.
        return easylocal::detail::with_tracer_of_cost<stage_cost>(
            typed,
            [&](const auto& run_options) {
                return easylocal::detail::run_with_solver_rng(
                    bound_runner,
                    std::move(solution),
                    rng,
                    run_options);
            });
    }

    std::tuple<Stages...> stages_;
};

/// The pipeline with one more stage: `pipeline.then(stage)`.
template<std::uniform_random_bit_generator RNG, class... Stages, class Runner>
[[nodiscard]]
auto operator|(Pipeline<RNG, Stages...> pipeline, pipeline_stage<Runner> stage)
{
    return std::move(pipeline).then(std::move(stage));
}

/// A pipeline of `stages`, in order: `pipeline(a, b, c)` is `a | b | c`, and
/// `pipeline(a)` the pipeline of one stage, which `then()` extends.
///
/// RNG, the type of the pipeline's random number generator, may be given:
/// `pipeline<std::minstd_rand>(a, b)`.
template<
    std::uniform_random_bit_generator RNG = std::mt19937_64,
    class Runner,
    class... Stages>
[[nodiscard]]
auto pipeline(pipeline_stage<Runner> first, Stages... rest)
{
    using first_type = pipeline_stage<Runner>;
    return (
        Pipeline<RNG, first_type>{
            std::tuple<first_type>{std::move(first)},
            RNG{std::uint64_t{0}}}
        | ... | std::move(rest));
}

/// The pipeline of two stages: `pipeline(first, second)`.
template<class FirstRunner, class SecondRunner>
[[nodiscard]]
auto operator|(pipeline_stage<FirstRunner> first, pipeline_stage<SecondRunner> second)
{
    return solvers::pipeline(std::move(first), std::move(second));
}

} // namespace easylocal::solvers
