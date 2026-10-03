#pragma once

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/search_run.hpp>

#include <cstddef>
#include <optional>
#include <utility>

namespace easylocal::runners
{

struct BestImprovementParameters
{
    // Evaluation budget, including the initial evaluation; 0 means no budget:
    // the search runs until a local optimum.
    std::size_t max_evaluations{0};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "max_evaluations",
                &BestImprovementParameters::max_evaluations>(
                    "Maximum number of solution evaluations "
                    "(0: until a local optimum)"));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        return config::validation_result::success();
    }
};

class BestImprovement
{
public:
    using parameters_type = BestImprovementParameters;

    explicit BestImprovement(
        const BestImprovementParameters parameters) noexcept
        : parameters_{parameters}
    {
    }

    [[nodiscard]]
    const BestImprovementParameters& parameters() const noexcept
    {
        return parameters_;
    }

    [[nodiscard]]
    config::validation_result configure(BestImprovementParameters parameters) noexcept
    {
        const auto validation = parameters.validate();
        if (!validation)
        {
            return validation;
        }

        parameters_ = parameters;
        return config::validation_result::success();
    }

    // The parameters, at the root: the runner puts them under "search".
    [[nodiscard]]
    config::parameter_set configuration()
    {
        config::parameter_set parameters;
        parameters.add(*this);
        return parameters;
    }

    [[nodiscard]]
    config::parameter_set configuration() const
    {
        config::parameter_set parameters;
        parameters.add(*this);
        return parameters;
    }

    template<class Run>
        requires detail::enumerating_strict_improvement_context<
            typename Run::context_type>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution) const
    {
        if (parameters_.max_evaluations != 0)
        {
            run.limit_evaluations(parameters_.max_evaluations);
        }
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

} // namespace easylocal::runners
