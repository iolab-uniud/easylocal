# 3. Moves

The **NeighborhoodExplorer** owns *move semantics*: which moves exist, whether a
move is valid and how it changes a solution.

<!-- snippet: tutorial/tsp.hpp:neighborhood -->
```cpp
class TwoOptExplorer
    : public easylocal::neighborhood_explorer_base<TourManager, TwoOpt>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]] static auto name() -> std::string_view { return "2-opt"; }

    [[nodiscard]] auto moves(const Tour& tour) const -> std::vector<TwoOpt>
    {
        std::vector<TwoOpt> result;
        const auto n = tour.order.size();
        for (std::size_t i = 0; i + 2 < n; ++i)
        {
            for (std::size_t j = i + 2; j < n && !(i == 0 && j + 1 == n); ++j)
            {
                result.push_back({i, j});
            }
        }
        return result;
    }

    // Uniform by rejection: two positions drawn independently, ordered, and
    // drawn again while they are not a 2-opt move.
    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] auto random_move(const Tour& tour, RNG& rng) const
        -> std::optional<TwoOpt>
    {
        const auto n = tour.order.size();
        if (n < 4)
        {
            return std::nullopt;
        }
        std::uniform_int_distribution<std::size_t> pick{0, n - 1};
        while (true)
        {
            auto i = pick(rng);
            auto j = pick(rng);
            if (j < i)
            {
                std::swap(i, j);
            }
            if (i + 2 <= j && !(i == 0 && j + 1 == n))
            {
                return TwoOpt{i, j};
            }
        }
    }

    [[nodiscard]] auto is_valid(const Tour& tour, const TwoOpt& move) const -> bool
    {
        return move.i + 2 <= move.j && move.j < tour.order.size();
    }

    void make_move(Tour& tour, const TwoOpt& move) const
    {
        std::reverse(
            tour.order.begin() + static_cast<std::ptrdiff_t>(move.i + 1),
            tour.order.begin() + static_cast<std::ptrdiff_t>(move.j + 1));
    }
};
```

- `is_valid(solution, move)` and `make_move(solution, move)` are required.
  `is_valid` checks the move against an already valid solution.
- `moves(solution)` lists the moves for *deterministic* algorithms such as
  First Improvement. It may return any input range whose elements convert to
  the move type: a container, a view, a generator.
- `random_move(solution, rng)` samples one move for *stochastic* algorithms
  such as Simulated Annealing, returning `std::nullopt` when there is none. It
  need not be uniform, but should be cheap: it runs once per proposal. Drawing
  the move's components and drawing again while they are not a valid move, as
  here, is uniform over the valid moves and takes constant expected time when
  most draws are valid; listing all the moves to pick one would cost a whole
  neighborhood per proposal.
- `name()` is optional: it names the neighborhood in the interactive tester.

`easylocal::neighborhood_explorer_base<SolutionManager, Move>` provides the
aliases and the SolutionManager reference; like the SolutionManager base, it is
optional and non-virtual.

> **Choice: range or cursor.** Instead of `moves`, an explorer may provide the
> EasyLocal 3 cursor `first_move(solution, move)` / `next_move(solution, move)`.
> Use it when enumerating lazily is natural and you want to avoid materializing
> all moves; when both exist, the cursor is used.

Attach the explorer with a recipe, `neighborhood<TwoOptExplorer>()`. In debug
builds the framework asserts that the solution and the move are valid before
every application, and that the solution is valid afterwards.

## See also

- [NeighborhoodExplorer](../reference/neighborhood-explorer.md): the full
  contract, cursors and unions.

## Next steps

Evaluating a move currently means applying it to a copy and re-evaluating the
whole tour. [Chapter 4](04-delta-evaluation.md) does better.
