#pragma once

#include "solution_manager.hpp"
#include "swap_move.hpp"

#include <easylocal/cursor_moves.hpp>
#include <easylocal/service_base.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <ranges>

namespace easylocal::mwe::tsp
{

class SwapCitiesNeighborhoodExplorer
    : public easylocal::neighborhood_explorer_base<
          TspSolutionManager,
          SwapCitiesMove>
{
private:
    [[nodiscard]]
    static constexpr auto move_count(const std::size_t city_count) noexcept
        -> std::size_t
    {
        return city_count >= 2
            ? city_count * (city_count - 1) / 2
            : std::size_t{0};
    }

    [[nodiscard]]
    static auto move_at_rank(
        const std::size_t city_count,
        const std::size_t rank) noexcept -> SwapCitiesMove
    {
        assert(rank < move_count(city_count));

        std::size_t current_rank = 0;

        for (std::size_t first = 0; first < city_count; ++first)
        {
            for (std::size_t second = first + 1; second < city_count; ++second)
            {
                if (current_rank == rank)
                {
                    return SwapCitiesMove{
                        .first_position = first,
                        .second_position = second,
                    };
                }

                ++current_rank;
            }
        }

        assert(false && "swap move rank must decode to a valid move");
        return {};
    }

#ifndef NDEBUG
    [[nodiscard]]
    static auto debug_signature(const Tour& solution) noexcept -> std::uint64_t
    {
        std::uint64_t signature = 1469598103934665603ULL;

        for (const auto city : solution.tour)
        {
            signature ^= static_cast<std::uint64_t>(city) +
                         0x9e3779b97f4a7c15ULL;
            signature *= 1099511628211ULL;
        }

        signature ^= static_cast<std::uint64_t>(solution.tour.size());
        return signature;
    }
#endif

public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]]
    auto is_valid(
        const Tour& solution,
        const SwapCitiesMove& move) const noexcept -> bool
    {
        return solution_manager_.is_valid(solution) &&
               move.first_position < move.second_position &&
               move.second_position < solution.tour.size();
    }

    [[nodiscard]]
    auto moves(const Tour& solution) const
    {
        assert(solution_manager_.is_valid(solution));

#ifndef NDEBUG
        const auto expected_signature = debug_signature(solution);

        return easylocal::cursor_moves(*this, solution)
             | std::views::transform(
                   [&solution, expected_signature](const SwapCitiesMove& move) {
                       assert(
                           debug_signature(solution) == expected_signature &&
                           "neighborhood range invalidated by Tour mutation");
                       return move;
                   });
#else
        return easylocal::cursor_moves(*this, solution);
#endif
    }

    [[nodiscard]]
    auto first_move(
        const Tour& solution,
        SwapCitiesMove& move) const noexcept -> bool
    {
        assert(solution_manager_.is_valid(solution));
        return find_from(solution.tour.size(), 0, 1, move);
    }

    [[nodiscard]]
    auto next_move(
        const Tour& solution,
        SwapCitiesMove& move) const noexcept -> bool
    {
        assert(solution_manager_.is_valid(solution));
        assert(is_valid(solution, move));

        return find_from(
            solution.tour.size(),
            move.first_position,
            move.second_position + 1,
            move);
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto random_move(
        const Tour& solution,
        RNG& rng) const -> std::optional<SwapCitiesMove>
    {
        assert(solution_manager_.is_valid(solution));
        const auto count = move_count(solution.tour.size());
        if (count == 0)
        {
            return std::nullopt;
        }

        std::uniform_int_distribution<std::size_t> draw{0, count - 1};
        return move_at_rank(solution.tour.size(), draw(rng));
    }

    void make_move(
        Tour& solution,
        const SwapCitiesMove& move) const noexcept
    {
        assert(is_valid(solution, move));
        std::swap(
            solution.tour[move.first_position],
            solution.tour[move.second_position]);
    }

private:
    [[nodiscard]]
    static auto find_from(
        const std::size_t city_count,
        const std::size_t initial_first,
        const std::size_t initial_second,
        SwapCitiesMove& move) noexcept -> bool
    {
        for (auto first = initial_first; first < city_count; ++first)
        {
            const auto second_begin = first == initial_first
                ? initial_second
                : first + 1;

            for (auto second = second_begin; second < city_count; ++second)
            {
                if (first < second)
                {
                    move = SwapCitiesMove{
                        .first_position = first,
                        .second_position = second,
                    };
                    return true;
                }
            }
        }

        return false;
    }
};

} // namespace easylocal::mwe::tsp
