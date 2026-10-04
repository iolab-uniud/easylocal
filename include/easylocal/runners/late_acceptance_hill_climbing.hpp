#pragma once

// LateAcceptanceHillClimbing (Burke and Bykov): a random move is accepted when
// it is no worse than the current cost or than the cost of history_length
// iterations ago.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/utils/limit.hpp>

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
    // Evaluation budget, including the initial evaluation; unlimited by default.
    limit max_evaluations{unlimited};

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
                "Maximum number of solution evaluations, or unlimited"));
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

    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::random_move_context<typename Run::context_type, RNG>
        && detail::non_worsening_context<typename Run::context_type>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
        run.limit_evaluations(parameters_.max_evaluations);
        auto current = run.start(solution);
        best_so_far best{solution, current.cost()};

        std::vector history(parameters_.history_length, current.cost());
        std::size_t position = 0;
        std::size_t idle_iterations = 0;

        while (!run.should_stop())
        {
            if (idle_iterations >= parameters_.max_idle_iterations)
            {
                return run.finish(
                    std::move(best.solution),
                    std::move(best.cost),
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

                if (best.update(run, solution, current))
                {
                    idle_iterations = 0;
                }
            }

            history[position] = current.cost();
            position = (position + 1) % history.size();
        }

        return run.finish(std::move(best.solution), std::move(best.cost));
    }

private:
    LateAcceptanceHillClimbingParameters parameters_;
};

} // namespace easylocal::runners
