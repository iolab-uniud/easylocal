#pragma once

#include <easylocal/config/tree.hpp>
#include <easylocal/run_control.hpp>
#include <easylocal/runner_tag.hpp>
#include <easylocal/search/detail/context_concepts.hpp>
#include <easylocal/trace.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <utility>

namespace easylocal::search
{

enum class FirstImprovementTermination
{
    local_optimum,
    evaluation_budget_exhausted,
    cancelled,
};

struct FirstImprovementParameters
{
    std::size_t max_evaluations;

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "max_evaluations",
                &FirstImprovementParameters::max_evaluations>(
                    "Maximum number of solution evaluations"));
    }

    [[nodiscard]]
    constexpr auto validate() const noexcept -> config::validation_result
    {
        if (max_evaluations == 0)
        {
            return config::validation_result::failure(
                "max_evaluations must be positive");
        }

        return config::validation_result::success();
    }
};

template<class Solution, class Cost>
struct FirstImprovementResult
{
    Solution solution;
    Cost cost;
    std::size_t evaluations;
    FirstImprovementTermination termination;
};

class FirstImprovement
{
public:
    explicit FirstImprovement(
        const FirstImprovementParameters parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.validate());
    }

    [[nodiscard]]
    auto parameters() const noexcept -> const FirstImprovementParameters&
    {
        return parameters_;
    }

    [[nodiscard]]
    auto configure(FirstImprovementParameters parameters) noexcept
        -> config::validation_result
    {
        const auto validation = parameters.validate();
        if (!validation)
        {
            return validation;
        }

        parameters_ = parameters;
        return config::validation_result::success();
    }

    [[nodiscard]]
    auto configuration() noexcept
    {
        return config::endpoint<"search">(*this);
    }

    [[nodiscard]]
    auto configuration() const noexcept
    {
        return config::endpoint<"search">(*this);
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
    template<class Context, easylocal::run_control_like Control, class Tracer>
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
        using result_type = FirstImprovementResult<solution_type, cost_type>;
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
                        .termination = FirstImprovementTermination::cancelled,
                    };
                }
            }

            bool improved = false;

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
                            .termination = FirstImprovementTermination::cancelled,
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
                        .termination = FirstImprovementTermination::
                            evaluation_budget_exhausted,
                    };
                }

                auto candidate =
                    evaluation.evaluate_move(solution, current, move);
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

                if (context.better(candidate.cost(), current.cost()))
                {
                    const auto previous_cost = current.cost();
                    evaluation.commit(
                        solution,
                        current,
                        std::move(candidate));
                    ++iterations;
                    trace::with_move_route(move, [&](const auto* route) {
                        trace::emit(tracer, trace::event::move_accepted<cost_type>{
                            .evaluations = evaluations,
                            .iterations = iterations,
                            .previous_cost = previous_cost,
                            .cost = current.cost(),
                            .neighborhood = route,
                        });
                    });
                    improved = true;
                    break;
                }
            }

            if (!improved)
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
                    .termination = FirstImprovementTermination::local_optimum,
                };
            }
        }
    }

    FirstImprovementParameters parameters_;
};

} // namespace easylocal::search

namespace easylocal::runner
{

using first_improvement = algorithm_tag<
    search::FirstImprovement,
    search::FirstImprovementParameters>;

} // namespace easylocal::runner
