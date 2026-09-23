#pragma once

#include <cassert>
#include <cstddef>
#include <optional>
#include <utility>

namespace easylocal::mwe::assignment
{

enum class BestImprovementTermination
{
    local_optimum,
    evaluation_budget_exhausted,
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
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const
    {
        const auto& solution_manager = context.solution_manager();
        const auto& neighborhood = context.neighborhood_explorer();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using result_type = BestImprovementResult<solution_type, cost_type>;

        auto current_cost = solution_manager.evaluate(solution);
        std::size_t evaluations = 1;

        while (true)
        {
            std::optional<solution_type> best_solution;
            auto best_cost = current_cost;

            for (const auto move : neighborhood.moves(solution))
            {
                if (evaluations == parameters_.max_evaluations)
                {
                    return result_type{
                        .solution = std::move(solution),
                        .cost = current_cost,
                        .evaluations = evaluations,
                        .termination = BestImprovementTermination::
                            evaluation_budget_exhausted,
                    };
                }

                solution_type candidate = solution;
                neighborhood.make_move(candidate, move);

                const auto candidate_cost =
                    solution_manager.evaluate(candidate);
                ++evaluations;

                if (candidate_cost < best_cost)
                {
                    best_solution = std::move(candidate);
                    best_cost = candidate_cost;
                }
            }

            if (!best_solution.has_value())
            {
                return result_type{
                    .solution = std::move(solution),
                    .cost = current_cost,
                    .evaluations = evaluations,
                    .termination = BestImprovementTermination::local_optimum,
                };
            }

            solution = std::move(*best_solution);
            current_cost = best_cost;
        }
    }

private:
    BestImprovementParameters parameters_;
};

} // namespace easylocal::mwe::assignment
