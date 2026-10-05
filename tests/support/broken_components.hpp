#pragma once

// Components of the tutorial's TSP with the mistakes the contract checks must
// catch: a delta that confuses positions with cities (right on the identity
// tour only), a make_move that takes the tour by value, random moves outside
// the neighborhood, a random_move that finds nothing, one that draws from its
// own generator, and hooks that throw.

#include "../../examples/tutorial/tsp.hpp"

#include <cstddef>
#include <optional>
#include <random>
#include <stdexcept>

namespace broken
{

using tutorial::Tour;
using tutorial::TourManager;
using tutorial::Tsp;
using tutorial::TwoOpt;

// The 2-opt delta with the positions of the move used as cities: on the
// identity tour, where order[k] == k, it is right.
class PositionsAsCitiesDelta
{
public:
    explicit PositionsAsCitiesDelta(const Tsp& input) : input_{input} {}

    double delta_evaluate(const Tour& tour, const TwoOpt& move) const
    {
        const auto n = tour.order.size();
        const auto a = move.i;
        const auto b = move.i + 1;
        const auto c = move.j;
        const auto d = (move.j + 1) % n;
        const auto& distance = input_.distance;
        return distance[a][c] + distance[b][d] - distance[a][b] - distance[c][d];
    }

private:
    const Tsp& input_;
};

// The explorers below are the tutorial's 2-opt with one hook changed.

// make_move takes the tour by value: it changes a copy.
class ByValueTwoOpt : public tutorial::TwoOptExplorer
{
public:
    using TwoOptExplorer::TwoOptExplorer;

    void make_move(Tour tour, const TwoOpt& move) const
    {
        TwoOptExplorer::make_move(tour, move);
    }
};

// random_move draws (0, n - 1), which is valid but which moves() skips.
class OutsideTwoOpt : public tutorial::TwoOptExplorer
{
public:
    using TwoOptExplorer::TwoOptExplorer;

    template<std::uniform_random_bit_generator RNG>
    std::optional<TwoOpt> random_move(const Tour& tour, RNG&) const
    {
        return TwoOpt{0, tour.order.size() - 1};
    }
};

// random_move finds nothing, although the neighborhood has moves.
class EmptyRandomTwoOpt : public tutorial::TwoOptExplorer
{
public:
    using TwoOptExplorer::TwoOptExplorer;

    template<std::uniform_random_bit_generator RNG>
    std::optional<TwoOpt> random_move(const Tour&, RNG&) const
    {
        return std::nullopt;
    }
};

// random_move draws from a generator of its own, not from the one given.
class OwnGeneratorTwoOpt : public tutorial::TwoOptExplorer
{
public:
    using TwoOptExplorer::TwoOptExplorer;

    template<std::uniform_random_bit_generator RNG>
    std::optional<TwoOpt> random_move(const Tour& tour, RNG&) const
    {
        static std::mt19937_64 own{12345};
        return TwoOptExplorer::random_move(tour, own);
    }
};

// make_move throws on the move (1, 3).
class ThrowingTwoOpt : public tutorial::TwoOptExplorer
{
public:
    using TwoOptExplorer::TwoOptExplorer;

    void make_move(Tour& tour, const TwoOpt& move) const
    {
        if (move.i == 1 && move.j == 3)
            throw std::runtime_error{"cannot apply this move"};
        TwoOptExplorer::make_move(tour, move);
    }
};

} // namespace broken
