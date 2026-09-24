# Assignment MWE

This concrete model is used to discover the EasyLocal++ API.

It is deliberately **outside** `include/easylocal/` and is not part of the
public framework API.

## Problem

Each job has a non-negative demand and is assigned to one machine. Each machine
has a non-negative capacity.

The current capacity component is structured:

```text
CapacityValue {
    overloaded_machines,
    total_overload
}
```

where:

```text
total_overload = sum_m max(0, load[m] - capacity[m])
```

The algorithm-facing cost is the hierarchical pair:

```text
(total_overload, overloaded_machines)
```

so the original total-overload objective remains primary, while the number of
overloaded machines is a deterministic secondary level.

Example:

```text
demand   = [4, 3, 2]
capacity = [5, 5]
solution = [0, 0, 1]
```

Loads are `[7, 2]`, so the capacity value and final cost are both represented by
`CapacityValue{1, 2}` and `Cost{2, 1}` at their respective abstraction levels.

Move `(job=1, destination=1)` produces:

```text
solution = [0, 1, 1]
loads    = [4, 5]
cost     = (0, 0)
```

## Cost composition

`CapacityCostComponent` performs full eager evaluation and returns a materialized
`CapacityValue`. The problem-side `SolutionManager` remains responsible for
solution validity and aggregation policy, while cost components are attached to
the runner recipe compositionally:

```cpp
auto manager =
    solution_manager<SolutionManager>()
    | component<CapacityCostComponent>();
```

The bound configured manager eagerly evaluates every attached component and
passes their materialized values to the problem-side aggregation policy. The
resulting `cost_type` remains the ordinary three-way-comparable value seen by
search algorithms. Each component type may occur at most once in a manager
recipe; semantically distinct parameterizations can use distinct wrapper or
subclass types and therefore distinct compile-time identities.

The MWE prototypes three reusable aggregation categories:

- `aggregation::weighted_sum`;
- `aggregation::lexicographic`;
- `aggregation::hierarchical`.

`AssignmentCostAggregator` uses the predefined hierarchical aggregator to map
fields of the structured capacity value into the final `Cost`. Domain-specific
projection from structured component values remains explicit for now; no
projection DSL is introduced by this iteration.

The aggregators are still MWE-local prototypes. Promotion to the public
`include/easylocal/` API is intentionally deferred until the design has received
further pressure testing.

## Delta evaluation

The assignment MWE also prototypes a separate delta evaluator for the pair
`CapacityCostComponent x Move`:

```cpp
ReassignCapacityDeltaEvaluator::delta_evaluate(solution, move)
    -> CapacityDelta
```

`CapacityDelta` is a distinct structured, materialized value containing changes
to both `overloaded_machines` and `total_overload`. Applying it follows the
contract:

```text
component_value_after = component_value_before + delta
```

The tests check this property against full component evaluation for every move
in the small deterministic assignment neighborhood.

Delta evaluators are attached to a neighborhood recipe explicitly by component
type rather than being intrinsic metadata of the neighborhood or move type:

```cpp
auto nhe =
    neighborhood<NeighborhoodExplorer>()
    | delta<CapacityCostComponent, ReassignCapacityDeltaEvaluator>();
```

The runner's internal evaluation facility matches active component types against
the deltas attached to that particular neighborhood recipe. A delta attached to
a component that is not active in the paired solution-manager recipe is a
compile-time error at bind. If an active component has no matching delta, the
framework materializes the candidate `Solution` once and reuses it for every
fallback full-component evaluation. If all components have deltas, a rejected
candidate never requires `make_move`; an accepted candidate applies the move
exactly once. If fallback materialization was already necessary, acceptance
promotes that materialized candidate instead of applying the move again.

The search algorithms see only eager materialized evaluations and `cost_type`
values. They do not select deltas, distinguish fallback components, or manage
candidate materialization. `after_move` is only the current working name for the
algorithm-side operation and is intentionally not considered final terminology.
Laziness, caching and proxy lifetime/invalidation remain postponed.

## Responsibilities

`Instance`
: Plain problem data. The example publishes it as a const object during use.

`Solution`
: Plain value representation. It does not store an `Instance` pointer/reference.

`Move`
: Plain descriptive value. It does not store a `Solution` or `Instance`
  pointer/reference.

`CapacityCostComponent`
: Instance-bound full evaluator for one structured cost component.

