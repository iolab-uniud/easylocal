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
        parameters_.timeout = steady == std::chrono::steady_clock::duration::max()
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

    /// The limits of the stage started now: its deadline and its evaluations,
    /// none when it has no limit of its own.
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

    /// The same stage on the hard cost only, until it is zero: a feasible
    /// solution.
    ///
    /// The runner becomes `runner.with_hard_cost()` and the target the zero of
    /// the hard cost; the name and the attempts stay. Requires a runner with a
    /// hierarchical cost (`cost::hierarchical`).
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
        return stage;
    }

    /// The same stage on the hard cost only, until it is zero: a feasible
    /// solution.
    ///
    /// The runner becomes `runner.with_hard_cost()` and the target the zero of
    /// the hard cost; the name and the attempts stay. Requires a runner with a
    /// hierarchical cost (`cost::hierarchical`).
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

    std::string name_;
    Runner runner_;
    std::optional<cost_type> target_;
    StageParameters parameters_{};
};

/// The stage `name` of a pipeline, running `runner`.
template<class Runner>
[[nodiscard]]
pipeline_stage<Runner> stage(std::string name, Runner runner)
{
    return {std::move(name), std::move(runner)};
}

/// A stage modifier: the stage stops as soon as its best cost reaches `cost`.
template<class Cost>
struct stage_target
{
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

/// The stage with a target: `stage.with_target(target.cost)`.
template<class Runner, class Cost>
[[nodiscard]]
pipeline_stage<Runner> operator&(pipeline_stage<Runner> stage, stage_target<Cost> target)
{
    using cost_type = typename pipeline_stage<Runner>::cost_type;
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

/// The stage with a time limit or an evaluation budget: `stage & timeout(10s)`,
/// as `stage.with_timeout(10s)`, and `stage & max_evaluations(5000)`, as
/// `stage.with_max_evaluations(5000)`. The options given carry only limits.
template<class Runner>
[[nodiscard]]
pipeline_stage<Runner> operator&(
    pipeline_stage<Runner> stage,
    const run_options<trace::null_tracer>& options)
{
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

template<std::uniform_random_bit_generator RNG, class... Stages>
class Pipeline;

/// Runners in sequence over the same Input and Solution: each stage starts from
/// the solution of the previous one, the first from an initial solution.
///
/// A stage runs up to its attempts while it does not reach its target, keeping
/// its best run (with a cost::pareto cost, the front of all its runs); the
/// first stage starts every attempt from a new initial solution, the others
/// from the solution they received. The result is the last stage's, with the
/// effort of every stage. Requires stages with the same Input and Solution, and
/// a first stage whose SolutionManager builds an initial or a random solution.
template<std::uniform_random_bit_generator RNG, class... Stages>
class Pipeline
    : public easylocal::detail::InitializationSupport<
          typename std::tuple_element_t<0, std::tuple<Stages...>>::bound_runner_type,
          RNG>
{
    using first_stage_type = std::tuple_element_t<0, std::tuple<Stages...>>;
    using last_stage_type =
        std::tuple_element_t<sizeof...(Stages) - 1, std::tuple<Stages...>>;
    using initialization_support = easylocal::detail::InitializationSupport<
        typename first_stage_type::bound_runner_type,
        RNG>;
    // What a builder returns: a reference to the pipeline on an lvalue, the
    // pipeline itself on a temporary.
    template<class Self>
    using builder_result =
        std::conditional_t<std::is_lvalue_reference_v<Self>, Pipeline&, Pipeline>;

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
    using initialization_support::supports_initial;
    /// Whether the first stage can build a random solution
    /// (`random_solution(rng)`).
    using initialization_support::supports_random;

    static_assert(
        supports_initial || supports_random,
        "the first stage of a pipeline must build an initial or a random solution");

    /// The number of stages.
    static constexpr std::size_t stage_count = sizeof...(Stages);

    /// The stages, initialized randomly when the first stage supports it,
    /// otherwise from its initial solution.
    Pipeline(std::tuple<Stages...> stages, RNG rng)
        : Pipeline(
              std::move(stages),
              std::move(rng),
              supports_random ? initialization::Mode::random
                              : initialization::Mode::initial)
    {
    }

    /// The same pipeline, with its RNG seeded with `seed`: this pipeline on an
    /// lvalue, the moved pipeline on a temporary. Each solve() continues the
    /// stream, so the seed reproduces the sequence of solves.
    template<class Self>
        requires std::constructible_from<RNG, std::uint64_t>
    builder_result<Self> seed(this Self&& self, const std::uint64_t seed)
    {
        Pipeline& pipeline = self;
        pipeline.rng_ = RNG{seed};
        return std::forward<Self>(self);
    }

    /// The same pipeline, building its initial solutions as `initialization`
    /// says: initialization::initial or random, rejected at compile time when
    /// the first stage does not support it, or a Mode, checked here. This
    /// pipeline on an lvalue, the moved pipeline on a temporary.
    template<class Self, class Initialization>
        requires easylocal::detail::accepted_initialization<
            Initialization,
            typename first_stage_type::bound_runner_type,
            RNG>
    builder_result<Self> initialization(
        this Self&& self,
        const Initialization initialization)
    {
        Pipeline& pipeline = self;
        pipeline.initialization_mode(initialization_support::to_mode(initialization));
        return std::forward<Self>(self);
    }

    /// The pipeline with `stage` after its stages, its RNG and initialization
    /// kept: `pipeline | stage`.
    template<class Runner>
    [[nodiscard]]
    Pipeline<RNG, Stages..., pipeline_stage<Runner>> then(pipeline_stage<Runner> stage) &&
    {
        const auto mode = this->initialization_mode();
        return Pipeline<RNG, Stages..., pipeline_stage<Runner>>{
            std::tuple_cat(
                std::move(stages_),
                std::tuple<pipeline_stage<Runner>>{std::move(stage)}),
            std::move(rng_),
            mode};
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

    /// The RNG, which feeds the initial solutions and the runs.
    template<class Self>
    [[nodiscard]]
    auto& rng(this Self&& self) noexcept
    {
        return self.rng_;
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
    /// last evaluates the solution once. Throws `std::invalid_argument` when
    /// two stages have the same name, or one has none.
    template<class... Options>
        requires easylocal::detail::solve_options<Options...>
    [[nodiscard]]
    auto solve(const input_type& input, const Options&... options)
    {
        return execute(
            input,
            [this](const auto& bound_runner) {
                return this->make_initial_solution(bound_runner, rng_);
            },
            rng_,
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
    ///
    /// Throws `std::invalid_argument` when two stages have the same name, or
    /// one has none.
    template<class Self>
    [[nodiscard]]
    config::parameter_set configuration(this Self& self)
    {
        self.check_names();
        config::parameter_set parameters;
        std::apply(
            [&parameters](auto&... stage) {
                (add_stage_configuration(parameters, stage), ...);
            },
            self.stages_);
        return parameters;
    }

private:
    template<std::uniform_random_bit_generator, class...>
    friend class Pipeline;

    Pipeline(std::tuple<Stages...> stages, RNG rng, const initialization::Mode mode)
        : initialization_support{mode}, stages_{std::move(stages)}, rng_{std::move(rng)}
    {
    }

    template<class Stage>
    static void add_stage_configuration(config::parameter_set& parameters, Stage& stage)
    {
        parameters.add(stage.name(), stage.parameters());
        config::add_configuration(parameters, stage.name(), stage.runner());
    }

    void check_names() const
    {
        const auto names = std::apply(
            [](const auto&... stage) {
                return std::vector<std::string_view>{stage.name()...};
            },
            stages_);
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
        check_names();
        std::vector<stage_report> reports;
        reports.reserve(stage_count);
        easylocal::detail::search_effort effort;
        // The solve's time limit and evaluation budget bound all the stages
        // together.
        auto budget = easylocal::detail::solve_budget::of(options...);
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
        auto result = run_stage<Index>(
            bound_runner,
            incoming,
            first_start,
            rng,
            budget,
            reports,
            effort,
            options...);
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

    // The attempts of the stage at Index, keeping the best.
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
        easylocal::detail::search_effort& effort,
        const Options&... options) const
    {
        const auto& stage = std::get<Index>(stages_);
        // What is left of the solve's budget, at most the stage's own limits.
        auto stage_budget = budget.within(stage.limits());
        const auto start = [&]() -> solution_type {
            if constexpr (Index == 0)
                return first_start(bound_runner);
            else
                return *incoming;
        };

        easylocal::detail::search_effort stage_effort;
        auto best =
            run_once<Index>(bound_runner, start(), 0, rng, stage_budget, options...);
        stage_effort.add(best);
        easylocal::detail::merged_front<decltype(best)> front{
            easylocal::detail::front_parameters_of(options...)};
        front.add(bound_runner, best);
        stage_budget.consume(best);
        budget.consume(best);
        auto termination = detail::ends_stage(bound_runner, best, stage.target());
        auto last_termination = easylocal::detail::termination_of(best);
        std::size_t attempts = 1;
        for (; attempts < stage.parameters().attempts && !termination; ++attempts)
        {
            if (easylocal::detail::stop_requested(options...))
            {
                termination = termination_reason::cancelled;
                break;
            }
            if (stage_budget.spent())
                break;
            auto candidate = run_once<
                Index>(bound_runner, start(), attempts, rng, stage_budget, options...);
            stage_effort.add(candidate);
            front.add(bound_runner, candidate);
            stage_budget.consume(candidate);
            budget.consume(candidate);
            termination = detail::ends_stage(bound_runner, candidate, stage.target());
            last_termination = easylocal::detail::termination_of(candidate);
            if (bound_runner.better(candidate.cost, best.cost))
                best = std::move(candidate);
        }
        // Why the stage stopped: a run that ended it, the time or the
        // evaluations running out, before an attempt or during the last one,
        // or else the end of its last attempt.
        if (!termination)
            termination = stage_budget.spent();
        if (!termination)
            termination = last_termination;
        if (termination)
            easylocal::detail::set_termination(best, *termination);
        front.assign_to(best);

        stage_report report{
            .name = stage.name(),
            .attempts = attempts,
            .evaluations = stage_effort.evaluations,
            .iterations = stage_effort.iterations,
            .termination = termination,
            .cost = {},
        };
        if constexpr (cost::text_readable<typename BoundRunner::cost_type>)
            report.cost = cost::to_text(best.cost);
        reports.push_back(std::move(report));
        effort.evaluations += stage_effort.evaluations;
        effort.iterations += stage_effort.iterations;
        return best;
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
        if (stage.target().has_value())
        {
            return easylocal::detail::run_with_solver_rng(
                bound_runner,
                std::move(solution),
                rng,
                timed.stop_at(*stage.target()));
        }
        if constexpr (Index + 1 == stage_count)
        {
            return easylocal::detail::run_with_solver_rng(
                bound_runner,
                std::move(solution),
                rng,
                timed);
        }
        else
        {
            return easylocal::detail::run_with_solver_rng(
                bound_runner,
                std::move(solution),
                rng,
                timed.without_target());
        }
    }

    std::tuple<Stages...> stages_;
    RNG rng_;
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
