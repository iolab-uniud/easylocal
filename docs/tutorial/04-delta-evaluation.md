# 4. Delta evaluation

A 2-opt move removes two edges and adds two. A **delta evaluator** computes the
change of the tour length from those four edges, without building the new tour:

<!-- snippet: tutorial/tsp.hpp:delta -->
```cpp
class TwoOptLengthDelta
{
public:
    explicit TwoOptLengthDelta(const Tsp& tsp) : tsp_{tsp} {}

    [[nodiscard]] auto delta_evaluate(const Tour& tour, const TwoOpt& move) const
        -> double
    {
        const auto n = tour.order.size();
        const auto a = tour.order[move.i];
        const auto b = tour.order[move.i + 1];
        const auto c = tour.order[move.j];
        const auto d = tour.order[(move.j + 1) % n];
        return tsp_.d(a, c) + tsp_.d(b, d) - tsp_.d(a, b) - tsp_.d(c, d);
    }

private:
    const Tsp& tsp_;
};
```

It is bound to the component in the neighborhood recipe:

```cpp
auto nhe = el::neighborhood<TwoOptExplorer>()
         | el::delta<TourLength, TwoOptLengthDelta>();
```

- The contract is algebraic: `Value + Delta -> Value`. With an arithmetic value
  the delta can have the same type; with a domain value, define
  `operator+(Value, Delta)`.
- Deltas are **per component and per neighborhood**. The cost of a move is
  always recomputed by the cost expression from the updated component values,
  so a delta never deals with weights or hierarchical structure.
- Coverage can be partial: components without a delta for a neighborhood are
  re-evaluated on a candidate solution. When every component has one, no
  candidate solution is built.

> **Choice: separate or co-located.** A delta evaluator can be a separate class,
> as here, or a `delta_evaluate(solution, move)` member of the component itself,
> attached with `delta<TourLength>()`. Separate evaluators keep a component
> reusable across neighborhoods; co-located ones are shorter when a component
> serves a single neighborhood.

Chapter 10 shows how to check that a delta agrees with the full evaluation.

## See also

- [Cost](../reference/cost.md#delta-evaluators): the delta contract.
- [NeighborhoodExplorer](../reference/neighborhood-explorer.md): how deltas are
  bound.

## Next steps

[Chapter 5](05-running-a-search.md) runs the search.
