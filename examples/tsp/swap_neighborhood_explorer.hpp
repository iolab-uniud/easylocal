#pragma once

#include "solution_manager.hpp"
#include "swap_move.hpp"

#include <easylocal/helpers/neighborhood_explorer.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <random>
#include <string_view>

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

public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]]
    static constexpr auto name() noexcept -> std::string_view
    {
        return "Swap cities";
    }

    [[nodiscard]]
    auto is_valid(
        const Tour& solution,
        const SwapCitiesMove& move) const noexcept -> bool
    {
        return move.first_position < move.second_position &&
               move.second_position < solution.tour.size();
    }

    [[nodiscard]]
    auto first_move(
        const Tour& solution,
        SwapCitiesMove& move) const noexcept -> bool
    {
        return find_from(solution.tour.size(), 0, 1, move);
    }

    [[nodiscard]]
    auto next_move(
        const Tour& solution,
        SwapCitiesMove& move) const noexcept -> bool
    {

        return find_from(
            solution.tour.size(),
            move.first_position,
            move.second_position + 1,
            move);
    }

    // A uniform swap in O(1): two distinct positions, the second drawn among
    // the others and ordered.
    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto random_move(
        const Tour& solution,
        RNG& rng) const -> std::optional<SwapCitiesMove>
    {
        const auto city_count = solution.tour.size();
        if (move_count(city_count) == 0)
        {
            return std::nullopt;
        }

        std::uniform_int_distribution<std::size_t> draw_first{0, city_count - 1};
        std::uniform_int_distribution<std::size_t> draw_other{0, city_count - 2};
        const auto first = draw_first(rng);
        auto second = draw_other(rng);
        if (second >= first)
        {
            ++second;
        }
        return SwapCitiesMove{
            .first_position = std::min(first, second),
            .second_position = std::max(first, second),
        };
    }

    void make_move(
        Tour& solution,
        const SwapCitiesMove& move) const noexcept
    {
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
