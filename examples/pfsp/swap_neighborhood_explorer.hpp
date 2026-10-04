#pragma once

#include "solution_manager.hpp"
#include "swap_move.hpp"

#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/utils/generator.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string_view>
#include <utility>

namespace pfsp
{

// Which later moves a swap of jobs a and b forbids while it is tabu (Da Ros,
// Di Gaspero and Schaerf, "A performance analysis of tabu list strategies"):
// IN1, a swap of the same two jobs; IN2, any swap moving a or b. The two are
// distinct neighborhoods, resolved at compile time: inverse is called for
// every candidate move against every entry of the tabu list.
enum class SwapInverse : std::uint8_t
{
    both_jobs,
    either_job,
};

template<SwapInverse Inverse>
class BasicSwapJobsNeighborhoodExplorer
    : public easylocal::neighborhood_explorer_base<PfspSolutionManager, SwapJobsMove>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    static constexpr std::string_view name()
    {
        return Inverse == SwapInverse::both_jobs ? "Swap jobs (IN1)" : "Swap jobs (IN2)";
    }

    bool is_valid(const Schedule& solution, const SwapJobsMove& move) const
    {
        return move.first_position < move.second_position
            && move.second_position < solution.order.size()
            && solution.order[move.first_position] == move.first_job
            && solution.order[move.second_position] == move.second_job;
    }

    // Every pair of positions i < j, in lexicographic order.
    easylocal::generator<SwapJobsMove> moves(const Schedule& solution) const
    {
        const auto n = solution.order.size();
        for (std::size_t i = 0; i < n; ++i)
            for (auto j = i + 1; j < n; ++j)
                co_yield make(solution, i, j);
    }

    // A uniform swap: two distinct positions, the second drawn among the
    // others and ordered.
    template<std::uniform_random_bit_generator RNG>
    std::optional<SwapJobsMove> random_move(const Schedule& solution, RNG& rng) const
    {
        const auto job_count = solution.order.size();
        if (job_count < 2)
            return std::nullopt;

        std::uniform_int_distribution<std::size_t> draw_first{0, job_count - 1};
        std::uniform_int_distribution<std::size_t> draw_other{0, job_count - 2};
        const auto first = draw_first(rng);
        auto second = draw_other(rng);
        if (second >= first)
            ++second;
        return make(solution, std::min(first, second), std::max(first, second));
    }

    void make_move(Schedule& solution, const SwapJobsMove& move) const
    {
        std::swap(
            solution.order[move.first_position],
            solution.order[move.second_position]);
    }

    // Whether move is forbidden by tabu_move, by the neighborhood's definition.
    bool inverse(const Schedule&, const SwapJobsMove& move, const SwapJobsMove& tabu_move)
        const
    {
        const auto touches = [&move](const job_id job) {
            return move.first_job == job || move.second_job == job;
        };
        if constexpr (Inverse == SwapInverse::both_jobs)
            return touches(tabu_move.first_job) && touches(tabu_move.second_job);
        else
            return touches(tabu_move.first_job) || touches(tabu_move.second_job);
    }

    // The pair of jobs, whatever their positions, for frequency-based memory.
    static std::uint64_t tabu_attribute(const SwapJobsMove& move)
    {
        const auto low = std::min(move.first_job, move.second_job);
        const auto high = std::max(move.first_job, move.second_job);
        return (static_cast<std::uint64_t>(low) << 32U)
            | static_cast<std::uint64_t>(high);
    }

private:
    static SwapJobsMove make(
        const Schedule& solution,
        const std::size_t first,
        const std::size_t second)
    {
        return SwapJobsMove{
            .first_position = first,
            .second_position = second,
            .first_job = solution.order[first],
            .second_job = solution.order[second],
        };
    }
};

// IN1, the default of the study's best configurations, and IN2.
using SwapJobsNeighborhoodExplorer =
    BasicSwapJobsNeighborhoodExplorer<SwapInverse::both_jobs>;
using SwapEitherJobNeighborhoodExplorer =
    BasicSwapJobsNeighborhoodExplorer<SwapInverse::either_job>;

} // namespace pfsp
