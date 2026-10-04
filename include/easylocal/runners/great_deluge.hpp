#pragma once

/// \file
/// GreatDeluge (Dueck): random moves accepted while their cost stays below a
/// decreasing water level; arithmetic costs only.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost/concepts.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/utils/limit.hpp>

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <random>
#include <utility>

namespace easylocal::runners
{

/// The parameters of GreatDeluge.
struct GreatDelugeParameters
{
    /// Initial water level, as a factor of the initial cost.
    double initial_level{1.1};
    /// The search stops when the level falls below this factor of the best
    /// cost.
    double min_level{0.9};
    /// Multiplicative decrease of the level.
    double level_rate{0.99};
    /// Proposals at each level.
    std::size_t neighbors_sampled{100};
    /// Evaluation budget, including the initial evaluation; unlimited by default.
    limit max_evaluations{unlimited};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"initial_level", &GreatDelugeParameters::initial_level>(
                "Initial water level, as a factor of the initial cost"),
            config::field<"min_level", &GreatDelugeParameters::min_level>(
                "Final water level, as a factor of the best cost"),
            config::field<"level_rate", &GreatDelugeParameters::level_rate>(
                "Multiplicative decrease of the water level",
                config::range(0.0, 1.0).open()),
            config::field<"neighbors_sampled", &GreatDelugeParameters::neighbors_sampled>(
                "Number of proposals at each water level"),
            config::field<"max_evaluations", &GreatDelugeParameters::max_evaluations>(
                "Maximum number of solution evaluations, or unlimited"));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        if (const auto domains = config::check_domains(*this); !domains)
            return domains;
        if (!std::isfinite(initial_level) || initial_level <= 0.0)
        {
            return config::validation_result::failure(
                "initial_level must be finite and positive");
        }
        if (!std::isfinite(min_level) || min_level <= 0.0)
        {
            return config::validation_result::failure(
                "min_level must be finite and positive");
        }
        if (min_level >= initial_level)
        {
            return config::validation_result::failure(
                "min_level must be smaller than initial_level");
        }
        if (!std::isfinite(level_rate) || level_rate <= 0.0 || level_rate >= 1.0)
        {
            return config::validation_result::failure(
                "level_rate must be finite and in the open interval (0, 1)");
        }
        if (neighbors_sampled == 0)
        {
            return config::validation_result::failure(
                "neighbors_sampled must be positive");
        }
        return config::validation_result::success();
    }
};

namespace detail
{

template<class Context>
concept great_deluge_cost = cost::arithmetic<typename Context::cost_type>;

} // namespace detail

/// Great Deluge (Dueck): a random move is accepted if it improves the current
/// cost or if its cost does not exceed the water level.
///
/// The level starts at initial_level times the initial cost and is multiplied
/// by level_rate every neighbors_sampled proposals; the search stops when it
/// falls below min_level times the best cost, and returns the best solution
/// found. The level is a value of the cost, so costs are arithmetic and, as the
/// levels are factors of them, positive: the search stops at once when the best
/// cost is zero or negative. Requires a neighborhood explorer with
/// random_move() and an arithmetic cost (cost::arithmetic).
class GreatDeluge
{
public:
    /// The parameter block of the algorithm.
    using parameters_type = GreatDelugeParameters;

    explicit GreatDeluge(const GreatDelugeParameters parameters) noexcept
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
        && detail::strict_improvement_context<typename Run::context_type>
        && detail::great_deluge_cost<typename Run::context_type>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
        run.limit_evaluations(parameters_.max_evaluations);
        auto current = run.start(solution);
        best_so_far best{solution, current.cost()};

        auto level = parameters_.initial_level * static_cast<double>(current.cost());
        std::size_t sampled = 0;

        while (!run.should_stop()
            && level >= parameters_.min_level * static_cast<double>(best.cost)
            && best.cost > 0)
        {
            auto move = run.random_move(solution, rng);
            if (!move.has_value())
                break;

            run.next_iteration();
            auto candidate = run.evaluate_move(solution, current, *move);

            if (run.better(candidate.cost(), current.cost())
                || static_cast<double>(candidate.cost()) <= level)
            {
                run.commit(solution, current, std::move(candidate), *move);

                best.update(run, solution, current);
            }

            if (++sampled == parameters_.neighbors_sampled)
            {
                level *= parameters_.level_rate;
                sampled = 0;
            }
        }

        return run.finish(std::move(best.solution), std::move(best.cost));
    }

private:
    GreatDelugeParameters parameters_;
};

} // namespace easylocal::runners
