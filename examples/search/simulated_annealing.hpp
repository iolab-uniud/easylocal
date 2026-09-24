#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <random>
#include <ranges>
#include <utility>

namespace easylocal::mwe::search
{

namespace detail
{

template<class Context, class RNG>
concept simulated_annealing_context =
    std::uniform_random_bit_generator<RNG> &&
    requires(
        const Context& context,
        const typename Context::solution_type& solution,
        RNG& rng)
    {
        {
            context.neighborhood_explorer().random_moves(solution, rng)
        } -> std::ranges::input_range;
    };

} // namespace detail

enum class SimulatedAnnealingTermination
{
    evaluation_budget_exhausted,
    empty_neighborhood,
};

struct SimulatedAnnealingParameters
{
    std::size_t max_evaluations;
    double initial_temperature;
    double cooling_factor;
};

template<class Solution, class Cost>
struct SimulatedAnnealingResult
{
    Solution solution;
    Cost cost;
    std::size_t evaluations;
    SimulatedAnnealingTermination termination;
};

template<class Acceptance>
class SimulatedAnnealing
{
public:
    SimulatedAnnealing(
        const SimulatedAnnealingParameters parameters,
        Acceptance acceptance)
        : parameters_{parameters},
          acceptance_{std::move(acceptance)}
    {
        assert(parameters_.max_evaluations >= 1);
        assert(parameters_.initial_temperature > 0.0);
        assert(parameters_.cooling_factor > 0.0);
        assert(parameters_.cooling_factor <= 1.0);
    }

    template<class Context, std::uniform_random_bit_generator RNG>
        requires detail::simulated_annealing_context<Context, RNG>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution,
        RNG& rng) const
    {
        const auto& neighborhood = context.neighborhood_explorer();
        const auto evaluation = context.evaluation();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using result_type = SimulatedAnnealingResult<solution_type, cost_type>;

        auto current = evaluation.evaluate(solution);
        std::size_t evaluations = 1;
        auto temperature = parameters_.initial_temperature;

        while (evaluations < parameters_.max_evaluations)
        {
            // One proposal is consumed per SA iteration. This deliberately
            // does not interpret the explorer's replacement semantic yet.
            auto proposals = neighborhood.random_moves(solution, rng);
            auto proposal = std::ranges::begin(proposals);

            if (proposal == std::ranges::end(proposals))
            {
                return result_type{
                    .solution = std::move(solution),
                    .cost = current.cost(),
                    .evaluations = evaluations,
                    .termination =
                        SimulatedAnnealingTermination::empty_neighborhood,
                };
            }

            auto candidate = evaluation.after_move(
                solution,
                current,
                *proposal);
            ++evaluations;

            if (acceptance_.accept(
                    candidate.cost(),
                    current.cost(),
                    temperature,
                    rng))
            {
                evaluation.accept(
                    solution,
                    current,
                    std::move(candidate));
            }

            temperature *= parameters_.cooling_factor;
        }

        return result_type{
            .solution = std::move(solution),
            .cost = current.cost(),
            .evaluations = evaluations,
            .termination =
                SimulatedAnnealingTermination::evaluation_budget_exhausted,
        };
    }

private:
    SimulatedAnnealingParameters parameters_;
    Acceptance acceptance_;
};

template<class Acceptance>
SimulatedAnnealing(SimulatedAnnealingParameters, Acceptance)
    -> SimulatedAnnealing<Acceptance>;

} // namespace easylocal::mwe::search
