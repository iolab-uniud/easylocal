# 3. Moves

The **NeighborhoodExplorer** owns *move semantics*: which moves exist, whether a
move is valid and how it changes a solution.

The *neighborhood* of a tour is the set of moves that can be applied to it.
For `SwapCities` it has one move for every pair of positions `i < j`:
n(n − 1)/2 moves, 10 for five cities.

<!-- snippet: tutorial/tsp.hpp:neighborhood -->
```cpp
class SwapExplorer : public easylocal::neighborhood_explorer_base<TourManager, SwapCities>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    // Every pair of positions i < j, one move at a time.
    easylocal::generator<SwapCities> moves(const Tour& tour) const
    {
        const auto n = tour.order.size();
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j)
                co_yield SwapCities{i, j};
    }

    // Uniform: two distinct positions, each pair equally likely, put in order.
    template<std::uniform_random_bit_generator RNG>
    std::optional<SwapCities> random_move(const Tour& tour, RNG& rng) const
    {
        const auto n = tour.order.size();
        if (n < 2)
            return std::nullopt;
        std::uniform_int_distribution<std::size_t> pick{0, n - 1};
        const auto i = pick(rng);
        auto j = pick(rng);
        while (j == i)
            j = pick(rng);
        return SwapCities{std::min(i, j), std::max(i, j)};
    }

    bool is_valid(const Tour& tour, const SwapCities& move) const
    {
        return move.i < move.j && move.j < tour.order.size();
    }

    void make_move(Tour& tour, const SwapCities& move) const
    {
        std::swap(tour.order[move.i], tour.order[move.j]);
    }
};
```

- `is_valid(solution, move)` and `make_move(solution, move)` are required.
  `is_valid` checks the move against an already valid solution: two distinct
  positions, in order, inside the tour. `make_move` changes the solution in
  place, here with one `std::swap`.
- `moves(solution)` enumerates the moves for *deterministic* algorithms such
  as First Improvement. Here it is a generator: each `co_yield` hands one move
  to the runner and pauses until the runner asks for the next one. Any input
  range whose elements convert to the move type works as well. Avoid filling a
  container: the whole neighborhood would be built even when the runner stops
  at its first move.
- `random_move(solution, rng)` samples one move for *stochastic* algorithms
  such as Simulated Annealing, returning `std::nullopt` when there is none. It
  need not be uniform, but should be cheap: it runs once per proposal. Here two
  positions are drawn, the second again until it differs from the first, and
  put in order; every pair has the same probability. Listing all the moves to
  pick one would cost a whole neighborhood per proposal.

`easylocal::neighborhood_explorer_base<SolutionManager, Move>` provides the
aliases and the SolutionManager reference; like the SolutionManager base, it is
optional and non-virtual.

> **Choice: range or cursor.** Instead of `moves`, an explorer may provide the
> EasyLocal 3 cursor `first_move(solution, move)` / `next_move(solution, move)`.
> It enumerates lazily as a generator does, without a coroutine frame, at the
> price of keeping the position in the move itself; when both exist, the cursor
> is used.

Attach the explorer with a recipe, `neighborhood<TwoOptExplorer>()`. In debug
builds the framework asserts that the solution and the move are valid before
every application, and that the solution is valid afterwards.

## See also

- [NeighborhoodExplorer](../reference/neighborhood-explorer.md): the full
  contract, cursors and unions.

## Next steps

Evaluating a move currently means applying it to a copy and re-evaluating the
whole tour. [Chapter 4](04-delta-evaluation.md) introduces a move whose effect
on the length is computed directly.
