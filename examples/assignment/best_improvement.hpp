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
        const auto& neighborhood = context.neighborhood_explorer();
        const auto evaluation = context.evaluation();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using result_type = BestImprovementResult<solution_type, cost_type>;

        auto current = evaluation.evaluate(solution);
        std::size_t evaluations = 1;

        while (true)
        {
            using candidate_type = typename decltype(evaluation)::candidate_type;

            std::optional<candidate_type> best_candidate;
            auto best_cost = current.cost();

            for (const auto move : neighborhood.moves(solution))
            {
                if (evaluations == parameters_.max_evaluations)
                {
                    return result_type{
                        .solution = std::move(solution),
                        .cost = current.cost(),
                        .evaluations = evaluations,
                        .termination = BestImprovementTermination::
                            evaluation_budget_exhausted,
                    };
                }

                auto candidate =
                    evaluation.after_move(solution, current, move);
                ++evaluations;

                if (candidate.cost() < best_cost)
                {
                    best_cost = candidate.cost();
                    best_candidate = std::move(candidate);
                }
            }

            if (!best_candidate.has_value())
            {
                return result_type{
                    .solution = std::move(solution),
                    .cost = current.cost(),
                    .evaluations = evaluations,
                    .termination = BestImprovementTermination::local_optimum,
                };
            }

            evaluation.accept(
                solution,
                current,
                std::move(*best_candidate));
        }
    }

private:
    BestImprovementParameters parameters_;
};

} // namespace easylocal::mwe::assignment
