# Cost

`<easylocal/cost.hpp>` (cost models and cost expressions, `easylocal::cost`)
and `<easylocal/helpers/recipes.hpp>` (`component`, `delta`)

The cost of a solution is computed by **cost components**, combined by a
**cost expression** into a value of a **cost type**. Costs are minimized: a
lower cost is better; to maximize, return what is lost or the negated profit,
or give a root `cost::apply` its own `compare` (Great Deluge, whose levels are
fractions of the cost, needs positive costs). Moves are evaluated
incrementally by **delta cost components**. Together with the SolutionManager they
form the *cost layer*; the delta cost components bound to a NeighborhoodExplorer
form the *delta cost layer*.

## Cost components

| Member | Required |
| --- | --- |
| `evaluate(const Solution&) const -> Value` | yes |
| `delta_evaluate(const Solution&, const Move&) const -> Delta` | no: a co-located delta cost component |
| `name() -> std::string_view` (static or not) | no: its name in reports, otherwise its position, `#1`; static, and required, for a component with parameters, which is configured under it |
| `parameters_type` | no: a parameter block, which makes the component configurable (`cost.<name>.*`); the component is constructed from it |
| `describe(const Solution&) const -> std::string` | no: a text that explains its value on a solution, such as the violations it counts |

- `Value` is deduced from `evaluate` and may be arithmetic or a domain type,
  but not an unsigned integer: the difference of two costs would wrap around,
  so a component that counts returns `int` or `long long`.
- Construction: `Component(const Input&, args...)` is preferred,
  `Component(args...)` is accepted; `args` come from `component<C>(args...)`
  and convert as the constructor takes them. The same holds for a delta cost
  component and `delta<C, D>(args...)`.
- With parameters: `Component(const Input&, const parameters_type&, args...)`
  (or without the Input), from `component<C>(parameters, args...)`; without
  a first argument of that type, the parameters are the defaults. The recipe
  holds them, and builds the component from them when a runner or an app is
  bound.
- A component type may appear only once in a cost expression: the type is the
  identity of the component. To use one parametric component twice (two
  thresholds), derive a named type for each, `struct LongEdges8 :
  LongEdges<8> {};`, or give each its own parameters as a class.
- `name()` and `describe(solution)` are for people: `Session::cost_report()`,
  `cli::run`'s `--report` and the TextUI's solution window show each
  component's value with them. Without `describe`, only the value is shown.

## Cost expressions

A SolutionManager recipe has exactly one cost expression:
`solution_manager<SM>() | expression`, or `.with_cost(expression)`. Its leaves
are cost components; its nodes combine the costs of their children.

| Expression | Cost | Configuration |
| --- | --- | --- |
| `component<C>([parameters,] args...)` | the value of the component | its `parameters_type`'s, under `<name>` |
| `cost::sum(t1, ..., tn)` | `Σ wᵢ · costᵢ`, in the common type of the costs and the weights | `weights` |
| `cost::weighted(child, w)`, or `child * w`, `w * child` | a term of `cost::sum` with weight `w` (default 1) | |
| `cost::in_order(c1, ..., cn)` | `cost::lexicographic` of the children's costs | children's |
| `cost::objectives(c1, ..., cn)` | `cost::pareto` of the children's costs (n ≥ 2) | children's |
| `cost::hard_soft(hard, soft)` | `cost::hierarchical` of the two costs | `hard`, `soft` |
| `cost::apply(f, c1, ..., cn)` | `f(cost₁, ..., costₙ)` | children's |
| `cost::apply<F>(parameters, c1, ..., cn)` | `F{parameters}(cost₁, ..., costₙ)` | `F::parameters_type`'s, under `<name>`, and children's |
| `cost::approximately(c, tolerance)` | the cost of `c`, compared by the search within a `cost::tolerance` (at the root) | `tolerance.relative`, `tolerance.absolute`, and the child's as they are |

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
- At the root, the function of a `cost::apply` may define the order of its
  costs, `compare(a, b)`, and `cost::approximately` compares them within a
  tolerance (see Cost semantics).
- Configuration: the expression is exposed under `cost`. A `sum` has its
  `weights` (one per term, finite: a negative weight is accepted, NaN and
  infinity are not), a `hard_soft` names its children `hard` and
  `soft`, `in_order`, `objectives` and `apply` name them by position (`0`,
  `1`, ...); only
  configurable children appear. In the example above: `cost.hard.weights`.
