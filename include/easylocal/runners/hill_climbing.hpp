#pragma once

// HillClimbing: random moves accepted when they do not worsen the cost, until
// too many consecutive proposals bring no strict improvement.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/search_run.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <random>
#include <utility>

namespace easylocal::runners
{

struct HillClimbingParameters
{
    // Consecutive proposals without a strict improvement after which the
    // search stops.
    std::size_t max_idle_iterations{1000};
    // Evaluation budget, including the initial evaluation; 0 means no budget.
    std::size_t max_evaluations{0};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "max_idle_iterations",
                &HillClimbingParameters::max_idle_iterations>(
                "Maximum number of consecutive proposals without "
                "improvement"),
            config::field<"max_evaluations", &HillClimbingParameters::max_evaluations>(
                "Maximum number of solution evaluations (0: no budget)"));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        if (max_idle_iterations == 0)
        {
            return config::validation_result::failure(
                "max_idle_iterations must be positive");
        }
        return config::validation_result::success();
    }
};

// Hill Climbing: at each iteration a random move is proposed and accepted if
// it does not worsen the current cost, so the search can drift across
// plateaus. It stops after max_idle_iterations consecutive proposals without
// a strict improvement. The cost never worsens, so the current solution is
// also the best one.
class HillClimbing
{
public:
    using parameters_type = HillClimbingParameters;

    explicit HillClimbing(const HillClimbingParameters parameters) noexcept
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
        if (parameters_.max_evaluations != 0)
            run.limit_evaluations(parameters_.max_evaluations);
        auto current = run.start(solution);
        std::size_t idle_iterations = 0;

        while (!run.should_stop())
        {
            if (idle_iterations >= parameters_.max_idle_iterations)
            {
                return run.finish(
                    std::move(solution),
                    current.cost(),
                    termination_reason::idle_limit_reached);
            }

            auto move = run.random_move(solution, rng);
            if (!move.has_value())
            {
                return run.finish(
                    std::move(solution),
                    current.cost(),
                    termination_reason::local_optimum);
            }

            run.next_iteration();
            auto candidate = run.evaluate_move(solution, current, *move);

            if (run.better(candidate.cost(), current.cost()))
                idle_iterations = 0;
            else
                ++idle_iterations;

            if (run.better_or_equivalent(candidate.cost(), current.cost()))
                run.commit(solution, current, std::move(candidate), *move);
        }

        return run.finish(std::move(solution), current.cost());
    }

private:
    HillClimbingParameters parameters_;
};

} // namespace easylocal::runners
