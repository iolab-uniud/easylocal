# 4. Delta evaluation

So far, evaluating a move means copying the tour, applying the move and
recomputing its length. A **delta cost component** computes the change
directly from the affected edges. For 2-opt, this reduces evaluation from a
pass over the tour to four distance lookups.

We add a delta for 2-opt because it always replaces two edges. Swap deltas
need more care: adjacent or nearby positions can share affected edges,
including across the end of the tour. We leave swaps on full evaluation.

## The delta cost component

The change of the tour length under a 2-opt move needs only the four cities
`a`, `b`, `c` and `d`:

<!-- snippet: tutorial/tsp.hpp:delta -->
```cpp
class TwoOptLengthDelta : public easylocal::input_base<Tsp>
{
public:
    using input_base::input_base;

    // The tour goes a -> b ... c -> d; after the move it goes a -> c ... b -> d,
    // the segment b ... c reversed, at the same cost with symmetric distances.
    double delta_evaluate(const Tour& tour, const TwoOpt& move) const
    {
        const auto n = tour.order.size();
        const auto a = tour.order[move.i];
        const auto b = tour.order[move.i + 1];
        const auto c = tour.order[move.j];
        const auto d = tour.order[(move.j + 1) % n];
        const auto& distance = input().distance;
        return distance[a][c] + distance[b][d] - distance[a][b] - distance[c][d];
    }
};
```

It is bound to the component in the neighborhood recipe:

<!-- snippet: tutorial/main.cpp:nhe-recipe -->
```cpp
auto nhe =
    el::neighborhood<TwoOptExplorer>() | el::delta<TourLength, TwoOptLengthDelta>();
```

- The contract is algebraic: `Value + Delta -> Value`. With an arithmetic value
  the delta can have the same type; with a domain value, define
  `operator+(Value, Delta)`.
- With floating-point values the law holds up to rounding: the length of a
  tour with decimal distances, updated by deltas, drifts from its full
  evaluation in the last bits. The checks of chapters 10 and 14 forgive it,
  within a tolerance; the search compares the costs exactly, unless the cost
  expression is `cost::approximately(...)` (chapter 2).
- Deltas are **per component and per neighborhood**. The cost of a move is
  always recomputed by the cost expression from the updated component values,
  so a delta never deals with weights or hierarchical structure.
- Coverage can be partial: components without a delta for a neighborhood are
  re-evaluated on a candidate solution. When every component has one, no
  candidate solution is built.

> **Write a delta when it saves work.** If computing the change requires
> scanning the whole solution, full evaluation is often just as cheap.
> Leave that component without a delta and let EasyLocal evaluate the
> candidate. The Assignment and Exam Timetabling examples use this fallback
> to avoid code that adds no speed benefit.

## Choice: separate or co-located

A delta cost component can be a separate class, as `TwoOptLengthDelta` above, or a
`delta_evaluate(solution, move)` member of the component itself. The
co-located version of the tour length puts the two computations in one class:

<!-- snippet: tutorial/tsp.hpp:co-located -->
```cpp
// The tour length with its 2-opt delta in the same class: a co-located delta.
// The component is attached as usual, with component<TourLengthWithDelta>(),
// and its delta with delta<TourLengthWithDelta>().
class TourLengthWithDelta : public easylocal::input_base<Tsp>
{
public:
    using input_base::input_base;

    double evaluate(const Tour& tour) const
    {
        const auto n = tour.order.size();
        double length = 0.0;
        for (std::size_t k = 0; k < n; ++k)
            length += input().distance[tour.order[k]][tour.order[(k + 1) % n]];
        return length;
    }

    double delta_evaluate(const Tour& tour, const TwoOpt& move) const
    {
        const auto n = tour.order.size();
        const auto a = tour.order[move.i];
        const auto b = tour.order[move.i + 1];
        const auto c = tour.order[move.j];
        const auto d = tour.order[(move.j + 1) % n];
        const auto& distance = input().distance;
        return distance[a][c] + distance[b][d] - distance[a][b] - distance[c][d];
    }
};
```

The component is attached as before, and `delta` names only the component,
which is also its delta cost component:

<!-- snippet: tutorial/main.cpp:co-located-recipe -->
```cpp
auto colocated_sm =
    el::solution_manager<TourManager>() | el::component<TourLengthWithDelta>();
auto colocated_nhe =
    el::neighborhood<TwoOptExplorer>() | el::delta<TourLengthWithDelta>();
```

The two choices give the same search; they differ in how the code is
organised:

| | Separate | Co-located |
| --- | --- | --- |
| classes | the component, and one delta cost component per neighborhood | one |
| recipe | `delta<TourLength, TwoOptLengthDelta>()` | `delta<TourLengthWithDelta>()` |
| fits | a component reused by several neighborhoods, each with its own delta, or one without a delta | a component that serves a single neighborhood |

A separate delta keeps `TourLength` independent of `TwoOpt`. You can reuse the
component with any neighborhood and add deltas as needed.

The co-located version uses one class, but ties it to the move type. Its
`evaluate` and `delta_evaluate` members share an object, so they can also share
precomputed data. The rest of this tutorial uses the separate version.

Chapter 10 shows how to check that a delta agrees with the full evaluation.

## Checking the deltas during a run

A delta bug may appear only after many moves. To check the solutions a search
actually visits, compile with `-DEASYLOCAL_VERIFY_DELTAS` or set the definition
through `target_compile_definitions`. After every committed move, the search
compares each component's incremental value with its full evaluation. The
first disagreement stops the program and identifies the component:

```text
EASYLOCAL_VERIFY_DELTAS: the delta of cost component #1 disagrees with its full evaluation after a move
```

Floating-point values are compared with a relative tolerance of 1e-9.
This adds a full evaluation per accepted move, so enable it for debugging
and disable it for performance measurements.

## See also

- [Cost](../reference/cost.md#delta-cost-components): the delta contract.
- [NeighborhoodExplorer](../reference/neighborhood-explorer.md): how deltas are
  bound.

## Next steps

[Chapter 5](05-running-a-search.md) runs the search.
