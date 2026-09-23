#pragma once

#include "detail/random_ordinals_view.hpp"
#include "move.hpp"
#include "sampling.hpp"
#include "solution_manager.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <random>
#include <ranges>
#include <utility>

namespace easylocal::mwe::assignment
{

class NeighborhoodExplorer
{
private:
    template<std::ranges::viewable_range Ordinals>
    [[nodiscard]]
    auto decode_moves(
        const Solution& solution,
        Ordinals&& ordinals) const
    {
#ifndef NDEBUG
        const auto expected_signature = debug_signature(solution);

        return std::forward<Ordinals>(ordinals)
             | std::views::transform(
                   [this, &solution, expected_signature](
                       const std::size_t ordinal) {
                       assert(
                           debug_signature(solution) == expected_signature &&
                           "neighborhood range invalidated by Solution mutation");
                       return move_at(solution, ordinal);
                   });
#else
        return std::forward<Ordinals>(ordinals)
             | std::views::transform(
                   [this, &solution](const std::size_t ordinal) {
                       return move_at(solution, ordinal);
                   });
#endif
    }

public:
    using random_sampling = sampling::without_replacement;

    explicit NeighborhoodExplorer(
        const SolutionManager& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto is_valid(
        const Solution& solution,
        const Move& move) const noexcept -> bool
    {
        if (!solution_manager_.is_valid(solution))
        {
            return false;
        }

        const auto& instance = solution_manager_.instance();

        return move.job < solution.assignment.size() &&
               move.destination < instance.capacity.size() &&
               solution.assignment[move.job] != move.destination;
    }

    [[nodiscard]]
    auto moves(const Solution& solution) const
    {
        assert(solution_manager_.is_valid(solution));

        return decode_moves(
            solution,
            std::views::iota(std::size_t{0}, move_count(solution)));
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto random_moves(
        const Solution& solution,
        RNG& rng) const
    {
        assert(solution_manager_.is_valid(solution));

        return decode_moves(
            solution,
            detail::random_ordinals_view<RNG>{
                rng,
                move_count(solution),
            });
    }

    void make_move(
        Solution& solution,
        const Move& move) const noexcept
    {
        assert(is_valid(solution, move));
        solution.assignment[move.job] = move.destination;
    }

private:
    [[nodiscard]]
    auto move_count(const Solution& solution) const noexcept -> std::size_t
    {
        const auto machine_count =
            solution_manager_.instance().capacity.size();
        const auto alternatives_per_job =
            machine_count > 0 ? machine_count - 1 : std::size_t{0};

        return solution.assignment.size() * alternatives_per_job;
    }

    [[nodiscard]]
    auto move_at(
        const Solution& solution,
        const std::size_t ordinal) const noexcept -> Move
    {
        const auto machine_count =
            solution_manager_.instance().capacity.size();
        const auto alternatives_per_job = machine_count - 1;

        assert(alternatives_per_job != 0);
        assert(ordinal < move_count(solution));

        const auto job = ordinal / alternatives_per_job;
        const auto offset = ordinal % alternatives_per_job;
        const auto current = solution.assignment[job];
        const auto destination = offset < current ? offset : offset + 1;

        return Move{
            .job = job,
            .destination = destination,
        };
    }

#ifndef NDEBUG
    [[nodiscard]]
    static auto debug_signature(const Solution& solution) noexcept
        -> std::uint64_t
    {
        // Debug-only fingerprint used to catch stale neighborhood views after
        // the underlying solution has been modified, including direct changes
        // that bypass make_move().
        std::uint64_t signature = 1469598103934665603ULL;

        for (const auto machine : solution.assignment)
        {
            signature ^= static_cast<std::uint64_t>(machine) +
                         0x9e3779b97f4a7c15ULL;
            signature *= 1099511628211ULL;
        }

        signature ^= static_cast<std::uint64_t>(solution.assignment.size());
        return signature;
    }
#endif

    const SolutionManager& solution_manager_;
};

} // namespace easylocal::mwe::assignment
