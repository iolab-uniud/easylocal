#pragma once

#include "detail/random_ordinals_view.hpp"
#include "move.hpp"
#include <easylocal/sampling.hpp>
#include "solution_manager.hpp"

#include <easylocal/cursor_moves.hpp>
#include <easylocal/service_base.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <ranges>
#include <utility>

namespace easylocal::mwe::assignment
{

class ReassignJobNeighborhoodExplorer
    : public easylocal::neighborhood_explorer_base<
          AssignmentSolutionManager,
          ReassignJobMove>
{
private:
    // Ordinal decoding is retained only for random without-replacement
    // traversal; deterministic authoring uses first_move/next_move.
    template<std::ranges::viewable_range Ordinals>
    [[nodiscard]]
    auto decode_moves(
        const AssignmentSolution& solution,
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
                           "neighborhood range invalidated by AssignmentSolution mutation");
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
    using neighborhood_explorer_base::neighborhood_explorer_base;
    using random_sampling = easylocal::sampling::without_replacement;

    [[nodiscard]]
    auto is_valid(
        const AssignmentSolution& solution,
        const ReassignJobMove& move) const noexcept -> bool
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
    auto moves(const AssignmentSolution& solution) const
    {
        assert(solution_manager_.is_valid(solution));

#ifndef NDEBUG
        const auto expected_signature = debug_signature(solution);

        return easylocal::cursor_moves(*this, solution)
             | std::views::transform(
                   [&solution, expected_signature](const ReassignJobMove& move) {
                       assert(
                           debug_signature(solution) == expected_signature &&
                           "neighborhood range invalidated by AssignmentSolution mutation");
                       return move;
                   });
#else
        return easylocal::cursor_moves(*this, solution);
#endif
    }

    [[nodiscard]]
    auto first_move(
        const AssignmentSolution& solution,
        ReassignJobMove& move) const noexcept -> bool
    {
        assert(solution_manager_.is_valid(solution));

        const auto machine_count =
            solution_manager_.instance().capacity.size();

        if (solution.assignment.empty() || machine_count < 2)
        {
            return false;
        }

        move.job = 0;
        move.destination = first_destination(solution, move.job);
        return true;
    }

    [[nodiscard]]
    auto next_move(
        const AssignmentSolution& solution,
        ReassignJobMove& move) const noexcept -> bool
    {
        assert(solution_manager_.is_valid(solution));
        assert(is_valid(solution, move));

        const auto machine_count =
            solution_manager_.instance().capacity.size();
        const auto current_machine = solution.assignment[move.job];

        for (auto destination = move.destination + 1;
             destination < machine_count;
             ++destination)
        {
            if (destination != current_machine)
            {
                move.destination = destination;
                return true;
            }
        }

        for (auto job = move.job + 1;
             job < solution.assignment.size();
             ++job)
        {
            move.job = job;
            move.destination = first_destination(solution, job);
            return true;
        }

        return false;
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto random_move(
        const AssignmentSolution& solution,
        RNG& rng) const -> std::optional<ReassignJobMove>
    {
        assert(solution_manager_.is_valid(solution));
        const auto count = move_count(solution);
        if (count == 0)
        {
            return std::nullopt;
        }

        std::uniform_int_distribution<std::size_t> draw{0, count - 1};
        return move_at(solution, draw(rng));
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto random_moves(
        const AssignmentSolution& solution,
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
        AssignmentSolution& solution,
        const ReassignJobMove& move) const noexcept
    {
        assert(is_valid(solution, move));
        solution.assignment[move.job] = move.destination;
    }

private:
    [[nodiscard]]
    auto first_destination(
        const AssignmentSolution& solution,
        const std::size_t job) const noexcept -> machine_id
    {
        const auto machine_count =
            solution_manager_.instance().capacity.size();
        assert(machine_count >= 2);
        assert(job < solution.assignment.size());

        return solution.assignment[job] == 0
            ? machine_id{1}
            : machine_id{0};
    }

    [[nodiscard]]
    auto move_count(const AssignmentSolution& solution) const noexcept
        -> std::size_t
    {
        const auto machine_count =
            solution_manager_.instance().capacity.size();
        const auto alternatives_per_job =
            machine_count > 0 ? machine_count - 1 : std::size_t{0};

        return solution.assignment.size() * alternatives_per_job;
    }

    [[nodiscard]]
    auto move_at(
        const AssignmentSolution& solution,
        const std::size_t ordinal) const noexcept -> ReassignJobMove
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

        return ReassignJobMove{
            .job = job,
            .destination = destination,
        };
    }

#ifndef NDEBUG
    [[nodiscard]]
    static auto debug_signature(const AssignmentSolution& solution) noexcept
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

};

} // namespace easylocal::mwe::assignment
