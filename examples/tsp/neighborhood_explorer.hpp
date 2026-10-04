#pragma once

// The neighborhood explorer of the 2-opt moves.

#include "move.hpp"
#include "solution_manager.hpp"

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

class TwoOptNeighborhoodExplorer
    : public easylocal::neighborhood_explorer_base<TspSolutionManager, TwoOptMove>
{
private:
    static constexpr bool valid_edge_pair(
        std::size_t city_count,
        std::size_t first_edge,
        std::size_t second_edge)
    {
        return first_edge < second_edge && second_edge < city_count
            && second_edge != first_edge + 1
            && !(first_edge == 0 && second_edge + 1 == city_count);
    }

public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    static constexpr std::string_view name()
    {
        return "2-opt";
    }

    bool is_valid(const Tour& solution, const TwoOptMove& move) const
    {
        return valid_edge_pair(solution.tour.size(), move.first_edge, move.second_edge);
    }

    // Every valid pair of edges i < j, in lexicographic order.
    easylocal::generator<TwoOptMove> moves(const Tour& solution) const
    {
        const auto n = solution.tour.size();
        for (std::size_t i = 0; i < n; ++i)
            for (auto j = i + 2; j < n; ++j)
                if (valid_edge_pair(n, i, j))
                    co_yield TwoOptMove{i, j};
    }

    // A uniform 2-opt move in expected O(1): two edges drawn independently,
    // ordered, and drawn again while they do not form a valid move (the same
    // edge, adjacent edges, or the first and the last edge). Every valid pair
    // is equally likely; with n cities about 3 draws in n are rejected.
    template<std::uniform_random_bit_generator RNG>
    std::optional<TwoOptMove> random_move(const Tour& solution, RNG& rng) const
    {
        const auto city_count = solution.tour.size();
        if (city_count < 4)
            return std::nullopt;

        std::uniform_int_distribution<std::size_t> draw{0, city_count - 1};
        while (true)
        {
            auto first_edge = draw(rng);
            auto second_edge = draw(rng);
            if (second_edge < first_edge)
                std::swap(first_edge, second_edge);
            if (valid_edge_pair(city_count, first_edge, second_edge))
            {
                return TwoOptMove{
                    .first_edge = first_edge,
                    .second_edge = second_edge,
                };
            }
        }
    }

    void make_move(Tour& solution, const TwoOptMove& move) const
    {
        const auto first = static_cast<std::ptrdiff_t>(move.first_edge + 1);
        const auto last = static_cast<std::ptrdiff_t>(move.second_edge + 1);
        std::reverse(solution.tour.begin() + first, solution.tour.begin() + last);
    }
};

} // namespace tsp
