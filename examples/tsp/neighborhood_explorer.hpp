#pragma once

#include "move.hpp"
#include "solution_manager.hpp"

#include <easylocal/helpers/neighborhood_explorer.hpp>

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

    static constexpr std::size_t move_count(std::size_t city_count)
    {
        return city_count >= 4 ? city_count * (city_count - 3) / 2 : std::size_t{0};
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

    bool first_move(const Tour& solution, TwoOptMove& move) const
    {
        return find_from(solution.tour.size(), 0, 1, move);
    }

    bool next_move(const Tour& solution, TwoOptMove& move) const
    {
        return find_from(
            solution.tour.size(),
            move.first_edge,
            move.second_edge + 1,
            move);
    }

    // A uniform 2-opt move in expected O(1): two edges drawn independently,
    // ordered, and drawn again while they do not form a valid move (the same
    // edge, adjacent edges, or the first and the last edge). Every valid pair
    // is equally likely; with n cities about 3 draws in n are rejected.
    template<std::uniform_random_bit_generator RNG>
    std::optional<TwoOptMove> random_move(const Tour& solution, RNG& rng) const
    {
        const auto city_count = solution.tour.size();
        if (move_count(city_count) == 0)
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

private:
    static bool find_from(
        std::size_t city_count,
        std::size_t initial_first_edge,
        std::size_t initial_second_edge,
        TwoOptMove& move)
    {
        for (auto first_edge = initial_first_edge; first_edge < city_count; ++first_edge)
        {
            const auto second_begin =
                first_edge == initial_first_edge ? initial_second_edge : first_edge + 1;

            for (auto second_edge = second_begin; second_edge < city_count; ++second_edge)
            {
                if (valid_edge_pair(city_count, first_edge, second_edge))
                {
                    move = TwoOptMove{
                        .first_edge = first_edge,
                        .second_edge = second_edge,
                    };
                    return true;
                }
            }
        }

        return false;
    }
};

} // namespace tsp
