#pragma once

#include <easylocal/run_control.hpp>
#include <easylocal/runner_tag.hpp>
#include <easylocal/search/detail/context_concepts.hpp>
#include <easylocal/trace.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <optional>
#include <utility>

namespace easylocal::search
{

enum class BestImprovementTermination
{
    local_optimum,
    evaluation_budget_exhausted,
    cancelled,
};

struct BestImprovementParameters
{
    std::size_t max_evaluations;
};

template<class Solution, class Cost>
struct BestImprovementResult
{
    Solution solution;
    Cost cost;
    std::size_t evaluations;
    BestImprovementTermination termination;
};

class BestImprovement
{
public:
    explicit BestImprovement(
        const BestImprovementParameters parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.max_evaluations >= 1);
    }

    template<class Context>
        requires detail::enumerating_strict_improvement_context<Context>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const
    {
        trace::null_tracer tracer;
        return run_impl(
            context,
            std::move(solution),
            easylocal::detail::no_run_control{},
            tracer);
    }

    template<class Context>
        requires detail::enumerating_strict_improvement_context<Context>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution,
        const run_control& control) const
    {
        trace::null_tracer tracer;
        return run_impl(context, std::move(solution), control, tracer);
    }

    template<class Context, class Tracer>
        requires detail::enumerating_strict_improvement_context<Context> &&
                 trace::tracer_for<
                     Tracer,
                     trace::event::run_started<typename Context::cost_type>>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution,
        Tracer& tracer) const
    {
        return run_impl(
            context,
            std::move(solution),
            easylocal::detail::no_run_control{},
            tracer);
    }

    template<class Context, class Tracer>
        requires detail::enumerating_strict_improvement_context<Context> &&
                 trace::tracer_for<
                     Tracer,
                     trace::event::run_started<typename Context::cost_type>>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution,
        const run_control& control,
        Tracer& tracer) const
    {
        return run_impl(context, std::move(solution), control, tracer);
    }

private:
    template<class Context, easylocal::detail::run_control_like Control, class Tracer>
    [[nodiscard]]
    auto run_impl(
        const Context& context,
        typename Context::solution_type solution,
        const Control& control,
        Tracer& tracer) const
    {
        const auto& neighborhood = context.neighborhood_explorer();
        const auto evaluation = context.evaluation();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using result_type = BestImprovementResult<solution_type, cost_type>;
        constexpr bool controlled_run =
            !std::same_as<Control, easylocal::detail::no_run_control>;

        auto current = evaluation.evaluate(solution);
        std::size_t evaluations = 1;
        std::size_t iterations = 0;

        trace::emit(tracer, trace::event::run_started<cost_type>{current.cost()});

        if constexpr (controlled_run)
        {
            control.report(run_progress{
                .evaluations = evaluations,
                .iterations = iterations,
                .evaluation_limit = parameters_.max_evaluations,
            });
        }

        while (true)
        {
            if constexpr (controlled_run)
            {
                if (control.stop_requested())
                {
                    trace::emit(tracer, trace::event::run_finished<cost_type>{
                        .evaluations = evaluations,
                        .iterations = iterations,
                        .cost = current.cost(),
                    });
                    return result_type{
                        .solution = std::move(solution),
                        .cost = current.cost(),
                        .evaluations = evaluations,
                        .termination = BestImprovementTermination::cancelled,
                    };
                }
            }

            using candidate_type = typename decltype(evaluation)::candidate_type;

            std::optional<candidate_type> best_candidate;
            std::optional<typename Context::neighborhood_explorer_type::move_type> best_move;
            auto best_cost = current.cost();

            for (const auto move : easylocal::moves(neighborhood, solution))
            {
                if constexpr (controlled_run)
                {
                    if (control.stop_requested())
                    {
                        trace::emit(tracer, trace::event::run_finished<cost_type>{
                            .evaluations = evaluations,
                            .iterations = iterations,
                            .cost = current.cost(),
                        });
                        return result_type{
                            .solution = std::move(solution),
                            .cost = current.cost(),
                            .evaluations = evaluations,
                            .termination = BestImprovementTermination::cancelled,
                        };
                    }
                }

                if (evaluations == parameters_.max_evaluations)
                {
                    trace::emit(tracer, trace::event::run_finished<cost_type>{
                        .evaluations = evaluations,
                        .iterations = iterations,
                        .cost = current.cost(),
                    });
                    return result_type{
                        .solution = std::move(solution),
                        .cost = current.cost(),
                        .evaluations = evaluations,
                        .termination = BestImprovementTermination::
                            evaluation_budget_exhausted,
                    };
                }

                auto candidate = evaluation.evaluate_move(solution, current, move);
                ++evaluations;

                trace::with_move_route(move, [&](const auto* route) {
                    trace::emit(tracer, trace::event::move_evaluated<cost_type>{
                        .evaluations = evaluations,
                        .iterations = iterations,
                        .current_cost = current.cost(),
                        .candidate_cost = candidate.cost(),
                        .neighborhood = route,
                    });
                });

                if constexpr (controlled_run)
                {
                    control.report(run_progress{
                        .evaluations = evaluations,
                        .iterations = iterations,
                        .evaluation_limit = parameters_.max_evaluations,
                    });
                }

                if (context.better(candidate.cost(), best_cost))
                {
                    best_cost = candidate.cost();
                    best_candidate = std::move(candidate);
                    best_move = move;
                }
            }

            if (!best_candidate.has_value())
            {
                trace::emit(tracer, trace::event::local_optimum<cost_type>{
                    .evaluations = evaluations,
                    .iterations = iterations,
                    .cost = current.cost(),
                });
                trace::emit(tracer, trace::event::run_finished<cost_type>{
                    .evaluations = evaluations,
                    .iterations = iterations,
                    .cost = current.cost(),
                });
                return result_type{
                    .solution = std::move(solution),
                    .cost = current.cost(),
                    .evaluations = evaluations,
                    .termination = BestImprovementTermination::local_optimum,
                };
            }

            const auto previous_cost = current.cost();
            evaluation.commit(
                solution,
                current,
                std::move(*best_candidate));
            ++iterations;
            trace::with_move_route(*best_move, [&](const auto* route) {
                trace::emit(tracer, trace::event::move_accepted<cost_type>{
                    .evaluations = evaluations,
                    .iterations = iterations,
                    .previous_cost = previous_cost,
                    .cost = current.cost(),
                    .neighborhood = route,
                });
            });
        }
    }

    BestImprovementParameters parameters_;
};

} // namespace easylocal::search

namespace easylocal::runner
{

using best_improvement = algorithm_tag<
    search::BestImprovement,
    search::BestImprovementParameters>;

} // namespace easylocal::runner
