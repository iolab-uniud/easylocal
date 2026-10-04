#pragma once

#include "solution_manager.hpp"
#include "swap_move.hpp"

#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/utils/generator.hpp>

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

    // Every pair of positions i < j, in lexicographic order.
    easylocal::generator<SwapCitiesMove> moves(const Tour& solution) const
    {
        const auto n = solution.tour.size();
        for (std::size_t i = 0; i < n; ++i)
            for (auto j = i + 1; j < n; ++j)
                co_yield SwapCitiesMove{i, j};
    }

    // A uniform swap in O(1): two distinct positions, the second drawn among
    // the others and ordered.
    template<std::uniform_random_bit_generator RNG>
    std::optional<SwapCitiesMove> random_move(const Tour& solution, RNG& rng) const
    {
        const auto city_count = solution.tour.size();
        if (city_count < 2)
            return std::nullopt;

        std::uniform_int_distribution<std::size_t> draw_first{0, city_count - 1};
        std::uniform_int_distribution<std::size_t> draw_other{0, city_count - 2};
        const auto first = draw_first(rng);
        auto second = draw_other(rng);
        if (second >= first)
            ++second;
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
};

} // namespace tsp
