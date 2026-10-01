#pragma once

#include <easylocal/search/detail/context_concepts.hpp>
#include <easylocal/search_run.hpp>

#include <cassert>
#include <cstddef>
#include <optional>
#include <utility>

namespace easylocal::search
{

struct BestImprovementParameters
{
    std::size_t max_evaluations;
};

class BestImprovement
{
public:
    using parameters_type = BestImprovementParameters;

    explicit BestImprovement(
        const BestImprovementParameters parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.max_evaluations >= 1);
    }

    template<class Run>
        requires detail::enumerating_strict_improvement_context<
            typename Run::context_type>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution) const
    {
        run.limit_evaluations(parameters_.max_evaluations);
        auto current = run.start(solution);

        while (true)
        {
            std::optional<typename Run::candidate_type> best_candidate;
            std::optional<typename Run::move_type> best_move;
            auto best_cost = current.cost();

            for (const auto move : run.moves(solution))
            {
                if (run.should_stop())
                {
                    return run.finish(std::move(solution), current.cost());
                }

                auto candidate = run.evaluate_move(solution, current, move);
                if (run.better(candidate.cost(), best_cost))
                {
                    best_cost = candidate.cost();
                    best_candidate = std::move(candidate);
                    best_move = move;
                }
            }

            if (!best_candidate.has_value())
            {
                return run.finish(
                    std::move(solution),
                    current.cost(),
                    termination_reason::local_optimum);
            }

            run.next_iteration();
            run.commit(
                solution,
                current,
                std::move(*best_candidate),
                *best_move);
        }
    }

private:
    BestImprovementParameters parameters_;
};

} // namespace easylocal::search
