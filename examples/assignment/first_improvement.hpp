#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <utility>

namespace easylocal::mwe::assignment
{

namespace detail
{

template<class Context>
concept strict_improvement_context =
    requires(
        const Context& context,
        const typename Context::cost_type& candidate,
        const typename Context::cost_type& reference)
    {
        {
            context.better(candidate, reference)
        } -> std::convertible_to<bool>;
    };

} // namespace detail

enum class FirstImprovementTermination
{
    local_optimum,
    evaluation_budget_exhausted,
};

struct FirstImprovementParameters
{
    std::size_t max_evaluations;
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
        assert(parameters_.max_evaluations >= 1);
    }

    template<class Context>
        requires detail::strict_improvement_context<Context>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const
    {
        const auto& neighborhood = context.neighborhood_explorer();
        const auto evaluation = context.evaluation();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using result_type = FirstImprovementResult<solution_type, cost_type>;

        auto current = evaluation.evaluate(solution);
        std::size_t evaluations = 1;

        while (true)
        {
            bool improved = false;

            for (const auto move : neighborhood.moves(solution))
            {
                if (evaluations == parameters_.max_evaluations)
                {
                    return result_type{
                        .solution = std::move(solution),
                        .cost = current.cost(),
                        .evaluations = evaluations,
                        .termination = FirstImprovementTermination::
                            evaluation_budget_exhausted,
                    };
                }

                auto candidate =
                    evaluation.after_move(solution, current, move);
                ++evaluations;

                if (context.better(candidate.cost(), current.cost()))
                {
                    evaluation.accept(
                        solution,
                        current,
                        std::move(candidate));
                    improved = true;
                    break;
                }
            }

            if (!improved)
            {
                return result_type{
                    .solution = std::move(solution),
                    .cost = current.cost(),
                    .evaluations = evaluations,
                    .termination = FirstImprovementTermination::local_optimum,
                };
            }
        }
    }

private:
    FirstImprovementParameters parameters_;
};

} // namespace easylocal::mwe::assignment
