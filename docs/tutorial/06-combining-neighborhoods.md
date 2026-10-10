# 6. Combining neighborhoods

We now have two ways to change a tour: swaps and 2-opt. Combining them lets a
search try improvements that either neighborhood alone might miss.

`neighborhood_union` combines the explorers without changing either one.
Swaps keep their generator, 2-opt keeps its cursor, and each keeps its own
move type:

<!-- snippet: tutorial/main.cpp:union -->
```cpp
auto both =
    el::neighborhood_union(
        el::neighborhood<TwoOptExplorer>()
            | el::delta<TourLength, TwoOptLengthDelta>(),
        el::neighborhood<SwapExplorer>())
    | el::random_biases(3.0, 1.0);

auto union_sa =
    el::make_runner<runners::SimulatedAnnealing<Classic>>({
        .temperature =
            {
                .initial_temperature = 10.0,
                .final_temperature = 0.1,
                .cooling_rate = 0.95,
                .samples_per_temperature = 50,
            },
    })
    | sm | both;
```

- Deterministic algorithms enumerate the children in order, each in its own
  way: the swap moves from the generator, the 2-opt moves from the cursor.
- Delta evaluation requires a delta for the component in every child. Swaps
  have none for `TourLength`, so this union evaluates both kinds of move on a
  candidate tour. Add a swap delta to let the union use `TwoOptLengthDelta`
  as well.
- Random sampling selects a child according to its bias, then asks for a move.
  Here 2-opt is chosen with probability 3/4. A zero bias excludes a child from
  random selection; enumeration still includes it. If a child has no move,
  the union excludes it and tries another. Configure the biases through
  `NeighborhoodUnionParameters`.
- Unions nest, and traces report the route of every move through the nesting
  (chapter 16).

Algorithms need no change to use a union: it is just another neighborhood.

## See also

- [NeighborhoodExplorer](../reference/neighborhood-explorer.md#neighborhood-unions).

## Next steps

[Chapter 7](07-custom-runner.md) writes a new search algorithm.
