# Cost

`<easylocal/cost.hpp>` (cost models, `easylocal::cost`) and
`<easylocal/helpers/recipes.hpp>` (`component`, `aggregator`, `delta`)

The cost of a solution is computed by **cost components**, combined by an
**aggregator** into a value of a **cost type**. Moves are evaluated
incrementally by **delta evaluators**. Together with the SolutionManager they
form the *cost layer*; the delta evaluators bound to a NeighborhoodExplorer
form the *delta cost layer*.

## Cost components

| Member | Required |
| --- | --- |
| `evaluate(const Solution&) const -> Value` | yes |
| `delta_evaluate(const Solution&, const Move&) const -> Delta` | no: a co-located delta evaluator |

- `Value` is deduced from `evaluate` and may be arithmetic or a domain type.
- Construction: `Component{const Input&, args...}` is preferred,
  `Component{args...}` is accepted; `args` come from `component<C>(args...)`.
- A component type may appear only once in a recipe.

## Aggregators

An aggregator is a function object called with the component values in
declaration order and returning the cost.

| Aggregator | Result | Configuration |
| --- | --- | --- |
| `cost::weighted_sum{w1, ..., wn}` | scalar | `cost.weights` |
| `cost::weighted_sum_with_hard_penalty` | `hard_multiplier * hard + Σ wᵢ * softᵢ` | `cost.hard_multiplier`, `cost.soft_weights` |
| any function object | any cost type | none, unless it provides `configuration()` |

Implicit aggregator, when `aggregator(...)` is omitted:

| Components | Aggregator |
| --- | --- |
| one, any value type | identity, silently |
| several, all arithmetic | unit-weight `cost::weighted_sum`, with a warning |
| several, with domain values | none: compile-time error asking for an aggregator |

Optional aggregator members:

| Member | Effect |
| --- | --- |
| `better(a, b)`, `equivalent(a, b)`, `better_or_equivalent(a, b)` | redefine the cost semantics |
| `hard(hard_values...) -> HardCost` over the leading components | models `cost::hard_projection`, used by TwoStage |
| `configuration()` | exposes parameters to the configuration tree |

## Cost models

| Type | Ordering | `cost::delta` |
| --- | --- | --- |
| arithmetic (`cost::arithmetic`) | `<` | `candidate - current` |
| `cost::lexicographic<Ts...>` | lexicographic over the values | none |
| `cost::hierarchical<Hard, Soft>` | hard first, soft when hard is equivalent | hard better: `-∞`, hard worse: `+∞`, else the soft delta |

Both structured types are constructed directly, with deduced types:
`cost::hierarchical{hard, soft}`, `cost::lexicographic{a, b}`. Traits:
`cost::lexicographic_traits`, `cost::is_lexicographic_v`,
`cost::lexicographic_type`, `cost::is_hierarchical_v`,
`cost::hierarchical_type`.

`cost::delta(candidate, current)` is the numeric difference used by
delta-based acceptance; user cost types provide it as a free function found by
ADL (call it as `using cost::delta; delta(a, b)`). The concept
`cost::has_delta<Cost>` tells whether it exists.

## Cost semantics

`cost::better(sm, a, b)`, `cost::equivalent(sm, a, b)` and
`cost::better_or_equivalent(sm, a, b)` ask the aggregator first and fall back
to `<`, `==` and `<=`. Algorithms reach them through `run.better(...)`; the
three relations are deliberately independent queries.

## Delta evaluators

| Member | Required |
| --- | --- |
| `delta_evaluate(const Solution&, const Move&) const -> Delta` | yes |

- Law: `Value + Delta -> Value`, equal to the value of the component after the
  move. Check it with `testing::check_delta_evaluator`.
- Bound per component and per neighborhood: `delta<C, D>(args...)` for a
  separate evaluator, `delta<C>()` for one co-located in the component.
- Components without a delta for a neighborhood are re-evaluated on a candidate
  solution; with full coverage no candidate is built.

## Design choices

- **Cost comes only from components.** One evaluation path for every problem;
  the SolutionManager stays about solutions.
- **Aggregation is explicit when it involves a choice.** Identity and a unit
  weighted sum of numbers involve none; weighting domain values does, so it is
  never inferred and domain types never carry arithmetic operators just to be
  aggregated.
- **Semantics belong to the cost layer.** The aggregator may redefine ordering
  and equivalence (tolerances, lazy comparisons) without touching the cost type.
- **Deltas never see weights or structure.** They are per component; the move
  cost is always re-aggregated, so a delta stays valid whatever aggregator is
  used.
- **`delta` is the contract, `operator-` is syntax.** Acceptance criteria rely on
  `cost::delta`; `cost::lexicographic` has none on purpose.
