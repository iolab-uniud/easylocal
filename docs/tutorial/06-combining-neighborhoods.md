# 6. Combining neighborhoods

A second neighborhood swaps two cities. It has no delta evaluator, so its moves
are evaluated by re-evaluating `TourLength` on a candidate tour:

<!-- snippet: tutorial/tsp.hpp:swap -->
```cpp
struct Swap
{
    std::size_t first;
    std::size_t second;
};

// A second neighborhood, without a delta evaluator: its moves are evaluated by
// re-evaluating TourLength on a candidate solution.
class SwapExplorer : public easylocal::neighborhood_explorer_base<TourManager, Swap>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    easylocal::generator<Swap> moves(const Tour& tour) const
    {
        for (std::size_t first = 0; first < tour.order.size(); ++first)
            for (std::size_t second = first + 1; second < tour.order.size(); ++second)
                co_yield Swap{first, second};
    }

    template<std::uniform_random_bit_generator RNG>
    std::optional<Swap> random_move(const Tour& tour, RNG& rng) const
    {
        if (tour.order.size() < 2)
            return std::nullopt;
        std::uniform_int_distribution<std::size_t> pick{0, tour.order.size() - 1};
        const auto first = pick(rng);
        auto second = pick(rng);
        while (second == first)
            second = pick(rng);
        return Swap{std::min(first, second), std::max(first, second)};
    }

    bool is_valid(const Tour& tour, const Swap& move) const
    {
        return move.first < move.second && move.second < tour.order.size();
    }

    void make_move(Tour& tour, const Swap& move) const
    {
        std::swap(tour.order[move.first], tour.order[move.second]);
    }
};
```

`neighborhood_union` combines the two explorers into one; each child keeps its
own move type and delta bindings:

<!-- snippet: tutorial/main.cpp:union -->
```cpp
auto both =
    el::neighborhood_union(
        el::neighborhood<TwoOptExplorer>()
            | el::delta<TourLength, TwoOptLengthDelta>(),
        el::neighborhood<SwapExplorer>())
    | el::random_biases(3.0, 1.0);

auto union_sa =
    el::make_runner<runners::SimulatedAnnealing<Classic>>(
        Classic{runners::temperature::ClassicParameters{
            .initial_temperature = 10.0,
            .final_temperature = 0.1,
            .cooling_rate = 0.95,
            .samples_per_temperature = 50}})
    | sm | both;
```

- Deterministic algorithms enumerate the children in order.
- Random sampling picks a child according to the biases (here three 2-opt
  proposals for every swap), then asks it for a move. A zero bias disables a
  child; a child that cannot produce a move is excluded and another one is
  drawn. The biases are configurable through `NeighborhoodUnionParameters`.
- Unions nest, and traces report the route of every move through the nesting
  (chapter 15).

Algorithms need no change to use a union: it is just another neighborhood.

## See also

- [NeighborhoodExplorer](../reference/neighborhood-explorer.md#neighborhood-unions).

## Next steps

[Chapter 7](07-custom-runner.md) writes a new search algorithm.
