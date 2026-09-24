#pragma once

#include "move.hpp"
#include "solution_manager.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ranges>

namespace easylocal::mwe::tsp
{

class NeighborhoodExplorer
{
public:
    using instance_type = Instance;
    using solution_type = Solution;
    using move_type = TwoOptMove;

    explicit NeighborhoodExplorer(const SolutionManager& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const Instance&
    {
        return solution_manager_.instance();
    }

    [[nodiscard]]
    auto is_valid(
        const Solution& solution,
        const TwoOptMove& move) const noexcept -> bool
    {
        if (!solution_manager_.is_valid(solution))
        {
            return false;
        }

        return valid_edge_pair(
            solution.tour.size(),
            move.first_edge,
            move.second_edge);
    }

    [[nodiscard]]
    auto moves(const Solution& solution) const
    {
        assert(solution_manager_.is_valid(solution));

        const auto city_count = solution.tour.size();
#ifndef NDEBUG
        const auto expected_signature = debug_signature(solution);
#endif

        auto ordinals =
            std::views::iota(std::size_t{0}, city_count * city_count)
            | std::views::filter([city_count](const std::size_t ordinal) {
                  const auto first_edge = ordinal / city_count;
                  const auto second_edge = ordinal % city_count;
                  return valid_edge_pair(
                      city_count,
                      first_edge,
                      second_edge);
              });

#ifndef NDEBUG
        return ordinals
             | std::views::transform(
                   [&solution, city_count, expected_signature](
                       const std::size_t ordinal) {
                       assert(
                           debug_signature(solution) == expected_signature &&
                           "neighborhood range invalidated by Solution mutation");
                       return decode_move(city_count, ordinal);
                   });
#else
        return ordinals
             | std::views::transform(
                   [city_count](const std::size_t ordinal) {
                       return decode_move(city_count, ordinal);
                   });
#endif
    }

    void make_move(
        Solution& solution,
        const TwoOptMove& move) const noexcept
    {
        assert(is_valid(solution, move));

        const auto first = static_cast<std::ptrdiff_t>(move.first_edge + 1);
        const auto last = static_cast<std::ptrdiff_t>(move.second_edge + 1);
        std::reverse(
            solution.tour.begin() + first,
            solution.tour.begin() + last);
    }

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

    [[nodiscard]]
    static constexpr auto decode_move(
        const std::size_t city_count,
        const std::size_t ordinal) noexcept -> TwoOptMove
    {
        assert(city_count != 0);
        return TwoOptMove{
            .first_edge = ordinal / city_count,
            .second_edge = ordinal % city_count,
        };
    }

#ifndef NDEBUG
    [[nodiscard]]
    static auto debug_signature(const Solution& solution) noexcept
        -> std::uint64_t
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

    const SolutionManager& solution_manager_;
};

} // namespace easylocal::mwe::tsp
