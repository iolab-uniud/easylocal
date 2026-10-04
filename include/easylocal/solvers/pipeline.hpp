#pragma once

/// \file
/// solvers::Pipeline: runners in sequence over the same Input and Solution,
/// each stage starting from the solution of the previous one.
///
///     using namespace easylocal::solvers;
///     auto solver = (stage("feasible", descent) & until_feasible() & attempts(10))
///         | stage("descent", descent)
///         | stage("anneal", annealing);
///     auto result = solver.seed(7).solve(input);
///
/// The same pipeline, spelled out:
///
///     auto solver = pipeline(stage("feasible",
///     descent).until_feasible().with_attempts(10))
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

#include <concepts>
#include <cstddef>
#include <cstdint>
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

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"attempts", &StageParameters::attempts>(
                "Runs of the stage while its target is not reached; the best is kept"));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return attempts == 0
            ? config::validation_result::failure("a stage needs at least one attempt")
            : config::validation_result::success();
    }
};

template<class Runner>
class pipeline_stage;

/// A stage of a pipeline: a named runner, with an optional target cost and a
/// number of attempts.
///
/// A stage is built by stage(name, runner) and refined by with_target(),
/// with_attempts() and until_feasible(), each returning the refined stage, or
/// with `&` and the modifiers target(), attempts() and until_feasible().
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
    [[nodiscard]]
    Runner& runner() noexcept
    {
        return runner_;
    }

    /// The runner.
    [[nodiscard]]
    const Runner& runner() const noexcept
    {
        return runner_;
    }

    /// The target cost, if any.
    [[nodiscard]]
    const std::optional<cost_type>& target() const noexcept
    {
        return target_;
    }

    /// The stage's own parameters (attempts).
    [[nodiscard]]
    StageParameters& parameters() noexcept
    {
        return parameters_;
    }

    /// The stage's own parameters (attempts).
    [[nodiscard]]
    const StageParameters& parameters() const noexcept
    {
        return parameters_;
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
    /// Why its best run ended, when the result reports it.
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

// Run options without their target cost: those of a stage whose cost may not
// be the target's.
template<class Tracer, class Target>
[[nodiscard]]
run_options<Tracer> without_target(const run_options<Tracer, Target>& options) noexcept
{
    return {.control = options.control, .tracer = options.tracer};
}

// Whether a run ends the attempts of its stage: it reached the stage's target
// (by its termination, or by its cost for results that do not report one) or
// was cancelled.
template<class BoundRunner, class Result, class Target>
[[nodiscard]]
bool ends_stage(
    const BoundRunner& bound_runner,
    const Result& result,
    const std::optional<Target>& target)
{
    const auto termination = easylocal::detail::termination_of(result);
    if (termination == termination_reason::target_reached
        || termination == termination_reason::cancelled)
    {
        return true;
    }
    return target.has_value() && !bound_runner.better(*target, result.cost);
}

} // namespace detail

template<std::uniform_random_bit_generator RNG, class... Stages>
class Pipeline;

/// Runners in sequence over the same Input and Solution: each stage starts from
/// the solution of the previous one, the first from an initial solution.
///
/// A stage runs up to its attempts while it does not reach its target, keeping
/// its best run; the first stage starts every attempt from a new initial
/// solution, the others from the solution they received. The result is the last
/// stage's, with the effort of every stage. Requires stages with the same Input
/// and Solution, and a first stage whose SolutionManager builds an initial or a
/// random solution.
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

