#pragma once

#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/trace/events.hpp>
#include <easylocal/trace/tracer.hpp>

#include <concepts>
#include <cstddef>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace easylocal
{

enum class termination_reason
{
    completed,
    local_optimum,
    evaluation_budget_exhausted,
    cancelled,
};

// The result contract consumed by solvers, the Tester and the adapters: the
// final solution and its cost. search_result models it; custom runners may
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

// Caller-side run options: optional cancellation/progress control and an
// optional semantic tracer, passed as the trailing argument of Runner::run().
template<class Tracer>
struct run_options
{
    using tracer_type = Tracer;

    const run_control* control{};
    Tracer* tracer{};
};

[[nodiscard]]
inline auto with(const run_control& control) noexcept
    -> run_options<trace::null_tracer>
{
    return {.control = &control, .tracer = nullptr};
}

template<class Tracer>
    requires (!std::same_as<std::remove_cvref_t<Tracer>, run_control>)
[[nodiscard]]
auto with(Tracer& tracer) noexcept -> run_options<Tracer>
{
    return {.control = nullptr, .tracer = &tracer};
}

template<class Tracer>
[[nodiscard]]
auto with(const run_control& control, Tracer& tracer) noexcept
    -> run_options<Tracer>
{
    return {.control = &control, .tracer = &tracer};
}

// One execution of a search algorithm. search_run exposes the search context
// (neighborhood, evaluation, cost semantics) and owns everything that is common
// to every search: evaluation/iteration counters, the evaluation budget,
// cancellation, progress reporting and the core trace events. Algorithms
// describe only their search logic in terms of these primitives.
template<class Context, class Tracer = trace::null_tracer>
class search_run
{
    using evaluation_facility_type =
        std::remove_cvref_t<decltype(std::declval<const Context&>().evaluation())>;

public:
    using context_type = Context;
    using tracer_type = Tracer;
    using solution_type = typename Context::solution_type;
    using cost_type = typename Context::cost_type;
    using neighborhood_explorer_type = typename Context::neighborhood_explorer_type;
    using move_type = typename neighborhood_explorer_type::move_type;
    using evaluation_type = typename evaluation_facility_type::evaluation_type;
    using candidate_type = typename evaluation_facility_type::candidate_type;
    using result_type = search_result<solution_type, cost_type>;

    search_run(
        const Context& context,
        const run_control& control,
        Tracer& tracer,
        const std::size_t evaluation_limit = no_limit)
        : context_{context},
          evaluation_{context.evaluation()},
          control_{control},
          tracer_{tracer},
          evaluation_limit_{evaluation_limit}
    {
    }

    search_run(const search_run&) = delete;
    auto operator=(const search_run&) -> search_run& = delete;

    // Context access.

    [[nodiscard]]
    auto context() const noexcept -> const Context&
    {
        return context_;
    }

    [[nodiscard]]
    auto neighborhood_explorer() const noexcept
        -> const neighborhood_explorer_type&
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
    auto input() const noexcept -> decltype(auto)
        requires requires(const Context& context) { context.input(); }
    {
        return context_.input();
    }

    [[nodiscard]]
    auto solution_manager() const noexcept -> decltype(auto)
        requires requires(const Context& context) { context.solution_manager(); }
    {
        return context_.solution_manager();
    }

    [[nodiscard]]
    constexpr auto better(
        const cost_type& candidate,
        const cost_type& reference) const -> bool
        requires requires(const Context& context) {
            { context.better(candidate, reference) } -> std::convertible_to<bool>;
        }
    {
        return context_.better(candidate, reference);
    }

    [[nodiscard]]
    constexpr auto equivalent(
        const cost_type& lhs,
        const cost_type& rhs) const -> bool
        requires requires(const Context& context) {
            { context.equivalent(lhs, rhs) } -> std::convertible_to<bool>;
        }
    {
        return context_.equivalent(lhs, rhs);
    }

    [[nodiscard]]
    constexpr auto better_or_equivalent(
        const cost_type& candidate,
        const cost_type& reference) const -> bool
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
    auto evaluations() const noexcept -> std::size_t
    {
        return evaluations_;
    }

    [[nodiscard]]
    auto iterations() const noexcept -> std::size_t
    {
        return iterations_;
    }

    [[nodiscard]]
    auto tracer() noexcept -> Tracer&
    {
        return tracer_;
    }

    [[nodiscard]]
    auto control() const noexcept -> const run_control&
    {
        return control_;
    }

    // The initial evaluation counts towards the budget.
    void limit_evaluations(const std::size_t max_evaluations) noexcept
    {
        evaluation_limit_ = max_evaluations;
    }

    template<class Event>
    void emit(const Event& event)
    {
        trace::emit(tracer_, event);
    }

    // Search primitives.

    [[nodiscard]]
    auto start(const solution_type& solution) -> evaluation_type
    {
        auto current = evaluation_.evaluate(solution);
        evaluations_ = 1;
        iterations_ = 0;
        stop_reason_.reset();

        emit(trace::event::run_started<cost_type>{current.cost()});
        report();
        return current;
    }

    // True when the run must end: external cancellation or exhausted
    // evaluation budget. The reason is recorded for finish().
    [[nodiscard]]
    auto should_stop() -> bool
    {
        if (control_.stop_requested())
        {
            stop_reason_ = termination_reason::cancelled;
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
    auto evaluate_move(
        const solution_type& solution,
        const evaluation_type& current,
        const move_type& move) -> candidate_type
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

        trace::with_move_route(move, [&](const auto* route) {
            emit(trace::event::move_accepted<cost_type>{
                .evaluations = evaluations_,
                .iterations = iterations_,
                .previous_cost = previous_cost,
                .cost = current.cost(),
                .neighborhood = route,
            });
        });
    }

    void incumbent_updated(
        const cost_type& previous_cost,
        const cost_type& cost)
    {
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
    auto finish(solution_type solution, cost_type cost) -> result_type
    {
        return finish(
            std::move(solution),
            std::move(cost),
            stop_reason_.value_or(termination_reason::completed));
    }

    [[nodiscard]]
    auto finish(
        solution_type solution,
        cost_type cost,
        const termination_reason reason) -> result_type
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

        return result_type{
            .solution = std::move(solution),
            .cost = std::move(cost),
            .evaluations = evaluations_,
            .iterations = iterations_,
            .termination = reason,
        };
    }

    // A run over a decorated context sharing this run's control, tracer and
    // budget, e.g. for algorithms that delegate to another algorithm.
    template<class OtherContext>
    [[nodiscard]]
    auto with_context(const OtherContext& context) -> search_run<OtherContext, Tracer>
    {
        return search_run<OtherContext, Tracer>{
            context,
            control_,
            tracer_,
            evaluation_limit_,
        };
    }

private:
    void report() const
    {
        control_.report(run_progress{
            .evaluations = evaluations_,
            .iterations = iterations_,
            .evaluation_limit = evaluation_limit_ == no_limit
                ? std::nullopt
                : std::optional<std::size_t>{evaluation_limit_},
        });
    }

    static constexpr std::size_t no_limit = std::numeric_limits<std::size_t>::max();

    const Context& context_;
    evaluation_facility_type evaluation_;
    const run_control& control_;
    Tracer& tracer_;
    std::size_t evaluations_{};
    std::size_t iterations_{};
    std::size_t evaluation_limit_{no_limit};
    std::optional<termination_reason> stop_reason_;
};

} // namespace easylocal
