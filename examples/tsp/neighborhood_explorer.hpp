#pragma once

#include "move.hpp"
#include "solution_manager.hpp"

#include <easylocal/helpers/service_base.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <optional>
#include <random>
#include <string_view>

namespace easylocal::mwe::tsp
{

class TwoOptNeighborhoodExplorer
    : public easylocal::neighborhood_explorer_base<TspSolutionManager, TwoOptMove>
{
private:
    [[nodiscard]]
    static constexpr auto valid_edge_pair(
        const std::size_t city_count,
        const std::size_t first_edge,
        const std::size_t second_edge) noexcept -> bool
    {
        return first_edge < second_edge &&
               second_edge < city_count &&
               second_edge != first_edge + 1 &&
               !(first_edge == 0 && second_edge + 1 == city_count);
    }

    // Rank decoding supports uniform single-move proposals; deterministic
    // authoring uses first_move/next_move.
    [[nodiscard]]
    static constexpr auto move_count(const std::size_t city_count) noexcept
        -> std::size_t
    {
        return city_count >= 4
            ? city_count * (city_count - 3) / 2
            : std::size_t{0};
    }

    [[nodiscard]]
    static auto move_at_rank(
        const std::size_t city_count,
        const std::size_t rank) noexcept -> TwoOptMove
    {
        assert(rank < move_count(city_count));

        std::size_t current_rank = 0;

        for (std::size_t first_edge = 0;
             first_edge < city_count;
             ++first_edge)
        {
            for (std::size_t second_edge = first_edge + 1;
                 second_edge < city_count;
                 ++second_edge)
            {
                if (!valid_edge_pair(
                        city_count,
                        first_edge,
                        second_edge))
                {
                    continue;
                }

                if (current_rank == rank)
                {
                    return TwoOptMove{
                        .first_edge = first_edge,
                        .second_edge = second_edge,
                    };
                }

                ++current_rank;
            }
        }

        assert(false && "2-opt move rank must decode to a valid move");
        return {};
    }



public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]]
    static constexpr auto name() noexcept -> std::string_view
    {
        return "2-opt";
    }
    [[nodiscard]]
    auto is_valid(
        const Tour& solution,
        const TwoOptMove& move) const noexcept -> bool
    {
        return valid_edge_pair(
            solution.tour.size(),
            move.first_edge,
            move.second_edge);
    }

    [[nodiscard]]
    auto first_move(
        const Tour& solution,
        TwoOptMove& move) const noexcept -> bool
    {
        return find_from(solution.tour.size(), 0, 1, move);
    }

    [[nodiscard]]
    auto next_move(
        const Tour& solution,
        TwoOptMove& move) const noexcept -> bool
    {

        return find_from(
            solution.tour.size(),
            move.first_edge,
            move.second_edge + 1,
            move);
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto random_move(
        const Tour& solution,
        RNG& rng) const -> std::optional<TwoOptMove>
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
        const TwoOptMove& move) const noexcept
    {

        const auto first = static_cast<std::ptrdiff_t>(move.first_edge + 1);
        const auto last = static_cast<std::ptrdiff_t>(move.second_edge + 1);
        std::reverse(
            solution.tour.begin() + first,
            solution.tour.begin() + last);
    }

private:
    [[nodiscard]]
    static auto find_from(
        const std::size_t city_count,
        const std::size_t initial_first_edge,
        const std::size_t initial_second_edge,
        TwoOptMove& move) noexcept -> bool
    {
        for (auto first_edge = initial_first_edge;
             first_edge < city_count;
             ++first_edge)
        {
            const auto second_begin = first_edge == initial_first_edge
                ? initial_second_edge
                : first_edge + 1;

            for (auto second_edge = second_begin;
                 second_edge < city_count;
                 ++second_edge)
            {
                if (valid_edge_pair(
                        city_count,
                        first_edge,
                        second_edge))
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

} // namespace easylocal::mwe::tsp
