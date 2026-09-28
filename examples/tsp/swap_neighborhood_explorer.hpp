#pragma once

#include "solution_manager.hpp"
#include "swap_move.hpp"

#include <easylocal/service_base.hpp>

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

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto random_move(
        const Tour& solution,
        RNG& rng) const -> std::optional<SwapCitiesMove>
    {
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
