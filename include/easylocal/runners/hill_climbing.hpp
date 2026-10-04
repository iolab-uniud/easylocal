#pragma once

/// \file
/// HillClimbing: random moves accepted when they do not worsen the cost, until
/// too many consecutive proposals bring no strict improvement.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/utils/limit.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <random>
#include <utility>

namespace easylocal::runners
{

/// The parameters of HillClimbing.
struct HillClimbingParameters
{
    /// Consecutive proposals without a strict improvement after which the
    /// search stops.
    std::size_t max_idle_iterations{1000};
    /// Evaluation budget, including the initial evaluation; unlimited by default.
    limit max_evaluations{unlimited};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "max_idle_iterations",
                &HillClimbingParameters::max_idle_iterations>(
                "Maximum number of consecutive proposals without "
                "improvement",
                config::range(1, easylocal::unlimited)),
            config::field<"max_evaluations", &HillClimbingParameters::max_evaluations>(
                "Maximum number of solution evaluations, or unlimited",
                config::range(0, easylocal::unlimited)));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        if (const auto schema = config::check_schema(*this); !schema)
            return schema;
        if (max_idle_iterations == 0)
        {
            return config::validation_result::failure(
                "max_idle_iterations must be positive");
        }
        return config::validation_result::success();
    }
};

/// Hill Climbing: at each iteration a random move is proposed and accepted if
/// it does not worsen the current cost, so the search can drift across
/// plateaus.
///
/// It stops after max_idle_iterations consecutive proposals without a strict
/// improvement. The cost never worsens, so the current solution is also the
/// best one. Requires a neighborhood explorer with random_move() and a cost
/// with better() and better_or_equivalent().
class HillClimbing
{
public:
    /// The parameter block of the algorithm.
    using parameters_type = HillClimbingParameters;

    explicit HillClimbing(const HillClimbingParameters parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.validate());
    }

    /// Runs the search from solution, drawing random moves with rng.
    ///
    /// The bound runner calls it, with the run of its context (neighborhood,
    /// evaluation, cost relations).
    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::random_move_context<typename Run::context_type, RNG>
        && detail::non_worsening_context<typename Run::context_type>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
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
