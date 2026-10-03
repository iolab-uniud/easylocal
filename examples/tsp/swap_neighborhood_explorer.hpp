#pragma once

#include "solution_manager.hpp"
#include "swap_move.hpp"

#include <easylocal/helpers/neighborhood_explorer.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <random>
#include <string_view>
#include <utility>

namespace tsp
{

class SwapCitiesNeighborhoodExplorer
    : public easylocal::neighborhood_explorer_base<TspSolutionManager, SwapCitiesMove>
{
private:
    static constexpr std::size_t move_count(std::size_t city_count)
    {
        return city_count >= 2 ? city_count * (city_count - 1) / 2 : std::size_t{0};
    }

public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    static constexpr std::string_view name()
    {
        return "Swap cities";
    }

    bool is_valid(const Tour& solution, const SwapCitiesMove& move) const
    {
        return move.first_position < move.second_position
            && move.second_position < solution.tour.size();
    }

    bool first_move(const Tour& solution, SwapCitiesMove& move) const
    {
        return find_from(solution.tour.size(), 0, 1, move);
    }

    bool next_move(const Tour& solution, SwapCitiesMove& move) const
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
    std::optional<SwapCitiesMove> random_move(const Tour& solution, RNG& rng) const
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

    void make_move(Tour& solution, const SwapCitiesMove& move) const
    {
        std::swap(
            solution.tour[move.first_position],
            solution.tour[move.second_position]);
    }

private:
    static bool find_from(
        std::size_t city_count,
        std::size_t initial_first,
        std::size_t initial_second,
        SwapCitiesMove& move)
    {
        for (auto first = initial_first; first < city_count; ++first)
        {
            const auto second = first == initial_first ? initial_second : first + 1;

            if (second < city_count)
            {
                move = SwapCitiesMove{
                    .first_position = first,
                    .second_position = second,
                };
                return true;
            }
        }

        return false;
    }
};

} // namespace tsp
