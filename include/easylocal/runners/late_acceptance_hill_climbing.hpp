#pragma once

#include <easylocal/config/tree.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/search_run.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <random>
#include <utility>
#include <vector>

namespace easylocal::runners
{

struct LateAcceptanceHillClimbingParameters
{
    // Number of past costs a candidate is compared with.
    std::size_t history_length{10};
    // Consecutive proposals without improving the best cost after which the
    // search stops.
    std::size_t max_idle_iterations{1000};
    // Evaluation budget, including the initial evaluation; 0 means no budget.
    std::size_t max_evaluations{0};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "history_length",
                &LateAcceptanceHillClimbingParameters::history_length>(
                "Number of past costs a candidate is compared with"),
            config::field<
                "max_idle_iterations",
                &LateAcceptanceHillClimbingParameters::max_idle_iterations>(
                "Maximum number of consecutive proposals without improving "
                "the best cost"),
            config::field<
                "max_evaluations",
                &LateAcceptanceHillClimbingParameters::max_evaluations>(
                "Maximum number of solution evaluations (0: no budget)"));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        if (history_length == 0)
            return config::validation_result::failure("history_length must be positive");
        if (max_idle_iterations == 0)
        {
            return config::validation_result::failure(
                "max_idle_iterations must be positive");
        }
        return config::validation_result::success();
    }
};

// Late Acceptance Hill Climbing (Burke and Bykov): a random move is accepted
// if its cost is better than or equivalent to the current cost, or to the
// cost the current solution had history_length iterations earlier. After each
// proposal the current cost replaces that oldest entry of the history, which
// starts filled with the initial cost. With history_length 1 it accepts the
// moves Hill Climbing accepts. It stops after max_idle_iterations consecutive proposals
// without improving the best cost, and returns the best solution found.
class LateAcceptanceHillClimbing
{
public:
    using parameters_type = LateAcceptanceHillClimbingParameters;

    explicit LateAcceptanceHillClimbing(
        const LateAcceptanceHillClimbingParameters parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.validate());
    }

    [[nodiscard]]
    const LateAcceptanceHillClimbingParameters& parameters() const noexcept
    {
        return parameters_;
    }

    [[nodiscard]]
    config::validation_result configure(
        LateAcceptanceHillClimbingParameters parameters) noexcept
    {
        const auto validation = parameters.validate();
        if (!validation)
            return validation;

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

    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::random_move_context<typename Run::context_type, RNG>
        && detail::non_worsening_context<typename Run::context_type>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
        if (parameters_.max_evaluations != 0)
            run.limit_evaluations(parameters_.max_evaluations);
        auto current = run.start(solution);
        auto best_solution = solution;
        auto best_cost = current.cost();

        std::vector history(parameters_.history_length, current.cost());
        std::size_t position = 0;
        std::size_t idle_iterations = 0;

        while (!run.should_stop())
        {
            if (idle_iterations >= parameters_.max_idle_iterations)
            {
                return run.finish(
                    std::move(best_solution),
                    std::move(best_cost),
                    termination_reason::idle_limit_reached);
            }

            auto move = run.random_move(solution, rng);
            if (!move.has_value())
                break;

            run.next_iteration();
            auto candidate = run.evaluate_move(solution, current, *move);
            ++idle_iterations;

            if (run.better_or_equivalent(candidate.cost(), current.cost())
                || run.better_or_equivalent(candidate.cost(), history[position]))
            {
                run.commit(solution, current, std::move(candidate), *move);

                if (run.better(current.cost(), best_cost))
                {
                    const auto previous_best = best_cost;
                    best_solution = solution;
                    best_cost = current.cost();
                    run.incumbent_updated(previous_best, best_cost);
                    idle_iterations = 0;
                }
            }

            history[position] = current.cost();
            position = (position + 1) % history.size();
        }

        return run.finish(std::move(best_solution), std::move(best_cost));
    }

private:
    LateAcceptanceHillClimbingParameters parameters_;
};

} // namespace easylocal::runners
