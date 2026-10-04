# 6. Combining neighborhoods

The tour has two neighborhoods, both from chapter 3: the swap moves, listed by
a generator and without a delta cost component, and the 2-opt moves, listed by a
cursor and with the delta cost component of chapter 4. A search can use both. When
it evaluates a swap, `TourLength` is re-evaluated on a candidate tour; when it
evaluates a 2-opt move, `TwoOptLengthDelta` is used.

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
