#pragma once

#include <easylocal/config/tree.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/search_run.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <utility>

namespace easylocal::runners
{

struct FirstImprovementParameters
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
                &FirstImprovementParameters::max_evaluations>(
                    "Maximum number of solution evaluations "
                    "(0: until a local optimum)"));
    }

    [[nodiscard]]
    constexpr auto validate() const noexcept -> config::validation_result
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
