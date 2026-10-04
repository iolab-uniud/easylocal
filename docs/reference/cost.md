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
| `name() -> std::string_view` (static or not) | no: its name in reports, otherwise its position, `#1` |
| `describe(const Solution&) const -> std::string` | no: a text that explains its value on a solution, such as the violations it counts |

- `Value` is deduced from `evaluate` and may be arithmetic or a domain type.
- Construction: `Component{const Input&, args...}` is preferred,
  `Component{args...}` is accepted; `args` come from `component<C>(args...)`.
- A component type may appear only once in a cost expression.
- `name()` and `describe(solution)` are for people: `Session::cost_report()`,
  `cli::run`'s `--report` and the TextUI's solution window show each
  component's value with them. Without `describe`, only the value is shown.

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
| `cost::objectives(c1, ..., cn)` | `cost::pareto` of the children's costs (n ≥ 2) | children's |
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
  first: a pipeline stage `until_feasible()` (the first stage of
  `two_stage()`) evaluates only them.
- At the root, the function of a `cost::apply` may define `better`,
  `equivalent` and `better_or_equivalent` (see Cost semantics).
- Configuration: the expression is exposed under `cost`. A `sum` has its
  `weights` (one per term), a `hard_soft` names its children `hard` and
  `soft`, `in_order`, `objectives` and `apply` name them by position (`0`,
  `1`, ...); only
  configurable children appear. In the example above: `cost.hard.weights`.

## Cost models

| Type | Ordering | `cost::delta` |
| --- | --- | --- |
| arithmetic (`cost::arithmetic`) | `<` | `candidate - current` |
| `cost::lexicographic<Ts...>` | lexicographic over the values | none |
| `cost::pareto<Ts...>` | Pareto dominance (a partial order) | none |
| `cost::hierarchical<Hard, Soft>` | hard first, soft when hard is equivalent | hard better: `-∞`, hard worse: `+∞`, else the soft delta |

The structured types are constructed directly, with deduced types:
`cost::hierarchical{hard, soft}`, `cost::lexicographic{a, b}`,
`cost::pareto{a, b}`. Traits: `cost::lexicographic_traits`,
`cost::is_lexicographic_v`, `cost::lexicographic_type`,
`cost::is_hierarchical_v`, `cost::hierarchical_type`, `cost::is_pareto_v`,
`cost::pareto_type`.

### Pareto costs

A `cost::pareto` holds one value per objective, all minimized, and orders them
by Pareto dominance, which leaves some costs unordered. With two objectives,
makespan and tardiness of a schedule for example:

```cpp
solution_manager<ScheduleManager>()
    | cost::objectives(component<Makespan>(), component<Tardiness>())
```

each solution is a point of the plane:

<figure>
<svg viewBox="0 0 360 320" width="360" role="img"
     aria-label="Seven solutions in the plane of two objectives: four on the Pareto front, joined by a staircase, three dominated; the region dominated by B is shaded">
  <g fill="none" stroke="currentColor" stroke-width="1.5">
    <path d="M50 290 H340 M50 290 V30"/>
    <path d="M335 286 L341 290 L335 294 M46 35 L50 29 L54 35"/>
  </g>
  <rect x="106" y="40" width="224" height="125" fill="currentColor" opacity="0.12"/>
  <path d="M78 40 V90 H106 V165 H162 V215 H246 V240 H330" fill="none"
        stroke="currentColor" stroke-width="1.5" stroke-dasharray="5 3"/>
  <g fill="currentColor">
    <circle cx="78" cy="90" r="5"/><circle cx="106" cy="165" r="5"/>
    <circle cx="162" cy="215" r="5"/><circle cx="246" cy="240" r="5"/>
  </g>
  <g fill="none" stroke="currentColor" stroke-width="1.5">
    <circle cx="190" cy="140" r="5"/><circle cx="134" cy="115" r="5"/>
    <circle cx="274" cy="165" r="5"/>
  </g>
  <g fill="currentColor" font-family="sans-serif" font-size="13">
    <text x="86" y="86">A</text><text x="114" y="161">B</text>
    <text x="170" y="211">C</text><text x="254" y="236">D</text>
    <text x="198" y="136">E</text><text x="142" y="111">F</text>
    <text x="282" y="161">G</text>
    <text x="250" y="70">dominated by B</text>
    <text x="262" y="310">objective 1</text>
    <text x="8" y="22">objective 2</text>
  </g>
</svg>
<figcaption>A, B, C and D are the Pareto front (the staircase); E, F and G are
dominated. The shaded region is what B dominates.</figcaption>
</figure>

The cost semantics read as follows, for costs `a` and `b`:

- `better(a, b)`, `a < b`: `a` dominates `b`, no worse in every objective and
  better in at least one. B is better than E and G, which lie in the region it
  dominates; C is better than E.
- `better_or_equivalent(a, b)`, `a <= b`: `a` weakly dominates `b`, no worse in
  every objective, equal costs included.
- `equivalent(a, b)`, `a == b`: equal in every objective.
- Neither `better(a, b)` nor `better(b, a)`: the costs are unordered,
  `a <=> b` is `std::partial_ordering::unordered`. A and B are, as are any
  two points of the front: each is better in one objective.

So a runner moves from E to B or C, never from B to A, and from a point of the
front only to a point that dominates it. The Pareto front is the set of the
points that no other dominates, and a search with a pareto cost keeps the
non-dominated solutions it reaches in its archive and returns them as its
front (see [Runners](runners.md#results)). Pareto Late Acceptance Hill
Climbing explores from several solutions at once to spread over the front.
The runners that need a numeric delta (Simulated Annealing, Great Deluge) or a
total order do not accept a pareto cost.

`cost::delta(candidate, current)` is the numeric difference used by
delta-based acceptance; user cost types provide it as a free function found by
ADL (call it as `using cost::delta; delta(a, b)`). The concept
`cost::has_delta<Cost>` tells whether it exists.

`cost::zero<Cost>()` is the cost of no violation and no penalty: `Cost{}` for
value-initializable types (0 for arithmetic costs) and the zero of every level
for `lexicographic`, `hierarchical` and `pareto` costs. Other cost types provide it by
specializing `cost::zero_cost<Cost>` with a static `value()`;
`cost::has_zero<Cost>` tells whether it exists. A pipeline stage
`until_feasible()` stops at the zero of the hard cost.

### Costs as text

`cost::from_text<Cost>(text)` (`<easylocal/cost/text.hpp>`) reads a cost
written by a user, such as a target: a number for an arithmetic cost,
`[hard, soft]` for a `cost::hierarchical`, `[v1, v2, ...]` for a
`cost::lexicographic` or a `cost::pareto`, nested as the types are (`[0, [3, 1.5]]`). It throws
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
  list of weights. `until_feasible()` reads the hard components from the
  expression.
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
