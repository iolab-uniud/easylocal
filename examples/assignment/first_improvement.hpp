#pragma once

#include <cassert>
#include <cstddef>
#include <utility>

namespace easylocal::mwe::assignment
{

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
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const
    {
        const auto& solution_manager = context.solution_manager();
        const auto& neighborhood = context.neighborhood_explorer();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using result_type = FirstImprovementResult<solution_type, cost_type>;

        auto current_cost = solution_manager.evaluate(solution);
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
                        .cost = current_cost,
                        .evaluations = evaluations,
                        .termination = FirstImprovementTermination::
                            evaluation_budget_exhausted,
                    };
                }

                solution_type candidate = solution;
                neighborhood.make_move(candidate, move);

                const auto candidate_cost =
                    solution_manager.evaluate(candidate);
                ++evaluations;

                if (candidate_cost < current_cost)
                {
                    solution = std::move(candidate);
                    current_cost = candidate_cost;
                    improved = true;
                    break;
                }
            }

            if (!improved)
            {
                return result_type{
                    .solution = std::move(solution),
                    .cost = current_cost,
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
