#pragma once

/// \file
/// search_run: one execution of a search algorithm.
///
/// Algorithms are written in terms of its primitives (evaluate, commit,
/// random_move, ...), while it keeps the counters, the evaluation budget,
/// cancellation, progress, the target cost and the trace events. Also
/// run_options/with() for the caller's options, termination_reason and the
/// result types.

#include <easylocal/cost/pareto.hpp>
#include <easylocal/cost/tolerance.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/pareto_archive.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/trace/events.hpp>
#include <easylocal/trace/tracer.hpp>
#include <easylocal/utils/detail/attributes.hpp>
#include <easylocal/utils/limit.hpp>
#include <easylocal/utils/termination.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal
{

/// What solvers, runs by name (app.run) and the adapters read of a result: the
/// final solution and its cost.
///
/// search_result models it; a custom runner may return a richer type.
template<class Result, class Solution, class Cost>
concept search_result_for =
    requires(Result& result, const Result& const_result) {
        { std::move(result.solution) } -> std::convertible_to<Solution>;
        { const_result.cost } -> std::convertible_to<const Cost&>;
    };

/// The result of a search: the solution it returns, its cost, the effort it
/// took and why it ended.
template<class Solution, class Cost>
struct search_result
{
    /// The solution the algorithm returns: the best one found, for the built-in
    /// algorithms.
    Solution solution;
    /// Its cost.
    Cost cost;
    /// Solutions and moves evaluated, the initial evaluation included.
    std::size_t evaluations{};
    /// Iterations, as the algorithm counts them.
    std::size_t iterations{};
    /// Why the run ended.
    termination_reason termination{};
};

/// The result of a search with a cost::pareto cost: a solution of the front
/// with its cost, and the whole front (the non-dominated solutions reached,
/// ordered by their objectives).
template<class Solution, class Cost>
struct pareto_search_result
{
    /// The solution the algorithm returns: the best one found, for the built-in
    /// algorithms.
    Solution solution;
    /// Its cost.
    Cost cost;
    /// Solutions and moves evaluated, the initial evaluation included.
    std::size_t evaluations{};
    /// Iterations, as the algorithm counts them.
    std::size_t iterations{};
    /// Why the run ended.
    termination_reason termination{};
    /// The non-dominated solutions reached, ordered by their objectives.
    std::vector<pareto_point<Solution, Cost>> front;
};

namespace detail
{

// The archive of a search whose cost is totally ordered: nothing.
struct no_archive
{
};

} // namespace detail

/// The target of run options without a target cost.
struct no_target
{
};

namespace detail
{

// A time limit as the steady clock counts it; a limit beyond what it can count
// is the largest it can. Throws std::invalid_argument for a negative limit, or
// a floating-point one that is not a number.
template<class Rep, class Period>
[[nodiscard]]
std::chrono::steady_clock::duration steady_time_limit(
    const std::chrono::duration<Rep, Period> limit)
{
    using steady_duration = std::chrono::steady_clock::duration;
    if constexpr (std::is_floating_point_v<Rep>)
    {
        if (std::isnan(limit.count()))
            throw std::invalid_argument{"a time limit must be a number"};
    }
    if (limit < std::chrono::duration<Rep, Period>::zero())
        throw std::invalid_argument{"a time limit cannot be negative"};
    if (std::chrono::duration<double>{limit}
        >= std::chrono::duration<double>{(steady_duration::max)()})
    {
        return (steady_duration::max)();
    }
    return std::chrono::duration_cast<steady_duration>(limit);
}

// A time limit in seconds. Throws std::invalid_argument when the number is
// negative or not finite.
[[nodiscard]]
inline std::chrono::steady_clock::duration steady_time_limit(const double seconds)
{
    if (!std::isfinite(seconds))
        throw std::invalid_argument{"a time limit must be a finite number of seconds"};
    return steady_time_limit(std::chrono::duration<double>{seconds});
}

// The moment a limit starting now ends; none when the clock cannot count that
// far.
[[nodiscard]]
inline std::optional<std::chrono::steady_clock::time_point> deadline_after(
    const std::chrono::steady_clock::duration limit)
{
    const auto now = std::chrono::steady_clock::now();
    if (limit > (std::chrono::steady_clock::time_point::max)() - now)
        return std::nullopt;
    return now + limit;
}

} // namespace detail

/// Caller-side run options: optional cancellation/progress control, an optional
/// semantic tracer and an optional target cost, passed as the trailing argument
/// of Runner::run() and Solver::solve().
///
/// A run stops, with termination_reason::target_reached, as soon as its best
/// cost is at least as good as the target.
template<class Tracer, class Target = no_target>
struct run_options
{
    /// The type of the tracer.
    using tracer_type = Tracer;
    /// The type of the target cost, or no_target.
    using target_type = Target;

    /// Cancellation and progress, or nullptr.
    const run_control* control{};
    /// The tracer of the run's events, or nullptr.
    Tracer* tracer{};
    /// The cost at which the run stops, if any.
    std::optional<Target> target{};
    /// The time after which the run stops, if any, counted from its start.
    std::optional<std::chrono::steady_clock::duration> time_limit{};
    /// The evaluations the run may make, the initial one included; unlimited
    /// by default. The runner's own budget, if smaller, still applies.
    limit evaluation_limit{unlimited};
    /// What the archive of a run with a cost::pareto cost keeps (default one
    /// point per non-dominated cost, unbounded).
    pareto_archive_parameters front{};

    /// The same options with a target cost: with(control).stop_at(0).
    template<class Cost>
    [[nodiscard]]
    run_options<Tracer, Cost> stop_at(Cost cost) const
    {
        return {
            .control = control,
            .tracer = tracer,
            .target = std::move(cost),
            .time_limit = time_limit,
            .evaluation_limit = evaluation_limit,
            .front = front,
        };
    }

    /// The same options without a target cost, as a pipeline gives a stage
    /// that is not the last.
    [[nodiscard]]
    run_options<Tracer> without_target() const noexcept
    {
        return {
            .control = control,
            .tracer = tracer,
            .target = std::nullopt,
            .time_limit = time_limit,
            .evaluation_limit = evaluation_limit,
            .front = front,
        };
    }

    /// The same options with a time limit: with(control).timeout(5s).
    ///
    /// Throws `std::invalid_argument` when the limit is negative or not a
    /// number.
    template<class Rep, class Period>
    [[nodiscard]]
    run_options timeout(const std::chrono::duration<Rep, Period> limit) const
    {
        auto options = *this;
        options.time_limit = detail::steady_time_limit(limit);
        return options;
    }

    /// The same options with an evaluation budget:
    /// with(control).max_evaluations(10000).
    ///
    /// The run stops, with termination_reason::evaluation_budget_exhausted,
    /// once it has made `count` evaluations, the initial one included.
    [[nodiscard]]
    run_options max_evaluations(const limit count) const
    {
        auto options = *this;
        options.evaluation_limit = count;
        return options;
    }

    /// The same options with the parameters of the Pareto archive:
    /// with(control).keep_front({.keep_equivalent = true, .max_front_size = 100}).
    ///
    /// They matter only to a run with a cost::pareto cost.
    [[nodiscard]]
    run_options keep_front(const pareto_archive_parameters parameters) const
    {
        auto options = *this;
        options.front = parameters;
        return options;
    }

    /// The same options with a time limit in seconds:
    /// with(control).timeout(2.5).
    ///
    /// Throws `std::invalid_argument` when the number is negative or not
    /// finite.
    [[nodiscard]]
    run_options timeout(const double seconds) const
    {
        auto options = *this;
        options.time_limit = detail::steady_time_limit(seconds);
        return options;
    }
};

/// Run options with a run_control: run(solution, with(control)).
[[nodiscard]]
inline run_options<trace::null_tracer> with(const run_control& control) noexcept
{
    return {.control = &control, .tracer = nullptr};
}

/// Deleted: the options would refer to a temporary run_control.
run_options<trace::null_tracer> with(const run_control&&) = delete;

/// Run options with a tracer: run(solution, with(tracer)).
template<class Tracer>
    requires(!std::same_as<std::remove_cvref_t<Tracer>, run_control>)
[[nodiscard]]
run_options<Tracer> with(Tracer& tracer) noexcept
{
    return {.control = nullptr, .tracer = &tracer};
}

/// Run options with a run_control and a tracer.
template<class Tracer>
[[nodiscard]]
run_options<Tracer> with(const run_control& control, Tracer& tracer) noexcept
{
    return {.control = &control, .tracer = &tracer};
}

/// Deleted: the options would refer to a temporary run_control.
template<class Tracer>
run_options<Tracer> with(const run_control&&, Tracer&) = delete;

/// Run options with only a target cost: run(solution, easylocal::stop_at(0)).
template<class Cost>
[[nodiscard]]
run_options<trace::null_tracer, Cost> stop_at(Cost cost)
{
    return {.control = nullptr, .tracer = nullptr, .target = std::move(cost)};
}

/// Run options with only a time limit: run(solution, easylocal::timeout(5s)).
///
/// The run stops, with termination_reason::time_limit_reached, once `limit`
/// has passed since it started. Throws `std::invalid_argument` when the limit
/// is negative or not a number.
template<class Rep, class Period>
[[nodiscard]]
run_options<trace::null_tracer> timeout(const std::chrono::duration<Rep, Period> limit)
{
    return run_options<trace::null_tracer>{}.timeout(limit);
}

/// Run options with only an evaluation budget:
/// easylocal::max_evaluations(10000).
///
/// The run stops, with termination_reason::evaluation_budget_exhausted, once it
/// has made `count` evaluations, the initial one included; a runner's own
/// budget, if smaller, still applies.
[[nodiscard]]
inline run_options<trace::null_tracer> max_evaluations(const limit count)
{
    return run_options<trace::null_tracer>{}.max_evaluations(count);
}

/// Run options with only a time limit in seconds: easylocal::timeout(2.5).
///
/// Throws `std::invalid_argument` when the number is negative or not finite.
[[nodiscard]]
inline run_options<trace::null_tracer> timeout(const double seconds)
{
    return run_options<trace::null_tracer>{}.timeout(seconds);
}

/// One execution of a search algorithm. search_run exposes the search context
/// (neighborhood, evaluation, cost semantics) and owns everything that is
/// common to every search: evaluation/iteration counters, the evaluation
/// budget, cancellation, progress reporting and the core trace events.
///
/// Algorithms describe only their search logic in terms of these primitives.
/// With a cost::pareto cost it also keeps the archive of the non-dominated
/// solutions reached (start, evaluate_solution and commit offer them), and the
/// result carries that front. Evaluation is the facility that evaluates the
/// solutions and the moves: the context's, or the one with_evaluation() wraps
/// around it.
template<
    class Context,
    class Tracer = trace::null_tracer,
    class Evaluation = runners::detail::context_evaluation_type<Context>>
class search_run
{
    using evaluation_facility_type = Evaluation;

    static_assert(
        runners::detail::evaluation_facility_for<Evaluation, Context>,
        "the evaluation facility of a run must evaluate the context's solutions and "
        "moves, with its cost");

    // A search_run over the same context with another evaluation facility,
    // which with_evaluation() builds.
    template<class, class, class>
    friend class search_run;

public:
    /// The search context the bound runner provides.
    using context_type = Context;
    /// The type of the tracer.
    using tracer_type = Tracer;
    /// The type of the solutions.
    using solution_type = typename Context::solution_type;
    /// The type of the costs.
    using cost_type = typename Context::cost_type;
    /// The type of the neighborhood explorer.
    using neighborhood_explorer_type = typename Context::neighborhood_explorer_type;
    /// The type of the moves.
    using move_type = typename neighborhood_explorer_type::move_type;
    /// The evaluation of a solution: its cost, and what a delta evaluation
    /// keeps.
    using evaluation_type = typename evaluation_facility_type::evaluation_type;
    /// The evaluation of a move, which commit() applies.
    using candidate_type = typename evaluation_facility_type::candidate_type;
    /// Whether the run keeps a front: with a cost::pareto cost.
    static constexpr bool archives_front = cost::pareto_type<cost_type>;
    /// What finish() returns: pareto_search_result with a front, search_result
    /// otherwise.
    using result_type = std::conditional_t<
        archives_front,
        pareto_search_result<solution_type, cost_type>,
        search_result<solution_type, cost_type>>;

    /// A run of context, controlled by control and traced by tracer, with an
    /// evaluation limit (unlimited: none), a target cost (nullptr: none), a
    /// deadline (none: no time limit) and the parameters of its archive (with a
    /// cost::pareto cost).
    ///
    /// The bound runner makes it.
    search_run(
        const Context& context,
        const run_control& control,
        Tracer& tracer,
        const limit evaluation_limit = unlimited,
        const cost_type* target = nullptr,
        const std::optional<std::chrono::steady_clock::time_point> deadline =
            std::nullopt,
        const pareto_archive_parameters front = {})
        requires std::same_as<
            Evaluation,
            runners::detail::context_evaluation_type<Context>>
        : search_run{
              context,
              context.evaluation(),
              control,
              tracer,
              evaluation_limit,
              target,
              deadline,
              front}
    {
    }

    /// Deleted: the run would refer to a temporary context.
    search_run(
        const Context&&,
        const run_control&,
        Tracer&,
        limit = unlimited,
        const cost_type* = nullptr,
        std::optional<std::chrono::steady_clock::time_point> = std::nullopt,
        pareto_archive_parameters = {}) = delete;

    /// Not copyable: the run of one search.
    search_run(const search_run&) = delete;
    /// Not copyable: the run of one search.
    search_run& operator=(const search_run&) = delete;

private:
    search_run(
        const Context& context,
        Evaluation evaluation,
        const run_control& control,
        Tracer& tracer,
        const limit evaluation_limit,
        const cost_type* target,
        const std::optional<std::chrono::steady_clock::time_point> deadline,
        const pareto_archive_parameters front)
        : context_{context},
          evaluation_{std::move(evaluation)},
          control_{control},
          tracer_{tracer},
          caller_evaluation_limit_{evaluation_limit},
          evaluation_limit_{evaluation_limit},
          target_{target},
          deadline_{deadline},
          archive_{make_archive(front)}
    {
    }

    // The archive of the run: with a cost::pareto cost, one with the
    // parameters; otherwise nothing.
    [[nodiscard]]
    static auto make_archive(const pareto_archive_parameters front)
    {
        if constexpr (archives_front)
            return pareto_archive<solution_type, cost_type>{front};
        else
        {
            static_cast<void>(front);
            return detail::no_archive{};
        }
    }

public:
    // Context access.

    /// The search context.
    [[nodiscard]]
    const Context& context() const noexcept
    {
        return context_;
    }

    /// The neighborhood explorer.
    [[nodiscard]]
    const neighborhood_explorer_type& neighborhood_explorer() const noexcept
    {
        return context_.neighborhood_explorer();
    }

    /// Raw evaluation facility, the one the run evaluates with: bypasses
    /// counters, budget and trace events.
    [[nodiscard]]
    Evaluation evaluation() const
    {
        return evaluation_;
    }

    /// The Input, when the context has one.
    [[nodiscard]]
    decltype(auto) input() const noexcept
        requires requires(const Context& context) { context.input(); }
    {
        return context_.input();
    }

    /// The SolutionManager, when the context has one.
    [[nodiscard]]
    decltype(auto) solution_manager() const noexcept
        requires requires(const Context& context) { context.solution_manager(); }
    {
        return context_.solution_manager();
    }

    /// Whether candidate is strictly better than reference, by the semantics
    /// of the cost.
    [[nodiscard]]
    constexpr bool better(const cost_type& candidate, const cost_type& reference) const
        requires requires(const Context& context) {
            { context.better(candidate, reference) } -> std::convertible_to<bool>;
        }
    {
        return context_.better(candidate, reference);
    }

    /// Whether lhs and rhs are equivalent, by the semantics of the cost.
    [[nodiscard]]
    constexpr bool equivalent(const cost_type& lhs, const cost_type& rhs) const
        requires requires(const Context& context) {
            { context.equivalent(lhs, rhs) } -> std::convertible_to<bool>;
        }
    {
        return context_.equivalent(lhs, rhs);
    }

    /// Whether candidate is better than or equivalent to reference, by the
    /// semantics of the cost.
    [[nodiscard]]
    constexpr bool better_or_equivalent(
        const cost_type& candidate,
        const cost_type& reference) const
        requires requires(const Context& context) {
            {
                context.better_or_equivalent(candidate, reference)
            } -> std::convertible_to<bool>;
        }
    {
        return context_.better_or_equivalent(candidate, reference);
    }

    // Run state.

    /// Solutions and moves evaluated so far, the initial evaluation included.
    [[nodiscard]]
    std::size_t evaluations() const noexcept
    {
        return evaluations_;
    }

    /// Iterations so far, counted by next_iteration().
    [[nodiscard]]
    std::size_t iterations() const noexcept
    {
        return iterations_;
    }

    /// The tracer of the run.
    [[nodiscard]]
    Tracer& tracer() noexcept
    {
        return tracer_;
    }

    /// The run_control of the caller.
    [[nodiscard]]
    const run_control& control() const noexcept
    {
        return control_;
    }

    /// The runner's own budget; the initial evaluation counts towards it. The
    /// caller's budget, if smaller, stays.
    void limit_evaluations(const limit max_evaluations) noexcept
    {
        evaluation_limit_ =
            std::min<std::size_t>(max_evaluations, caller_evaluation_limit_);
    }

    /// The target cost given by the caller, or nullptr.
    [[nodiscard]]
    const cost_type* target() const noexcept
    {
        return target_;
    }

    /// Sends event to the tracer, if it observes events of that type.
    template<class Event>
    void emit(const Event& event)
    {
        trace::emit(tracer_, event);
    }

    // Search primitives.

    /// Starts the run from solution: evaluates it, resets the counters, and
    /// returns its evaluation.
    ///
    /// An algorithm calls it first.
    [[nodiscard]]
    evaluation_type start(const solution_type& solution)
    {
        auto current = evaluation_.evaluate(solution);
        evaluations_ = 1;
        iterations_ = 0;
        stop_reason_ = termination_reason::completed;
        target_reached_ = false;
        observe_cost(current.cost());

        emit(trace::event::run_started<cost_type>{current.cost()});
        visited(solution, current.cost());
        if constexpr (archives_front)
        {
            archive_.clear();
            archive(solution, current.cost());
        }
        report();
        return current;
    }

    /// Evaluates another solution than the one the run started from, for
    /// algorithms that keep several (a population, a history of solutions):
    /// it counts as an evaluation, is traced as visited and enters the archive.
    [[nodiscard]]
    evaluation_type evaluate_solution(const solution_type& solution)
    {
        auto evaluation = evaluation_.evaluate(solution);
        ++evaluations_;
        observe_cost(evaluation.cost());
        visited(solution, evaluation.cost());
        if constexpr (archives_front)
            archive(solution, evaluation.cost());
        report();
        return evaluation;
    }

    /// The non-dominated solutions reached so far.
    [[nodiscard]]
    const pareto_archive<solution_type, cost_type>& front() const noexcept
        requires archives_front
    {
        return archive_;
    }

    /// True when the run must end: external cancellation, a reached target
    /// cost, an exhausted evaluation budget or a passed time limit.
    ///
    /// The reason is recorded for finish().
    [[nodiscard]]
    bool should_stop()
    {
        if (control_.stop_requested())
        {
            stop_reason_ = termination_reason::cancelled;
            return true;
        }

        if (target_reached())
        {
            stop_reason_ = termination_reason::target_reached;
            return true;
        }

        if (evaluations_ >= evaluation_limit_)
        {
            stop_reason_ = termination_reason::evaluation_budget_exhausted;
            return true;
        }

        if (time_is_up())
        {
            stop_reason_ = termination_reason::time_limit_reached;
            return true;
        }

        return false;
    }

    /// Counts an iteration.
    void next_iteration() noexcept
    {
        ++iterations_;
    }

    /// The moves of the neighborhood of solution, for the algorithms that
    /// enumerate them.
    [[nodiscard]]
    auto moves(const solution_type& solution) const
    {
        return easylocal::moves(context_.neighborhood_explorer(), solution);
    }

    /// A random move of the neighborhood of solution, or nothing when there is
    /// none.
    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto random_move(const solution_type& solution, RNG& rng)
    {
        const auto& neighborhood = context_.neighborhood_explorer();
        auto observer = [this](const trace::event::neighborhood_selection& value) {
            emit(value);
        };

        if constexpr (
            trace::observes<Tracer, trace::event::neighborhood_selection> &&
            requires { neighborhood.random_move_traced(solution, rng, observer); })
        {
            return neighborhood.random_move_traced(solution, rng, observer);
        }
        else
        {
            return easylocal::random_move(neighborhood, solution, rng);
        }
    }

    /// Evaluates move on solution, whose evaluation is current; counts as an
    /// evaluation.
    ///
    /// Check should_stop() before.
    [[nodiscard]]
    candidate_type evaluate_move(
        const solution_type& solution,
        const evaluation_type& current,
        const move_type& move)
    {
        auto candidate = evaluation_.evaluate_move(solution, current, move);
        ++evaluations_;

        // Without a tracer of the event, no route is built (a std::visit for
        // a union's move) and no event.
        if constexpr (trace::observes<Tracer, trace::event::move_evaluated<cost_type>>)
        {
            trace::with_move_route(move, [&](const auto* route) {
                emit(
                    trace::event::move_evaluated<cost_type>{
                        .evaluations = evaluations_,
                        .iterations = iterations_,
                        .current_cost = current.cost(),
                        .candidate_cost = candidate.cost(),
                        .neighborhood = route,
                    });
            });
        }
        report();
        return candidate;
    }

    /// Applies move, evaluated as candidate, to solution and current.
    void commit(
        solution_type& solution,
        evaluation_type& current,
        candidate_type&& candidate,
        const move_type& move)
    {
        constexpr bool traced =
            trace::observes<Tracer, trace::event::move_accepted<cost_type>>;
        // The cost before the move is copied only for a tracer of the event.
        std::optional<cost_type> previous_cost;
        if constexpr (traced)
            previous_cost.emplace(current.cost());
        evaluation_.commit(solution, current, std::move(candidate));
        evaluate_near_target(solution, current);
        observe_cost(current.cost());

        if constexpr (traced)
        {
            trace::with_move_route(move, [&](const auto* route) {
                emit(
                    trace::event::move_accepted<cost_type>{
                        .evaluations = evaluations_,
                        .iterations = iterations_,
                        .previous_cost = *previous_cost,
                        .cost = current.cost(),
                        .neighborhood = route,
                    });
            });
        }
        visited(solution, current.cost());
        if constexpr (archives_front)
            archive(solution, current.cost());
    }

    /// Reports a new best cost, for the trace and the target cost; best_so_far
    /// calls it.
    void incumbent_updated(
        const cost_type& previous_cost,
        const cost_type& cost)
    {
        observe_cost(cost);
        if constexpr (trace::observes<Tracer, trace::event::incumbent_updated<cost_type>>)
        {
            emit(
                trace::event::incumbent_updated<cost_type>{
                    .evaluations = evaluations_,
                    .iterations = iterations_,
                    .previous_cost = previous_cost,
                    .cost = cost,
                });
        }
    }

    /// Ends the run.
    ///
    /// Without an explicit reason the termination is the one recorded by
    /// should_stop(), or completed.
    [[nodiscard]]
    result_type finish(solution_type solution, cost_type cost)
    {
        return finish(
            std::move(solution),
            std::move(cost),
            stop_reason_);
    }

    /// Ends the run with an explicit reason, such as
    /// termination_reason::local_optimum.
    [[nodiscard]]
    result_type finish(
        solution_type solution,
        cost_type cost,
        const termination_reason reason)
    {
        if (reason == termination_reason::local_optimum)
        {
            emit(trace::event::local_optimum<cost_type>{
                .evaluations = evaluations_,
                .iterations = iterations_,
                .cost = cost,
            });
        }
        // With a partial order the solution returned (the first of a front)
        // may not meet a target that another one reached: the front holds a
        // solution that does, which the run returns instead.
        if constexpr (archives_front)
        {
            if (reason != termination_reason::cancelled && target_reached()
                && !meets_target(cost))
            {
                for (auto& point : archive_.sorted())
                {
                    if (meets_target(point.cost))
                    {
                        solution = std::move(point.solution);
                        cost = std::move(point.cost);
                        break;
                    }
                }
            }
        }
        // A reached target is the reason a run ends, unless it was cancelled,
        // also when it coincides with a local optimum or the end of the
        // algorithm.
        const auto termination =
            reason != termination_reason::cancelled && target_reached()
                ? termination_reason::target_reached
                : reason;
        emit(
            trace::event::run_finished<cost_type>{
                .evaluations = evaluations_,
                .iterations = iterations_,
                .cost = cost,
                .termination = termination,
            });

        if constexpr (archives_front)
        {
            return result_type{
                .solution = std::move(solution),
                .cost = std::move(cost),
                .evaluations = evaluations_,
                .iterations = iterations_,
                .termination = termination,
                .front = archive_.sorted(),
            };
        }
        else
        {
            return result_type{
                .solution = std::move(solution),
                .cost = std::move(cost),
                .evaluations = evaluations_,
                .iterations = iterations_,
                .termination = termination,
            };
        }
    }

    /// A run that evaluates through wrap(evaluation()), a facility that
    /// decorates this run's (a delay, a cache, a count), and shares everything
    /// else: the context and its cost relations, the control, the tracer, the
    /// deadline, the target and what is left of the evaluation budget.
    ///
    /// The new run counts its own effort from its start(): an algorithm calls
    /// it instead of starting this run, and returns the new run's result. The
    /// facility has evaluate(solution), evaluate_move(solution, current, move)
    /// and commit(solution, current, candidate), as the context's does.
    template<class Wrap>
        requires std::invocable<Wrap, const Evaluation&>
    [[nodiscard]]
    search_run<
        Context,
        Tracer,
        std::remove_cvref_t<
            std::invoke_result_t<Wrap, const Evaluation&>>> with_evaluation(Wrap&& wrap)
    {
        const auto left = evaluation_limit_.is_unlimited()
            ? unlimited
            : limit{
                  evaluation_limit_
                  - std::min<std::size_t>(evaluations_, evaluation_limit_)};
        return {
            context_,
            std::invoke(std::forward<Wrap>(wrap), std::as_const(evaluation_)),
            control_,
            tracer_,
            left,
            target_,
            deadline_,
            front_parameters(),
        };
    }

private:
    // Whether the deadline has passed. The clock is read at the first check,
    // then at an interval of checks that adapts so that readings come about a
    // millisecond apart: it doubles while they come sooner, halves while they
    // come later. A fast loop reads the clock rarely; a slow one, at every
    // check.
    [[nodiscard]]
    bool time_is_up()
    {
        if (!deadline_.has_value())
            return false;
        if (time_up_)
            return true;
        if (++checks_since_clock_ < clock_interval_)
            return false;
        checks_since_clock_ = 0;

        const auto now = std::chrono::steady_clock::now();
        if (clock_read_)
        {
            const auto gap = now - last_clock_reading_;
            if (gap < clock_spacing / 2 && clock_interval_ < max_clock_interval)
                clock_interval_ *= 2;
            else if (gap > clock_spacing * 2 && clock_interval_ > 1)
                clock_interval_ /= 2;
        }
        last_clock_reading_ = now;
        clock_read_ = true;
        time_up_ = now >= *deadline_;
        return time_up_;
    }

    // Offers a solution to the archive, compared with the cost relations of
    // the context; solutions of equivalent cost are the same when the problem
    // has solution equality and they are equal, or always without it.
    void archive(const solution_type& solution, const cost_type& cost)
    {
        archive_.offer(solution, cost, context_, detail::same_solution_of(context_));
    }

    // The parameters of the archive, for a run that shares this one's limits.
    [[nodiscard]]
    pareto_archive_parameters front_parameters() const noexcept
    {
        if constexpr (archives_front)
            return archive_.parameters();
        else
            return {};
    }

    // The solution_visited event, when the tracer observes it and the problem
    // has a solution hash.
    void visited(const solution_type& solution, const cost_type& cost)
    {
        if constexpr (trace::observes<Tracer, trace::event::solution_visited<cost_type>>
            && requires(const Context& context) {
                   requires has_solution_hash<
                       std::remove_cvref_t<decltype(context.solution_manager())>>;
               })
        {
            emit(
                trace::event::solution_visited<cost_type>{
                    .evaluations = evaluations_,
                    .iterations = iterations_,
                    .hash =
                        easylocal::solution_hash(context_.solution_manager(), solution),
                    .cost = cost,
                });
        }
    }

    [[nodiscard]]
    bool target_reached() const noexcept
    {
        return target_reached_;
    }

    // Whether cost is at least as good as the target.
    [[nodiscard]]
    bool meets_target(const cost_type& cost) const
    {
        if constexpr (requires(const Context& context, const cost_type& value) {
                          {
                              context.better_or_equivalent(value, value)
                          } -> std::convertible_to<bool>;
                      })
            return target_ != nullptr && context_.better_or_equivalent(cost, *target_);
        else
            return false;
    }

    // A cost updated by deltas drifts from its full evaluation by rounding
    // errors: one that misses the target only within the tolerance of the
    // checks (cost::tolerance{}) is evaluated in full, so that a target such
    // as the zero hard cost of until_feasible() is reached. The re-evaluation
    // corrects the bookkeeping of a move already counted, so the budget does
    // not count it.
    void evaluate_near_target(const solution_type& solution, evaluation_type& current)
    {
        if constexpr (cost::approximately_comparable<cost_type>
            && requires(const Context& context, const cost_type& value) {
                   {
                       context.better_or_equivalent(value, value)
                   } -> std::convertible_to<bool>;
               })
        {
            if (target_ != nullptr && !target_reached_ && !meets_target(current.cost())
                && cost::approximate_compare(current.cost(), *target_) <= 0)
            {
                current = evaluation_.evaluate(solution);
            }
        }
    }

    // The best cost of a run is at least as good as every cost it reaches, so
    // the target is reached once any of them is at least as good as it.
    void observe_cost(const cost_type& cost)
    {
        if constexpr (requires(const Context& context, const cost_type& value) {
                          { context.better_or_equivalent(value, value) } ->
                              std::convertible_to<bool>;
                      })
        {
            if (target_ != nullptr && !target_reached_ &&
                context_.better_or_equivalent(cost, *target_))
            {
                target_reached_ = true;
            }
        }
    }

    void report() const
    {
        control_.report(
            run_progress{
                .evaluations = evaluations_,
                .iterations = iterations_,
                .evaluation_limit = evaluation_limit_.is_unlimited()
                    ? std::nullopt
                    : std::optional<std::size_t>{evaluation_limit_},
            });
    }


    const Context& context_;
    evaluation_facility_type evaluation_;
    const run_control& control_;
    Tracer& tracer_;
    std::size_t evaluations_{};
    std::size_t iterations_{};
    // The caller's budget (run_options::max_evaluations), which the runner's
    // own can only tighten.
    limit caller_evaluation_limit_{unlimited};
    limit evaluation_limit_{unlimited};
    // Recorded by should_stop(); completed while the run goes on.
    termination_reason stop_reason_{termination_reason::completed};
    const cost_type* target_{};
    bool target_reached_{};
    static constexpr std::chrono::steady_clock::duration clock_spacing =
        std::chrono::milliseconds{1};
    static constexpr std::size_t max_clock_interval = std::size_t{1} << 20U;
    std::optional<std::chrono::steady_clock::time_point> deadline_;
    // The last reading of the clock, when clock_read_ (not an optional, which
    // GCC 15 at -O3 reports as maybe uninitialized).
    std::chrono::steady_clock::time_point last_clock_reading_{};
    bool clock_read_{false};
    std::size_t clock_interval_{1};
    std::size_t checks_since_clock_{};
    bool time_up_{};
    EASYLOCAL_NO_UNIQUE_ADDRESS std::conditional_t<
        archives_front,
        pareto_archive<solution_type, cost_type>,
        detail::no_archive>
        archive_;
};

namespace detail
{

// Whether candidate meets the target of run and best does not.
template<class Run, class Cost>
[[nodiscard]]
bool reaches_target(const Run& run, const Cost& candidate, const Cost& best)
{
    if constexpr (requires {
                      { run.target() } -> std::convertible_to<const Cost*>;
                      {
                          run.better_or_equivalent(candidate, candidate)
                      } -> std::convertible_to<bool>;
                  })
    {
        const Cost* target = run.target();
        return target != nullptr && run.better_or_equivalent(candidate, *target)
            && !run.better_or_equivalent(best, *target);
    }
    else
    {
        return false;
    }
}

} // namespace detail

/// The best solution of a run and its cost, for the algorithms that return the
/// best solution they visited rather than the last one:
///
/// \code
/// best_so_far best{solution, current.cost()};
/// ...
/// if (best.update(run, solution, current))
///     idle_iterations = 0;
/// ...
/// return run.finish(std::move(best.solution), std::move(best.cost));
/// \endcode
template<class Solution, class Cost>
struct best_so_far
{
    /// The best solution.
    Solution solution;
    /// Its cost.
    Cost cost;

    /// Keeps candidate when current, its evaluation, is better than the best,
    /// and reports the new best to the run (incumbent_updated); true when it
    /// does.
    ///
    /// It also keeps the first candidate that meets the run's target when the
    /// best does not: with a partial order (a cost::pareto cost) it may not be
    /// better, and the run ends at the target with it.
    template<class Run, class Evaluation>
    bool update(Run& run, const Solution& candidate, const Evaluation& current)
    {
        if (!run.better(current.cost(), cost)
            && !detail::reaches_target(run, current.cost(), cost))
            return false;
        const auto previous = cost;
        solution = candidate;
        cost = current.cost();
        run.incumbent_updated(previous, cost);
        return true;
    }
};

} // namespace easylocal
