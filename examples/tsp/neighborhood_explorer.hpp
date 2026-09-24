#pragma once

#include "move.hpp"
#include "sampling.hpp"
#include "solution_manager.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <random>
#include <ranges>

namespace easylocal::mwe::tsp
{

class NeighborhoodExplorer
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

    [[nodiscard]]
    static constexpr auto move_count(const std::size_t city_count) noexcept
        -> std::size_t
    {
        return city_count >= 4
            ? city_count * (city_count - 3) / 2
            : std::size_t{0};
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

    template<std::uniform_random_bit_generator RNG>
    class random_moves_view
        : public std::ranges::view_interface<random_moves_view<RNG>>
    {
    public:
        random_moves_view() = default;

        random_moves_view(
            const Solution& solution,
            RNG& rng) noexcept
            : solution_{&solution},
              rng_{&rng},
              city_count_{solution.tour.size()},
              move_count_{NeighborhoodExplorer::move_count(city_count_)}
#ifndef NDEBUG
              , expected_signature_{debug_signature(solution)}
#endif
        {
        }

        class iterator
        {
        public:
            using iterator_concept = std::input_iterator_tag;
            using value_type = TwoOptMove;
            using difference_type = std::ptrdiff_t;

            iterator() = default;

            iterator(
                const Solution& solution,
                RNG& rng,
                const std::size_t city_count,
                const std::size_t move_count
#ifndef NDEBUG
                , const std::uint64_t expected_signature
#endif
                )
                : solution_{&solution},
                  rng_{&rng},
                  city_count_{city_count},
                  move_count_{move_count}
#ifndef NDEBUG
                  , expected_signature_{expected_signature}
#endif
            {
                if (move_count_ != 0)
                {
                    draw();
                }
            }

            [[nodiscard]]
            auto operator*() const noexcept -> value_type
            {
#ifndef NDEBUG
                assert(
                    debug_signature(*solution_) == expected_signature_ &&
                    "neighborhood range invalidated by Solution mutation");
#endif
                return current_;
            }

            auto operator++() -> iterator&
            {
#ifndef NDEBUG
                assert(
                    debug_signature(*solution_) == expected_signature_ &&
                    "neighborhood range invalidated by Solution mutation");
#endif
                draw();
                return *this;
            }

            void operator++(int)
            {
                ++*this;
            }

            friend auto operator==(
                const iterator& current,
                std::default_sentinel_t) noexcept -> bool
            {
                return current.move_count_ == 0;
            }

        private:
            void draw()
            {
                assert(move_count_ != 0);
                std::uniform_int_distribution<std::size_t> distribution{
                    0,
                    move_count_ - 1,
                };
                current_ = NeighborhoodExplorer::move_at_rank(
                    city_count_,
                    distribution(*rng_));
            }

            const Solution* solution_{nullptr};
            RNG* rng_{nullptr};
            std::size_t city_count_{0};
            std::size_t move_count_{0};
            TwoOptMove current_{};
#ifndef NDEBUG
            std::uint64_t expected_signature_{0};
#endif
        };

        [[nodiscard]]
        auto begin() -> iterator
        {
            return iterator{
                *solution_,
                *rng_,
                city_count_,
                move_count_
#ifndef NDEBUG
                , expected_signature_
#endif
            };
        }

        [[nodiscard]]
        auto end() const noexcept -> std::default_sentinel_t
        {
            return {};
        }

        [[nodiscard]]
        auto empty() const noexcept -> bool
        {
            return move_count_ == 0;
        }

    private:
        const Solution* solution_{nullptr};
        RNG* rng_{nullptr};
        std::size_t city_count_{0};
        std::size_t move_count_{0};
#ifndef NDEBUG
        std::uint64_t expected_signature_{0};
#endif
    };

public:
    using instance_type = Instance;
    using solution_type = Solution;
    using move_type = TwoOptMove;
    using random_sampling = sampling::with_replacement;

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

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto random_moves(
        const Solution& solution,
        RNG& rng) const
    {
        assert(solution_manager_.is_valid(solution));
        return random_moves_view<RNG>{solution, rng};
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
    const SolutionManager& solution_manager_;
};

} // namespace easylocal::mwe::tsp