`CapacityDelta`
: Materialized structured change applicable to `CapacityValue` with `operator+`.

`ReassignCapacityDeltaEvaluator`
: Separate evaluator specialized for the capacity component and assignment
  reassign move.

`Cost`
: Materialized value returned by full aggregation. It owns its hierarchical
  ordering semantics through three-way comparison.

`SolutionManager`
: Problem-side service responsible for structural solution validation and the
  aggregation policy. Cost-component storage/evaluation is supplied by the
  framework from the components attached to the manager recipe.

`NeighborhoodExplorer`
: Problem-side service responsible for neighborhood traversal, move validity and
  `make_move`. Delta evaluators are optional capabilities attached externally to
  a particular neighborhood recipe, so the same explorer and move types can be
  reused by different runners with different incremental-evaluation sets.

The generic aggregation implementations remain MWE-local for now. The public
framework composition surface introduced here is limited to attaching cost
components to a solution-manager recipe and delta evaluators to a neighborhood
recipe.

## Neighborhood traversal

Deterministic traversal is lazy:

```cpp
auto candidates = neighborhood.moves(solution);
```

The deterministic neighborhood is authored through the incremental
`first_move`/`next_move` cursor protocol and adapted to an `input_range` by
`easylocal::cursor_moves`. It is not implicitly materialized into a container,
and deterministic authoring does not require ordinal indexing.

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

## Neighborhood composition

The public `easylocal::neighborhood_union(...)` helper composes two or more
neighborhood recipes before an instance is bound:

```cpp
auto combined = neighborhood_union(
    neighborhood<RelocateNeighborhood>(),
    neighborhood<SwapNeighborhood>(),
    neighborhood<AnotherNeighborhood>());
```

At `bind(instance)`, every child explorer is constructed from the same bound
solution manager. The resulting composite explorer owns the children and
behaves as one deterministic neighborhood. `moves(solution)` lazily concatenates
child traversals in declaration order; no complete neighborhood is materialized.

Child explorers may use different `move_type`s. The composite move keeps the
originating child encoded in its type-safe tagged variant so `make_move()` can
dispatch without virtual calls. The same mechanism also distinguishes equal C++
move types originating from different child neighborhoods.

Random traversal of a union is deliberately deferred. The accepted propagation
rule is that a union can guarantee `without_replacement` only when every child
does; the actual random mixing scheme is not selected yet.

## Search runner

Search algorithms are wired through the public recipe-based
`easylocal::Runner`. Service objects are not constructed by application code.
Instead, the runner records the concrete service types and any constructor
arguments:

```cpp
auto manager =
    solution_manager<SolutionManager>()
        .with_component<CapacityCostComponent>();

auto nhe =
    neighborhood<NeighborhoodExplorer>()
        .with_delta<
            CapacityCostComponent,
            ReassignCapacityDeltaEvaluator>();

auto runner =
    Runner{FirstImprovement{params}}
        .with_solution_manager(manager)
        .with_neighborhood(nhe);
```

The equivalent pipeline syntax is also supported:

```cpp
auto runner =
    Runner{FirstImprovement{params}}
    | (solution_manager<SolutionManager>()
       | component<CapacityCostComponent>())
    | (neighborhood<NeighborhoodExplorer>()
       | delta<
             CapacityCostComponent,
             ReassignCapacityDeltaEvaluator>());
```

`bind(instance)` materializes an instance-bound graph owned by an internal,
non-movable bound runner: first the solution manager, then the neighborhood
explorer. A single run then supplies only its initial solution and any
algorithm-specific runtime dependencies such as an RNG:

```cpp
auto bound = runner.bind(instance);
auto result = bound.run(initial_solution);
```

This keeps service construction state reusable before an instance is loaded,
while ownership and graph consistency remain internal to the runner.

## Deferred

The current MWE deliberately does not define:

- public generic cost/component/aggregation concepts;
- lazy component or delta evaluation;
- evaluation-state caching;
- proxy lifetime/generation invalidation;
- move undo/reversibility;
- propagation of delta-evaluator capabilities through `neighborhood_union`;
- fallback/capability negotiation between random sampling strategies;
- diagnostics for fallback from without- to with-replacement;
- EL3 neighborhood adapters;
- biased/adaptive sampling;
- generic indexed/combinatorial move-space helpers;
- floating-point semantics;
- CLI/configuration;
- tracing/logging.