public:
    /// The random number generator it owns.
    using rng_type = RNG;
    /// The Input of the stages.
    using input_type = typename first_stage_type::input_type;
    /// The solution of the stages.
    using solution_type = typename first_stage_type::solution_type;

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

    /// The same pipeline, with its RNG seeded with `seed`.
    Pipeline& seed(const std::uint64_t seed) &
        requires std::constructible_from<RNG, std::uint64_t>
    {
        rng_ = RNG{seed};
        return *this;
    }

    /// The same pipeline, with its RNG seeded with `seed`.
    [[nodiscard]]
    Pipeline&& seed(const std::uint64_t seed) &&
        requires std::constructible_from<RNG, std::uint64_t>
    {
        rng_ = RNG{seed};
        return std::move(*this);
    }

    /// The same pipeline, building its initial solutions as `initialization`
    /// says: initialization::initial or random, rejected at compile time when the
    /// first stage does not support it, or a Mode, checked here.
    template<class Initialization>
        requires easylocal::detail::accepted_initialization<
            Initialization,
            typename first_stage_type::bound_runner_type,
            RNG>
    Pipeline& initialization(const Initialization initialization) &
    {
        this->initialization_mode(to_mode(initialization));
        return *this;
    }

    /// The same pipeline, building its initial solutions as `initialization`
    /// says: initialization::initial or random, rejected at compile time when the
    /// first stage does not support it, or a Mode, checked here.
    template<class Initialization>
        requires easylocal::detail::accepted_initialization<
            Initialization,
            typename first_stage_type::bound_runner_type,
            RNG>
    [[nodiscard]]
    Pipeline&& initialization(const Initialization initialization) &&
    {
        this->initialization_mode(to_mode(initialization));
        return std::move(*this);
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
    [[nodiscard]]
    RNG& rng() noexcept
    {
        return rng_;
    }

    /// The RNG, which feeds the initial solutions and the runs.
    [[nodiscard]]
    const RNG& rng() const noexcept
    {
        return rng_;
    }

    /// The stage at `Index`.
    template<std::size_t Index>
    [[nodiscard]]
    auto& stage() noexcept
    {
        return std::get<Index>(stages_);
    }

    /// The stage at `Index`.
    template<std::size_t Index>
    [[nodiscard]]
    const auto& stage() const noexcept
    {
        return std::get<Index>(stages_);
    }

    /// Runs the stages in order on `input`, and returns the last stage's result
    /// with the effort and the report of every stage.
    ///
    /// The optional trailing run options (easylocal::with(control, tracer),
    /// .stop_at(target)) go to every stage, except the target, which applies to
    /// the last stage when it has none of its own. After a cancellation the
    /// remaining stages stop at once. Throws `std::invalid_argument` when two
    /// stages have the same name, or one has none.
    template<class... Options>
        requires easylocal::detail::solve_options<Options...>
    [[nodiscard]]
    auto solve(const input_type& input, const Options&... options)
    {
        check_names();
        std::vector<stage_report> reports;
        reports.reserve(stage_count);
        easylocal::detail::search_effort effort;
        auto last = run_from<0>(input, std::nullopt, reports, effort, options...);
        effort.assign_to(last);
        using result_type = pipeline_result<decltype(last)>;
        return result_type{std::move(last), std::move(reports)};
    }

    /// The parameters of every stage under its name: its runner's and its own
    /// (`<name>.attempts`).
    ///
    /// Throws `std::invalid_argument` when two stages have the same name, or
    /// one has none.
    [[nodiscard]]
    config::parameter_set configuration()
    {
        check_names();
        config::parameter_set parameters;
        std::apply(
            [&parameters](auto&... stage) {
                (add_stage_configuration(parameters, stage), ...);
            },
            stages_);
        return parameters;
    }

private:
    template<std::uniform_random_bit_generator, class...>
    friend class Pipeline;

    Pipeline(std::tuple<Stages...> stages, RNG rng, const initialization::Mode mode)
        : initialization_support{mode}, stages_{std::move(stages)}, rng_{std::move(rng)}
    {
    }

    static constexpr initialization::Mode to_mode(const initialization::Initial) noexcept
    {
        return initialization::Mode::initial;
    }

    static constexpr initialization::Mode to_mode(const initialization::Random) noexcept
    {
        return initialization::Mode::random;
    }

    static constexpr initialization::Mode to_mode(
        const initialization::Mode mode) noexcept
    {
        return mode;
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

    // The stage at Index from `incoming` (none for the first stage), then the
    // following ones from its solution; the last stage's result. Each stage
    // binds its runner when it starts.
    template<std::size_t Index, class... Options>
    [[nodiscard]]
    auto run_from(
        const input_type& input,
        std::optional<solution_type> incoming,
        std::vector<stage_report>& reports,
        easylocal::detail::search_effort& effort,
        const Options&... options)
    {
        auto bound_runner = std::get<Index>(stages_).runner().bind(input);
        auto result =
            run_stage<Index>(bound_runner, incoming, reports, effort, options...);
        if constexpr (Index + 1 == stage_count)
            return result;
        else
            return run_from<Index + 1>(
                input,
                std::optional<solution_type>{std::move(result.solution)},
                reports,
                effort,
                options...);
    }

    // The attempts of the stage at Index, keeping the best.
    template<std::size_t Index, class BoundRunner, class... Options>
    [[nodiscard]]
    auto run_stage(
        BoundRunner& bound_runner,
        const std::optional<solution_type>& incoming,
        std::vector<stage_report>& reports,
        easylocal::detail::search_effort& effort,
        const Options&... options)
    {
        const auto& stage = std::get<Index>(stages_);
        const auto start = [&] {
            if constexpr (Index == 0)
                return this->make_initial_solution(bound_runner, rng_);
            else
                return *incoming;
        };

        easylocal::detail::search_effort stage_effort;
        auto best = run_once<Index>(bound_runner, start(), options...);
        stage_effort.add(best);
        bool ended = detail::ends_stage(bound_runner, best, stage.target());
        std::size_t attempts = 1;
        for (; attempts < stage.parameters().attempts && !ended
            && !easylocal::detail::stop_requested(options...);
            ++attempts)
        {
            auto candidate = run_once<Index>(bound_runner, start(), options...);
            stage_effort.add(candidate);
            ended = detail::ends_stage(bound_runner, candidate, stage.target());
            if (bound_runner.better(candidate.cost, best.cost))
                best = std::move(candidate);
        }

        stage_report report{
            .name = stage.name(),
            .attempts = attempts,
            .evaluations = stage_effort.evaluations,
            .iterations = stage_effort.iterations,
            .termination = easylocal::detail::termination_of(best),
            .cost = {},
        };
        if constexpr (cost::text_readable<typename BoundRunner::cost_type>)
            report.cost = cost::to_text(best.cost);
        reports.push_back(std::move(report));
        effort.evaluations += stage_effort.evaluations;
        effort.iterations += stage_effort.iterations;
        return best;
    }

    // One run of the stage at Index: its target if it has one, the solve's
    // target for the last stage, none otherwise.
    template<std::size_t Index, class BoundRunner, class... Options>
    [[nodiscard]]
    auto run_once(
        BoundRunner& bound_runner,
        solution_type solution,
        const Options&... options)
    {
        const auto& stage = std::get<Index>(stages_);
        if (stage.target().has_value())
        {
            return easylocal::detail::run_with_solver_rng(
                bound_runner,
                std::move(solution),
                rng_,
                easylocal::detail::with_target(*stage.target(), options...));
        }
        if constexpr (Index + 1 == stage_count)
        {
            return easylocal::detail::run_with_solver_rng(
                bound_runner,
                std::move(solution),
                rng_,
                options...);
        }
        else
        {
            return easylocal::detail::run_with_solver_rng(
                bound_runner,
                std::move(solution),
                rng_,
                detail::without_target(options)...);
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
        Pipeline<RNG, first_type>{std::tuple<first_type>{std::move(first)}, RNG{}} | ...
        | std::move(rest));
}

/// The pipeline of two stages: `pipeline(first, second)`.
template<class FirstRunner, class SecondRunner>
[[nodiscard]]
auto operator|(pipeline_stage<FirstRunner> first, pipeline_stage<SecondRunner> second)
{
    return pipeline(std::move(first), std::move(second));
}

} // namespace easylocal::solvers
