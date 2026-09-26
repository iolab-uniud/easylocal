# TSP MWE

This is the second concrete EasyLocal++ pressure-test model. It is deliberately
outside `include/easylocal/` and does not introduce new public framework API.

The model is a symmetric TSP with a dense `double` distance matrix, a `Tour`
represented by a permutation of city ids, and a deterministic lazy 2-opt
neighborhood. A `TwoOptMove{i, j}` cuts tour edges `(i, i+1)` and `(j, j+1)`
and reverses the segment `[i+1, j]`.

Deterministic 2-opt traversal is authored through `first_move`/`next_move` and
exposed to consumers through the unified `easylocal::moves(...)` customization
point, which adapts the cursor protocol to a lazy `input_range`.
Rank decoding remains an implementation detail used to generate a uniform
single random 2-opt proposal and is not part of deterministic neighborhood
authoring.

`TourLengthComponent` returns the structured materialized value
`TourLengthValue{total}`. The problem-side `TspSolutionManager` aggregates that
value to the algorithm-facing scalar `double` cost. This intentionally exercises
a partially ordered floating-point `cost_type` without introducing an epsilon or
approximate-comparison policy into the framework.

`TwoOptTourLengthDeltaEvaluator` provides a recipe-local incremental evaluator
for `TourLengthComponent`. Its structured `TourLengthDelta{change}` follows the
same semantic law used by the Assignment MWE:

```text
value_after == value_before + delta
```

For a symmetric TSP, 2-opt changes only the two cut edges, so the evaluator
computes the added edge cost minus the removed edge cost without materializing a
candidate `Tour`. Tests compare this incremental value against full component
evaluation for every move in the deterministic small neighborhood. Runner-level
tests retain the no-delta fallback case and separately verify that rejected
all-delta candidates perform no `make_move`, while accepted all-delta candidates
perform exactly one.

The current test matrices still use only values exactly representable in binary
floating point, such as halves and quarters. Non-binary-exact values and
approximate comparisons remain deliberately reserved for a following iteration,
so floating-point comparison policy can be examined independently from delta
integration.

## Application configuration and external instance

`sa_main.cpp` owns its `AppParameters` block directly, because the instance-file
path and RNG seed parameterize the application rather than the TSP model or an
EasyLocal component. `FixedLengthParameters` remains beside the temperature
policy and `NeighborhoodUnionParameters<2>` remains beside the union it
configures. After constructing the runner, the main combines its automatic
`solver.search.temperature.*` and `solver.neighborhood.random_biases` subtree
with `application.*` in a read-only `config::root(...)`, traverses the resulting
paths, and loads `instances/small.tsp`.

This layout is intentional: concrete parameter declarations live next to the
thing they parameterize, while `easylocal::config` contains only generic schema,
tree, textual-override, and frontend machinery. The CLI is derived from that
same tree; configuration-file loading remains a separate frontend.

## Runnable composite-neighborhood SA example

`sa_main.cpp` is a runnable end-to-end Simulated Annealing example using two
heterogeneous TSP neighborhoods: the existing 2-opt explorer and a
`SwapCitiesNeighborhoodExplorer`. The runner composes them with
`neighborhood_union(...)` and configures child-selection weights with
`random_biases(3.0, 1.0)`. A fixed `std::mt19937` seed makes repeated runs
reproducible within the same standard-library implementation.

Both child neighborhoods attach a `TourLengthComponent` delta evaluator. The
union exposes that component delta because every child provides it, then
dispatches incrementally to the evaluator belonging to the tagged child move.
`SwapTourLengthDeltaEvaluator` covers the four tour edges potentially affected
by a position swap, deduplicating them for adjacent and wrap-around cases. The
resulting SA path is therefore all-delta and does not materialize rejected
candidate tours.

Delta propagation is deliberately conservative per component: if any child of
a union lacks a delta for an active component, that component is omitted from
the union's delta set and the existing evaluation machinery falls back to full
evaluation for that component. Nested unions preserve the same rule.

With the default top-level build, run it as:

```text
./build/<preset>/examples/tsp/easylocal_tsp_sa_mwe
```

For example, the same executable can override both SA parameters and union
biases without changing the MWE source:

```sh
./build/<preset>/examples/tsp/easylocal_tsp_sa_mwe \
  --solver.search.temperature.max_iterations=50 \
  --solver.neighborhood.random_biases='[1, 4]'
```

`--help` lists all application and runner parameters with descriptions and
current values.

The example also accepts a compact configuration file:

```sh
./build/<preset>/examples/tsp/easylocal_tsp_sa_mwe \
  --config examples/tsp/configs/small.cfg \
  --solver.search.temperature.max_iterations=50
```

The precedence is C++ defaults, then file overrides, then CLI overrides. Any
file/CLI diagnostic causes a non-zero exit before the runner is bound.

## Floating-point pressure test

A separate test iteration also uses decimal distances such as `0.1`, `0.2`, and
`0.039`, which are not generally exactly representable as binary floating-point
values. The production MWE deliberately keeps exact `double` value semantics:
`TourLengthValue::operator==`, aggregation, and the framework remain unchanged.
Approximate comparison is explicit and test-local, with separately supplied
relative and absolute tolerances rather than a framework-wide implicit epsilon.

The tests exercise two distinct numerical questions. First, the delta law is
checked over the complete deterministic five-city 2-opt neighborhood using an
approximate comparison, while also requiring that at least one move genuinely
fails exact equality between full and incremental evaluation. Second, a
mathematically neutral 2-opt move demonstrates that raw `double` ordering can
make the incremental path appear microscopically better even when full
evaluation is unchanged. The test-local comparison must suppress that numerical
artifact without suppressing a nearby but real improvement.

The comparison helper is also tested independently for absolute and relative
tolerance, symmetry, finite/non-finite values, adjacent representable values,
and the deliberately non-transitive nature of approximate equality. This last
property is important evidence for the later API discussion: approximate
equality must not be silently treated as an ordinary equivalence relation or
assumed suitable for a three-way ordering.