- A component or a `cost::apply` function whose `parameters_type` is a
  parameter block is configured under its static `name()`, wherever it is in
  the expression: `cost.<name>.*`. Two of them with the same name make the
  configuration throw `std::invalid_argument`. A function with parameters is
  built from them, `cost::apply<F>(parameters, children...)`, and built again
  when they change; `cost::apply(F{...}, children...)` does not compile for it.
  A component or a function that has `parameters()` or `configuration()` and
  no such `parameters_type` does not compile.

## Cost models

| Type | Ordering | `cost::delta` |
| --- | --- | --- |
| arithmetic (`cost::arithmetic`: signed integers and floating point) | `<` | `candidate - current` |
| `cost::lexicographic<Ts...>` | lexicographic over the values | none |
| `cost::pareto<Ts...>` | Pareto dominance (a partial order) | none |
| `cost::hierarchical<Hard, Soft>` | hard first, soft when hard is equivalent; each branch by its `<=>`, or else by `<` and `==` | hard better: `-∞`, hard worse: `+∞`, else the soft delta |

The structured types are constructed directly, with deduced types:
`cost::hierarchical{hard, soft}`, `cost::lexicographic{a, b}`,
`cost::pareto{a, b}`. The concepts `cost::lexicographic_cost`,
`cost::hierarchical_cost` and `cost::pareto_cost` recognize them.

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

