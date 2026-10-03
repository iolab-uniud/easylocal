#pragma once

#include "solution_manager.hpp"
#include "swap_move.hpp"

#include <easylocal/helpers/neighborhood_explorer.hpp>

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
// IN1, a swap of the same two jobs; IN2, any swap moving a or b.
enum class SwapInverse
{
    both_jobs,
    either_job,
};

class SwapJobsNeighborhoodExplorer
    : public easylocal::neighborhood_explorer_base<PfspSolutionManager, SwapJobsMove>
{
public:
    explicit SwapJobsNeighborhoodExplorer(
        const PfspSolutionManager& solution_manager,
        const SwapInverse inverse = SwapInverse::both_jobs)
        : neighborhood_explorer_base{solution_manager}, inverse_{inverse}
    {
    }

    static constexpr std::string_view name()
    {
        return "Swap jobs";
    }

    bool is_valid(const Schedule& solution, const SwapJobsMove& move) const
    {
        return move.first_position < move.second_position
            && move.second_position < solution.order.size()
            && solution.order[move.first_position] == move.first_job
            && solution.order[move.second_position] == move.second_job;
    }

    bool first_move(const Schedule& solution, SwapJobsMove& move) const
    {
        return find_from(solution, 0, 1, move);
    }

    bool next_move(const Schedule& solution, SwapJobsMove& move) const
    {
        return find_from(solution, move.first_position, move.second_position + 1, move);
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

    // Whether move is forbidden by tabu_move, by the configured definition.
    bool inverse(const Schedule&, const SwapJobsMove& move, const SwapJobsMove& tabu_move)
        const
    {
        const auto moves = [&move](const job_id job) {
            return move.first_job == job || move.second_job == job;
        };
        if (inverse_ == SwapInverse::both_jobs)
            return moves(tabu_move.first_job) && moves(tabu_move.second_job);
        return moves(tabu_move.first_job) || moves(tabu_move.second_job);
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

    static bool find_from(
        const Schedule& solution,
        const std::size_t initial_first,
        const std::size_t initial_second,
        SwapJobsMove& move)
    {
        const auto job_count = solution.order.size();
        for (auto first = initial_first; first < job_count; ++first)
        {
            const auto second = first == initial_first ? initial_second : first + 1;
            if (second < job_count)
            {
                move = make(solution, first, second);
                return true;
            }
        }
        return false;
    }

    SwapInverse inverse_;
};

} // namespace pfsp
