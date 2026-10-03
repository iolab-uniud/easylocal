# Cost

`<easylocal/cost.hpp>` (cost models and cost expressions, `easylocal::cost`)
and `<easylocal/helpers/recipes.hpp>` (`component`, `delta`)

The cost of a solution is computed by **cost components**, combined by a
**cost expression** into a value of a **cost type**. Moves are evaluated
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
- A component type may appear only once in a cost expression.

## Cost expressions

A SolutionManager recipe has exactly one cost expression:
`solution_manager<SM>() | expression`, or `.with_cost(expression)`. Its leaves
are cost components; its nodes combine the costs of their children.

| Expression | Cost | Configuration |
| --- | --- | --- |
| `component<C>(args...)` | the value of the component | none |
| `cost::sum(t1, ..., tn)` | `Σ wᵢ · costᵢ`, in the common type of the costs and the weights | `weights` |
| `cost::weighted(child, w)`, or `child * w`, `w * child` | a term of `cost::sum` with weight `w` (default 1) | |
| `cost::in_order(c1, ..., cn)` | `cost::lexicographic` of the children's costs | children's |
| `cost::hard_soft(hard, soft)` | `cost::hierarchical` of the two costs | `hard`, `soft` |
| `cost::apply(f, c1, ..., cn)` | `f(cost₁, ..., costₙ)` | `f.configuration()`, if any, and children's |

```cpp
solution_manager<SM>()
    | cost::hard_soft(
          cost::sum(component<A>(), component<B>() * 10),
          cost::in_order(component<C>(), cost::apply(f, component<D>())))
```

- `cost::sum` adds arithmetic costs only; a domain value is turned into a
  number with `cost::apply`. A weighted term, `cost::weighted(child, w)` or its
  shorthand `child * w`, is allowed only directly inside a `cost::sum`.
- The components of the `hard` branch of a `cost::hard_soft` at the root come
  first: TwoStage evaluates only them in its first stage.
- At the root, the function of a `cost::apply` may define `better`,
  `equivalent` and `better_or_equivalent` (see Cost semantics).
- Configuration: the expression is exposed under `cost`. A `sum` has its
  `weights` (one per term), a `hard_soft` names its children `hard` and
  `soft`, `in_order` and `apply` name them by position (`0`, `1`, ...); only
  configurable children appear. In the example above: `cost.hard.weights`.

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

`cost::zero<Cost>()` is the cost of no violation and no penalty: `Cost{}` for
value-initializable types (0 for arithmetic costs) and the zero of every level
for `lexicographic` and `hierarchical` costs. Other cost types provide it by
specializing `cost::zero_cost<Cost>` with a static `value()`;
`cost::has_zero<Cost>` tells whether it exists. TwoStage stops its first stage
at the zero of the hard cost.

### Costs as text

`cost::from_text<Cost>(text)` (`<easylocal/cost/text.hpp>`) reads a cost
written by a user, such as a target: a number for an arithmetic cost,
`[hard, soft]` for a `cost::hierarchical`, `[v1, v2, ...]` for a
`cost::lexicographic`, nested as the types are (`[0, [3, 1.5]]`). It throws
`std::invalid_argument` with the reason; `cost::to_text(cost)` writes a cost
back in the same form. `cost::text_readable<Cost>` tells
which costs it reads; a problem with another cost type, or its own notation,
provides `read_cost(const Input&, std::string_view) -> Cost`, found by ADL
through its Input, which the Session and the TextUI use instead.

## Cost semantics

`cost::better(sm, a, b)`, `cost::equivalent(sm, a, b)` and
`cost::better_or_equivalent(sm, a, b)` ask the function of a root
`cost::apply` first and fall back to `<`, `==` and `<=`. Algorithms reach them through `run.better(...)`; the
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
- **The structure of the cost is written in the recipe.** Which components
  are hard and which soft, their order and their weights are read where the
  components are listed, not deduced from a declaration order and a separate
  list of weights. TwoStage reads the hard components from the expression.
- **Weights sit beside their term.** `component<C>() * w` cannot drift out of
  line with the components, as positional weights can. The `*` only marks a
  term of a `cost::sum`: it never multiplies component values, so domain types
  need no `operator*`.
- **`cost::sum` adds numbers.** Domain values are mapped explicitly with
  `cost::apply`, so domain types never carry arithmetic operators just to be
  summed.
- **Semantics belong to the cost layer.** The root of the expression may
  redefine ordering and equivalence (tolerances, lazy comparisons) without
  touching the cost type.
- **Deltas never see weights or structure.** They are per component; the move
  cost is always recomputed by the expression, so a delta stays valid whatever
  the expression is.
- **`delta` is the contract, `operator-` is syntax.** Acceptance criteria rely on
  `cost::delta`; `cost::lexicographic` has none on purpose.
