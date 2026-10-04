#pragma once

// search_run: one execution of a search algorithm. Algorithms are written in
// terms of its primitives (evaluate, commit, random_move, ...), while it keeps
// the counters, the evaluation budget, cancellation, progress, the target cost
// and the trace events. Also run_options/with() for the caller's options,
// termination_reason and the result types.

#include <easylocal/cost/pareto.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/pareto_archive.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/trace/events.hpp>
#include <easylocal/trace/tracer.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <concepts>
#include <cstddef>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal
{

enum class termination_reason
{
    completed,
    local_optimum,
    evaluation_budget_exhausted,
    cancelled,
    target_reached,
    idle_limit_reached,
};

// A readable name of the reason, e.g. "evaluation budget exhausted".
[[nodiscard]]
constexpr std::string_view to_string(const termination_reason reason) noexcept
{
    switch (reason)
    {
    case termination_reason::completed:
        return "completed";
    case termination_reason::local_optimum:
        return "local optimum";
    case termination_reason::evaluation_budget_exhausted:
        return "evaluation budget exhausted";
    case termination_reason::cancelled:
        return "cancelled";
    case termination_reason::target_reached:
        return "target reached";
    case termination_reason::idle_limit_reached:
        return "idle limit reached";
    }
    return "unknown";
}

// The result contract consumed by solvers, runs by name (app.run) and the
// adapters: the final solution and its cost. search_result models it; custom runners may
// return richer types.
template<class Result, class Solution, class Cost>
concept search_result_for =
    requires(Result& result, const Result& const_result) {
        { std::move(result.solution) } -> std::convertible_to<Solution>;
        { const_result.cost } -> std::convertible_to<const Cost&>;
    };

template<class Solution, class Cost>
struct search_result
{
    Solution solution;
    Cost cost;
    std::size_t evaluations{};
    std::size_t iterations{};
    termination_reason termination{};
};

// The result of a search with a cost::pareto cost: a solution of the front
// with its cost, and the whole front (the non-dominated solutions reached,
// ordered by their objectives).
template<class Solution, class Cost>
struct pareto_search_result
{
    Solution solution;
    Cost cost;
    std::size_t evaluations{};
    std::size_t iterations{};
    termination_reason termination{};
    std::vector<pareto_point<Solution, Cost>> front;
};

namespace detail
{

// The archive of a search whose cost is totally ordered: nothing.
struct no_archive
{
};

} // namespace detail

// The target of run options without a target cost.
struct no_target
{
};

// Caller-side run options: optional cancellation/progress control, an
// optional semantic tracer and an optional target cost, passed as the trailing
// argument of Runner::run() and Solver::solve(). A run stops, with
// termination_reason::target_reached, as soon as its best cost is at least as
// good as the target.
template<class Tracer, class Target = no_target>
struct run_options
{
    using tracer_type = Tracer;
    using target_type = Target;

    const run_control* control{};
    Tracer* tracer{};
    std::optional<Target> target{};

    template<class Cost>
    [[nodiscard]]
    run_options<Tracer, Cost> stop_at(Cost cost) const
    {
        return {.control = control, .tracer = tracer, .target = std::move(cost)};
    }
};

[[nodiscard]]
inline run_options<trace::null_tracer> with(const run_control& control) noexcept
{
    return {.control = &control, .tracer = nullptr};
}

template<class Tracer>
    requires(!std::same_as<std::remove_cvref_t<Tracer>, run_control>)
[[nodiscard]]
run_options<Tracer> with(Tracer& tracer) noexcept
{
    return {.control = nullptr, .tracer = &tracer};
}

template<class Tracer>
[[nodiscard]]
run_options<Tracer> with(const run_control& control, Tracer& tracer) noexcept
{
    return {.control = &control, .tracer = &tracer};
}

// Run options with only a target cost: run(solution, easylocal::stop_at(0)).
template<class Cost>
[[nodiscard]]
run_options<trace::null_tracer, Cost> stop_at(Cost cost)
{
    return {.control = nullptr, .tracer = nullptr, .target = std::move(cost)};
}

// One execution of a search algorithm. search_run exposes the search context
// (neighborhood, evaluation, cost semantics) and owns everything that is common
// to every search: evaluation/iteration counters, the evaluation budget,
// cancellation, progress reporting and the core trace events. Algorithms
// describe only their search logic in terms of these primitives. With a
// cost::pareto cost it also keeps the archive of the non-dominated solutions
// reached (start, evaluate_solution and commit offer them), and the result
// carries that front.
template<class Context, class Tracer = trace::null_tracer>
class search_run
{
    using evaluation_facility_type = runners::detail::context_evaluation_type<Context>;

public:
    using context_type = Context;
    using tracer_type = Tracer;
    using solution_type = typename Context::solution_type;
    using cost_type = typename Context::cost_type;
    using neighborhood_explorer_type = typename Context::neighborhood_explorer_type;
    using move_type = typename neighborhood_explorer_type::move_type;
    using evaluation_type = typename evaluation_facility_type::evaluation_type;
    using candidate_type = typename evaluation_facility_type::candidate_type;
    static constexpr bool archives_front = cost::pareto_type<cost_type>;
    using result_type = std::conditional_t<
        archives_front,
        pareto_search_result<solution_type, cost_type>,
        search_result<solution_type, cost_type>>;

    static constexpr std::size_t no_evaluation_limit =
        std::numeric_limits<std::size_t>::max();

    search_run(
        const Context& context,
        const run_control& control,
        Tracer& tracer,
        const std::size_t evaluation_limit = no_evaluation_limit,
        const cost_type* target = nullptr)
        : context_{context},
          evaluation_{context.evaluation()},
          control_{control},
          tracer_{tracer},
          evaluation_limit_{evaluation_limit},
          target_{target}
    {
    }

    search_run(const search_run&) = delete;
    search_run& operator=(const search_run&) = delete;

    // Context access.

    [[nodiscard]]
    const Context& context() const noexcept
    {
        return context_;
    }

    [[nodiscard]]
    const neighborhood_explorer_type& neighborhood_explorer() const noexcept
    {
        return context_.neighborhood_explorer();
    }

    // Raw evaluation facility: bypasses counters, budget and trace events.
    [[nodiscard]]
    auto evaluation() const
    {
        return context_.evaluation();
    }

    [[nodiscard]]
    decltype(auto) input() const noexcept
        requires requires(const Context& context) { context.input(); }
    {
        return context_.input();
    }

    [[nodiscard]]
    decltype(auto) solution_manager() const noexcept
        requires requires(const Context& context) { context.solution_manager(); }
    {
        return context_.solution_manager();
    }

    [[nodiscard]]
    constexpr bool better(const cost_type& candidate, const cost_type& reference) const
        requires requires(const Context& context) {
            { context.better(candidate, reference) } -> std::convertible_to<bool>;
        }
    {
        return context_.better(candidate, reference);
    }

    [[nodiscard]]
    constexpr bool equivalent(const cost_type& lhs, const cost_type& rhs) const
        requires requires(const Context& context) {
            { context.equivalent(lhs, rhs) } -> std::convertible_to<bool>;
        }
    {
        return context_.equivalent(lhs, rhs);
    }

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

    [[nodiscard]]
    std::size_t evaluations() const noexcept
    {
        return evaluations_;
    }

    [[nodiscard]]
    std::size_t iterations() const noexcept
    {
        return iterations_;
    }

    [[nodiscard]]
    Tracer& tracer() noexcept
    {
        return tracer_;
    }

    [[nodiscard]]
    const run_control& control() const noexcept
    {
        return control_;
    }

    // The initial evaluation counts towards the budget.
    void limit_evaluations(const std::size_t max_evaluations) noexcept
    {
        evaluation_limit_ = max_evaluations;
    }

    // The target cost given by the caller, or nullptr.
    [[nodiscard]]
    const cost_type* target() const noexcept
    {
        return target_;
    }

    template<class Event>
    void emit(const Event& event)
    {
        trace::emit(tracer_, event);
    }

    // Search primitives.

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

    // Evaluates another solution than the one the run started from, for
    // algorithms that keep several (a population, a history of solutions):
    // it counts as an evaluation, is traced as visited and enters the archive.
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

    // The non-dominated solutions reached so far.
    [[nodiscard]]
    const pareto_archive<solution_type, cost_type>& front() const noexcept
        requires archives_front
    {
        return archive_;
    }

    // True when the run must end: external cancellation, a reached target
    // cost or an exhausted evaluation budget. The reason is recorded for
    // finish().
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

        return false;
    }

    void next_iteration() noexcept
    {
        ++iterations_;
    }

    [[nodiscard]]
    auto moves(const solution_type& solution) const
    {
        return easylocal::moves(context_.neighborhood_explorer(), solution);
    }

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

    [[nodiscard]]
    candidate_type evaluate_move(
        const solution_type& solution,
        const evaluation_type& current,
        const move_type& move)
    {
        auto candidate = evaluation_.evaluate_move(solution, current, move);
        ++evaluations_;

        trace::with_move_route(move, [&](const auto* route) {
            emit(trace::event::move_evaluated<cost_type>{
                .evaluations = evaluations_,
                .iterations = iterations_,
                .current_cost = current.cost(),
                .candidate_cost = candidate.cost(),
                .neighborhood = route,
            });
        });
        report();
        return candidate;
    }

    void commit(
        solution_type& solution,
        evaluation_type& current,
        candidate_type&& candidate,
        const move_type& move)
    {
        const auto previous_cost = current.cost();
        evaluation_.commit(solution, current, std::move(candidate));
        observe_cost(current.cost());

        trace::with_move_route(move, [&](const auto* route) {
            emit(trace::event::move_accepted<cost_type>{
                .evaluations = evaluations_,
                .iterations = iterations_,
                .previous_cost = previous_cost,
                .cost = current.cost(),
                .neighborhood = route,
            });
        });
        visited(solution, current.cost());
        if constexpr (archives_front)
            archive(solution, current.cost());
    }

    void incumbent_updated(
        const cost_type& previous_cost,
        const cost_type& cost)
    {
        observe_cost(cost);
        emit(trace::event::incumbent_updated<cost_type>{
            .evaluations = evaluations_,
            .iterations = iterations_,
            .previous_cost = previous_cost,
            .cost = cost,
        });
    }

    // Ends the run. Without an explicit reason the termination is the one
    // recorded by should_stop(), or completed.
    [[nodiscard]]
    result_type finish(solution_type solution, cost_type cost)
    {
        return finish(
            std::move(solution),
            std::move(cost),
            stop_reason_);
    }

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
        emit(trace::event::run_finished<cost_type>{
            .evaluations = evaluations_,
            .iterations = iterations_,
            .cost = cost,
        });

        // A reached target is the reason a run ends, unless it was cancelled,
        // also when it coincides with a local optimum or the end of the
        // algorithm.
        const auto termination =
            reason != termination_reason::cancelled && target_reached()
                ? termination_reason::target_reached
                : reason;

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

    // A run over a decorated context sharing this run's control, tracer,
    // budget and (for the same cost type) target, e.g. for algorithms that
    // delegate to another algorithm.
    template<class OtherContext>
    [[nodiscard]]
    search_run<OtherContext, Tracer> with_context(const OtherContext& context)
    {
        const typename OtherContext::cost_type* target = nullptr;
        if constexpr (std::same_as<typename OtherContext::cost_type, cost_type>)
        {
            target = target_;
        }
        return search_run<OtherContext, Tracer>{
            context,
            control_,
            tracer_,
            evaluation_limit_,
            target,
        };
    }

private:
    // Offers a solution to the archive; solutions of equal cost are the same
    // when the problem has solution equality and they are equal, or always
    // without it.
    void archive(const solution_type& solution, const cost_type& cost)
    {
        archive_.offer(
            solution,
            cost,
            [this](const solution_type& lhs, const solution_type& rhs) {
                if constexpr (requires(const Context& context) {
                                  requires has_solution_equality<std::remove_cvref_t<
                                      decltype(context.solution_manager())>>;
                              })
                    return easylocal::solutions_equal(
                        context_.solution_manager(),
                        lhs,
                        rhs);
                else
                    return true;
            });
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
                .evaluation_limit = evaluation_limit_ == no_evaluation_limit
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
    std::size_t evaluation_limit_{no_evaluation_limit};
    // Recorded by should_stop(); completed while the run goes on.
    termination_reason stop_reason_{termination_reason::completed};
    const cost_type* target_{};
    bool target_reached_{};
    EASYLOCAL_NO_UNIQUE_ADDRESS std::conditional_t<
        archives_front,
        pareto_archive<solution_type, cost_type>,
        detail::no_archive>
        archive_{};
};

// The best solution of a run and its cost, for the algorithms that return the
// best solution they visited rather than the last one:
//
//     best_so_far best{solution, current.cost()};
//     ...
//     if (best.update(run, solution, current))
//         idle_iterations = 0;
//     ...
//     return run.finish(std::move(best.solution), std::move(best.cost));
template<class Solution, class Cost>
struct best_so_far
{
    Solution solution;
    Cost cost;

    // Keeps candidate when current, its evaluation, is better than the best,
    // and reports the new best to the run (incumbent_updated); true when it
    // does.
    template<class Run, class Evaluation>
    bool update(Run& run, const Solution& candidate, const Evaluation& current)
    {
        if (!run.better(current.cost(), cost))
            return false;
        const auto previous = cost;
        solution = candidate;
        cost = current.cost();
        run.incumbent_updated(previous, cost);
        return true;
    }
};

} // namespace easylocal
