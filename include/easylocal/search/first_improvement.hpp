#pragma once

#include <easylocal/config/tree.hpp>
#include <easylocal/search/detail/context_concepts.hpp>

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

            for (const auto move : easylocal::moves(neighborhood, solution))
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
                    evaluation.evaluate_move(solution, current, move);
                ++evaluations;

                if (context.better(candidate.cost(), current.cost()))
                {
                    evaluation.commit(
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

} // namespace easylocal::search

namespace easylocal::runner
{

struct first_improvement
{
    using config_type = search::FirstImprovementParameters;

    [[nodiscard]]
    static auto make(const config_type config) -> search::FirstImprovement
    {
        return search::FirstImprovement{config};
    }
};

} // namespace easylocal::runner
