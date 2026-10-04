#pragma once

// FirstImprovement: at each step the first move of the neighborhood that
// strictly improves the cost is applied, until a local optimum.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/utils/limit.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <utility>

namespace easylocal::runners
{

struct FirstImprovementParameters
{
    // Evaluation budget, including the initial evaluation; unlimited by default:
    // the search runs until a local optimum.
    limit max_evaluations{unlimited};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "max_evaluations",
                &FirstImprovementParameters::max_evaluations>(
                "Maximum number of solution evaluations "
                "(unlimited: until a local optimum)"));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        return config::validation_result::success();
    }
};

class FirstImprovement
{
public:
    using parameters_type = FirstImprovementParameters;

    explicit FirstImprovement(
        const FirstImprovementParameters parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.validate());
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
            bool improved = false;

            for (const auto move : run.moves(solution))
            {
                if (run.should_stop())
                {
                    return run.finish(std::move(solution), current.cost());
                }

                auto candidate = run.evaluate_move(solution, current, move);
                if (run.better(candidate.cost(), current.cost()))
                {
                    run.next_iteration();
                    run.commit(solution, current, std::move(candidate), move);
                    improved = true;
                    break;
                }
            }

            if (!improved)
            {
                return run.finish(
                    std::move(solution),
                    current.cost(),
                    termination_reason::local_optimum);
            }
        }
    }

private:
    FirstImprovementParameters parameters_;
};

} // namespace easylocal::runners