So a descent (First and Best Improvement, Hill Climbing) moves from E to B or
C, never from B to A, and from a point of the front only to a point that
dominates it. The Pareto front is the set of the points that no other
dominates, and a search with a pareto cost keeps the non-dominated solutions it
reaches in its archive and returns them as its front (see
[Runners](runners.md#results)). Pareto Late Acceptance Hill Climbing explores
from several solutions at once to spread over the front. An app run by name
carries the front in `named_run_result.front`, the Session keeps it
(`last_run_front()`), `cli::run` prints it and REST returns it with the
solution (see [Apps and tools](app-and-tools.md)); the TextUI shows only the
current solution.

The runners whose acceptance only compares costs accept a pareto cost: First
and Best Improvement, Hill Climbing, Late Acceptance, Pareto Late Acceptance
and the tabu searches, except Aspiration Plus and Elite Candidate, whose
levels are factors of the cost, with any tabu list but `Foo` and `RandomFoo`,
which need a delta. Late Acceptance and Tabu Search may move to a point that
does not dominate the current one: Late Acceptance accepts a candidate no worse
than an older cost, and Tabu Search applies the best admissible move even when
it worsens, choosing among equivalent candidates only, so the first of two
unordered ones stays. The runners that need a numeric delta (Simulated
Annealing, Great Deluge) do not accept a pareto cost.

`cost::delta(candidate, current)` is the numeric difference used by
delta-based acceptance; user cost types provide it as a free function found by
ADL (call it as `using cost::delta; delta(a, b)`). The concept
`cost::has_delta<Cost>` tells whether it exists.

`cost::zero<Cost>()` is the cost of no violation and no penalty: `Cost{}` for
value-initializable types (0 for arithmetic costs) and the zero of every level
for `lexicographic`, `hierarchical` and `pareto` costs. Other cost types provide it by
specializing `cost::zero_cost<Cost>` with a static `value()`;
`cost::has_zero<Cost>` tells whether it exists. A pipeline stage
`until_feasible()` stops at the zero of the hard cost, also when deltas leave a
floating-point hard cost a rounding error above it: the run then evaluates the
solution in full (see [Runners](runners.md)).

### Comparisons within a tolerance

`<easylocal/cost/tolerance.hpp>` compares costs forgiving the rounding errors
of floating-point arithmetic. `cost::tolerance{.relative, .absolute}` (both
1e-9 by default) is the tolerance: two floating-point values are equal within
it when they differ by at most `max(absolute, relative * max(|a|, |b|))`.

| Function | Result |
| --- | --- |
| `cost::approximately_equal(a, b, tolerance)`, or `tolerance(a, b)` | numbers within the tolerance when either is floating point, integers exactly; lexicographic, hierarchical and pareto costs level by level; other values with `==` |
| `cost::approximate_compare(a, b, tolerance)` | a `std::partial_ordering`: equivalent within the tolerance, otherwise by `<=>`; level by level, as the cost model orders, for structured costs |

The checks of `easylocal::testing`, `check(app)` and the Session use it by
default (`testing::approximately` is the same type). It is not transitive.

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
`cost::better_or_equivalent(sm, a, b)` are the relations algorithms compare
costs with, through `run.better(...)`. By default they are `<`, `==` and `<=`
of the cost type, independent queries, so a cost type may answer `<=` in one
pass.

The root of the expression may define them all with one hook: the function of
a root `cost::apply` defines `compare(a, b)`, returning a
`std::partial_ordering`. Then `better(a, b)` is `compare(a, b) < 0`,
`equivalent(a, b)` is `compare(a, b) == 0` and `better_or_equivalent(a, b)` is
`compare(a, b) <= 0`; an `unordered` result is none of them, as for two points
of a Pareto front. A function that maximizes:

    struct Profit
    {
        int operator()(int value) const { return value; }
        std::partial_ordering compare(int lhs, int rhs) const { return rhs <=> lhs; }
    };

    solution_manager<SM>() | cost::apply(Profit{}, component<Gain>())

The rules, checked when the recipe is built:

- `compare` is the only hook: a function that defines `better`, `equivalent`
  or `better_or_equivalent` does not compile, since one relation of its own
  next to the defaults of the others would order the costs two ways.
- Only the root defines the semantics: a `cost::apply` with a `compare` below
  another node does not compile.
- A root `cost::apply` hides the structure below it: with a `cost::hard_soft`
  under it, a pipeline stage `until_feasible()` evaluates every component, and
  its hard stage compares the hard costs with their own operators.

`cost::approximately(child, {.relative, .absolute})` is the root that compares
floating-point costs within a tolerance (both 1e-9 by default): costs equal
within it are `equivalent`, and a cost is `better` only by more than it, with
`cost::approximate_compare` (see Comparisons within a tolerance). A descent
then stops at a move that improves only by rounding errors, and a target is
reached by a cost within the tolerance of it. It keeps the structure below it:
over a `cost::hard_soft`, `until_feasible()` still evaluates only the hard
components, and its hard stage compares within the same tolerance. The
tolerance is configurable, `cost.tolerance.relative` and
`cost.tolerance.absolute` in an app.

    solution_manager<SM>()
        | cost::approximately(
              cost::hard_soft(component<Overlaps>(), component<Length>()),
              {.relative = 1e-9, .absolute = 1e-6})

Without it the search compares exactly, and only the checks forgive rounding
errors (see [Testing](testing.md)).
- The algorithms that read `cost::delta` (Simulated Annealing, Great Deluge,
  the aspiration levels of Tabu Search) assume that a negative delta is an
  improvement: the sign of `cost::delta(a, b)` must agree with `better(a, b)`.
  A `compare` that finds a larger cost better breaks them; maximize with
  `cost::apply` returning the negated value instead. Simulated Annealing and
  Great Deluge reject such a `compare` when the compiler can evaluate it (a
  `constexpr` member of a default-constructible function); `check(app)`
  verifies the sign on the moves it tries.

## Delta cost components

| Member | Required |
| --- | --- |
| `delta_evaluate(const Solution&, const Move&) const -> Delta` | yes |

- Law: `Value + Delta -> Value`, equal to the value of the component after the
  move. Check it with `testing::check_delta_cost_component`.
- Bound per component and per neighborhood: `delta<C, D>(args...)` for a
  separate delta cost component, `delta<C>()` for one co-located in the component.
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
  redefine ordering and equivalence (tolerances, a maximized objective)
  without touching the cost type.
- **One hook for the order.** `compare` defines the three relations together,
  so they cannot disagree, and a partial order (`unordered`) fits as Pareto
  dominance does.
- **Deltas never see weights or structure.** They are per component; the move
  cost is always recomputed by the expression, so a delta stays valid whatever
  the expression is.
- **`delta` is the contract, `operator-` is syntax.** Acceptance criteria rely on
  `cost::delta`; `cost::lexicographic` has none on purpose.
