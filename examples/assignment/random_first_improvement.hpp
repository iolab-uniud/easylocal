#pragma once

#include "sampling.hpp"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <random>
#include <ranges>
#include <utility>

namespace easylocal::mwe::assignment
{

namespace detail
{

template<class Context, class RNG>
concept random_first_improvement_context =
    std::uniform_random_bit_generator<RNG> &&
    requires(
        const Context& context,
        const typename Context::solution_type& solution,
        RNG& rng)
    {
        typename Context::neighborhood_explorer_type::random_sampling;

        requires std::same_as<
            typename Context::neighborhood_explorer_type::random_sampling,
            sampling::without_replacement>;

        {
            context.neighborhood_explorer().random_moves(solution, rng)
        } -> std::ranges::input_range;
    };

} // namespace detail

enum class RandomFirstImprovementTermination
{
    local_optimum,
    evaluation_budget_exhausted,
};

struct RandomFirstImprovementParameters
{
    std::size_t max_evaluations;
};

template<class Solution, class Cost>
struct RandomFirstImprovementResult
{
    Solution solution;
    Cost cost;
    std::size_t evaluations;
    RandomFirstImprovementTermination termination;
};

class RandomFirstImprovement
{
public:
    explicit RandomFirstImprovement(
        const RandomFirstImprovementParameters parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.max_evaluations >= 1);
    }

    template<class Context, std::uniform_random_bit_generator RNG>
        requires detail::random_first_improvement_context<Context, RNG>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution,
        RNG& rng) const
    {
        const auto& solution_manager = context.solution_manager();
        const auto& neighborhood = context.neighborhood_explorer();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using result_type =
            RandomFirstImprovementResult<solution_type, cost_type>;

        auto current_cost = solution_manager.evaluate(solution);
        std::size_t evaluations = 1;

        while (true)
        {
            bool improved = false;

            for (const auto move : neighborhood.random_moves(solution, rng))
            {
                if (evaluations == parameters_.max_evaluations)
                {
                    return result_type{
                        .solution = std::move(solution),
                        .cost = current_cost,
                        .evaluations = evaluations,
                        .termination = RandomFirstImprovementTermination::
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
                    .termination =
                        RandomFirstImprovementTermination::local_optimum,
                };
            }
        }
    }

private:
    RandomFirstImprovementParameters parameters_;
};

} // namespace easylocal::mwe::assignment
