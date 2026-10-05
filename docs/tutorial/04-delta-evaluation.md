# 4. Delta evaluation

To evaluate a move without a delta cost component, EasyLocal applies it to a copy
of the tour and evaluates `TourLength` on the copy: a pass over the whole tour
for every move. A move changes only a few edges, though, and its effect on the
length could be computed from those edges alone. A **delta cost component** does
that.

For swap moves, such a delta is possible but fiddly: the edges around the two
positions overlap when the positions are adjacent or one apart, also across the
end of the tour, and each case needs its own formula. A 2-opt move always
changes exactly two edges (chapter 3), so its delta is one formula; the swap
moves stay without one.

## The delta cost component

The change of the tour length under a 2-opt move needs only the four cities
`a`, `b`, `c` and `d`:

<!-- snippet: tutorial/tsp.hpp:delta -->
```cpp
class TwoOptLengthDelta
{
public:
    explicit TwoOptLengthDelta(const Tsp& input) : input_{input} {}

    // The tour goes a -> b ... c -> d; after the move it goes a -> c ... b -> d,
    // the segment b ... c reversed, at the same cost with symmetric distances.
    double delta_evaluate(const Tour& tour, const TwoOpt& move) const
    {
        const auto n = tour.order.size();
        const auto a = tour.order[move.i];
        const auto b = tour.order[move.i + 1];
        const auto c = tour.order[move.j];
        const auto d = tour.order[(move.j + 1) % n];
        const auto& distance = input_.distance;
        return distance[a][c] + distance[b][d] - distance[a][b] - distance[c][d];
    }

private:
    const Tsp& input_;
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
  evaluation in the last bits. The checks of chapters 10 and 13 forgive it,
  within a tolerance; the search compares the costs exactly, unless the cost
  expression is `cost::approximately(...)` (chapter 2).
- Deltas are **per component and per neighborhood**. The cost of a move is
  always recomputed by the cost expression from the updated component values,
  so a delta never deals with weights or hierarchical structure.
- Coverage can be partial: components without a delta for a neighborhood are
  re-evaluated on a candidate solution. When every component has one, no
  candidate solution is built.

> **When not to write a delta.** A delta pays off when it looks at the part of
> the solution the move touches. If computing it means looking at the whole
> solution, for example recounting the load of every machine to know the load
> of two, it costs as much as a full evaluation: leave the component without a
> delta and let EasyLocal evaluate it on the candidate solution, with no code
> to write and check. The Assignment and Exam Timetabling examples do so.

## Choice: separate or co-located

A delta cost component can be a separate class, as `TwoOptLengthDelta` above, or a
`delta_evaluate(solution, move)` member of the component itself. The
co-located version of the tour length puts the two computations in one class:

<!-- snippet: tutorial/tsp.hpp:co-located -->
```cpp
// The tour length with its 2-opt delta in the same class: a co-located delta.
// The component is attached as usual, with component<TourLengthWithDelta>(),
// and its delta with delta<TourLengthWithDelta>().
class TourLengthWithDelta
{
public:
    explicit TourLengthWithDelta(const Tsp& input) : input_{input} {}

    double evaluate(const Tour& tour) const
    {
        const auto n = tour.order.size();
        double length = 0.0;
        for (std::size_t k = 0; k < n; ++k)
            length += input_.distance[tour.order[k]][tour.order[(k + 1) % n]];
        return length;
    }

    double delta_evaluate(const Tour& tour, const TwoOpt& move) const
    {
        const auto n = tour.order.size();
        const auto a = tour.order[move.i];
        const auto b = tour.order[move.i + 1];
        const auto c = tour.order[move.j];
        const auto d = tour.order[(move.j + 1) % n];
        const auto& distance = input_.distance;
        return distance[a][c] + distance[b][d] - distance[a][b] - distance[c][d];
    }

private:
    const Tsp& input_;
};
```

The component is attached as before, and `delta` names only the component,
which is also the evaluator:

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
| classes | the component, and one evaluator per neighborhood | one |
| recipe | `delta<TourLength, TwoOptLengthDelta>()` | `delta<TourLengthWithDelta>()` |
| fits | a component reused by several neighborhoods, each with its own evaluator, or one without an evaluator | a component that serves a single neighborhood |

A separate evaluator keeps the component unaware of the moves: `TourLength`
does not mention `TwoOpt`, so it serves unchanged any neighborhood, with or
without an evaluator, and a new neighborhood only adds a new evaluator.
`TourLengthWithDelta` instead depends on the move type of its delta. In
exchange it is one class instead of two, and since `evaluate` and
`delta_evaluate` run on the same object, they can share data the component
prepares once. The rest of the tutorial uses the separate version.

Chapter 10 shows how to check that a delta agrees with the full evaluation.

## See also

- [Cost](../reference/cost.md#delta-cost-components): the delta contract.
- [NeighborhoodExplorer](../reference/neighborhood-explorer.md): how deltas are
  bound.

## Next steps

[Chapter 5](05-running-a-search.md) runs the search.
