# Assignment MWE

This concrete model is used to discover the EasyLocal++ API.

It is deliberately **outside** `include/easylocal/` and is not part of the
public framework API.

## Problem

Each job has a non-negative demand and is assigned to one machine. Each machine
has a non-negative capacity.

The current cost is total overload:

```text
sum_m max(0, load[m] - capacity[m])
```

Example:

```text
demand   = [4, 3, 2]
capacity = [5, 5]
solution = [0, 0, 1]
```

Loads are `[7, 2]`, so the cost is `2`. The MWE represents this result as a
small `Cost` value type whose ordering is defined with C++20/23 three-way
comparison.

Move `(job=1, destination=1)` produces:

```text
solution = [0, 1, 1]
loads    = [4, 5]
cost     = 0
```

## Responsibilities

`Instance`
: Plain problem data. The example publishes it as a const object during use.

`Solution`
: Plain value representation. It does not store an `Instance` pointer/reference.

`Move`
: Plain descriptive value. It does not store a `Solution` or `Instance`
  pointer/reference.

`Cost`
: Value returned by full evaluation. It owns its ordering semantics through
  three-way comparison.

`SolutionManager`
: Instance-bound service responsible for structural solution validation and full
  solution evaluation.

`NeighborhoodExplorer`
: Service responsible for neighborhood traversal, move validity and move
  application. It is bound to a `SolutionManager`, which in turn determines the
  instance.

No generic EasyLocal++ concepts are extracted yet.

## Neighborhood traversal

Deterministic traversal is lazy:

```cpp
auto candidates = neighborhood.moves(solution);
```

The neighborhood is represented through ordinal positions that are decoded into
`Move` values on demand. It is not implicitly materialized into a container.

Random traversal uses the same range-oriented interface:

```cpp
auto candidates = neighborhood.random_moves(solution, rng);
```

Each `NeighborhoodExplorer` declares one random sampling semantic through its
`random_sampling` type. The assignment MWE currently declares:

```cpp
using random_sampling = sampling::without_replacement;
```

Its random range is therefore finite and produces a random permutation of the
neighborhood. Sparse Fisher-Yates is applied to ordinal positions, so the
complete neighborhood is never materialized.

The number of random candidates consumed is a responsibility of the search
algorithm or caller, expressed through normal range composition:

```cpp
auto sample =
    neighborhood.random_moves(solution, rng)
    | std::views::take(sample_size);
```

A future explorer with `sampling::with_replacement` will be a distinct explorer
type. Such a random range may be unbounded; algorithms using it must impose the
appropriate evaluation/sample budget.

Traversal ranges compose with standard views:

```cpp
auto filtered =
    neighborhood.moves(solution)
    | std::views::filter(predicate);
```

In debug builds, dereferencing a previously-created move range after the
underlying `Solution` representation changes triggers an assertion. The check
uses a debug-only solution fingerprint and is compiled out when `NDEBUG` is
defined.

## Search runner

Search algorithms are wired through the public recipe-based
`easylocal::Runner`. Service objects are not constructed by application code.
Instead, the runner records the concrete service types and any constructor
arguments:

```cpp
auto runner =
    Runner{FirstImprovement{params}}
        .with_solution_manager<SolutionManager>()
        .with_neighborhood<NeighborhoodExplorer>();
```

The equivalent pipeline syntax is also supported:

```cpp
auto runner =
    Runner{FirstImprovement{params}}
    | solution_manager<SolutionManager>()
    | neighborhood<NeighborhoodExplorer>();
```

`bind(instance)` materializes an instance-bound graph owned by an internal,
non-movable bound runner: first the solution manager, then the neighborhood
explorer. A single run then supplies only its initial solution and any algorithm-specific
runtime dependencies such as an RNG:

```cpp
auto bound = runner.bind(instance);
auto result = bound.run(initial_solution);
```

This keeps service construction state reusable before an instance is loaded,
while ownership and graph consistency remain internal to the runner.

## Deferred

The current MWE deliberately does not define:

- generic framework concepts;
- fallback/capability negotiation between random sampling strategies;
- diagnostics for fallback from without- to with-replacement;
- EL3 neighborhood adapters;
- biased/adaptive sampling;
- generic indexed/combinatorial move-space helpers;
- generic cost structures;
- delta evaluation;
- floating-point semantics;
- CLI/configuration;
- tracing/logging.
