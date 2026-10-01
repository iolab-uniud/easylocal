# Assignment MWE

## Runnable TwoStage example

`main.cpp` is the end-to-end user-facing example for the current public API. It
defines the application-owned `AppParameters` block next to `main`, uses
`FirstImprovementParameters` from the search-method header, constructs the
runner, and combines `application.*` with the runner-provided
`solver.search.max_evaluations` subtree in a read-only `config::root(...)`.
The same values are then used to load the versioned instance from
`instances/small.assignment`, construct a `FirstImprovement` runner, and solve
the immutable instance through the hierarchical `solvers::TwoStage`. The first
stage optimizes only hard feasibility; the second sees the full hard/soft cost.

With examples enabled (the default for a top-level build), run it with:

```sh
./build/<preset>/examples/assignment/easylocal_assignment_mwe
```

The MWE is also a minimal CLI-configured application. `--help` is generated
from the configuration tree, and values can be overridden before binding, for
example:

```sh
./build/<preset>/examples/assignment/easylocal_assignment_mwe \
  --solver.search.max_evaluations=25
```


The same configuration can be supplied from a compact file. File values are
applied before CLI values, so explicit CLI options win:

```sh
./build/<preset>/examples/assignment/easylocal_assignment_mwe \
  --config examples/assignment/configs/small.cfg \
  --solver.search.max_evaluations=25
```

The example deliberately uses the recipe/pipeline API rather than constructing
framework services manually, so it is suitable as a minimal starting point for
a user program.

This concrete model is used to discover the EasyLocal API.

It is deliberately **outside** `include/easylocal/` and is not part of the
public framework API.

## Problem

Each job has a non-negative demand and is assigned to one machine. Each machine
has a non-negative capacity.

The hard capacity component is structured:

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

The hard cost is the lexicographic pair:

```text
(total_overload, overloaded_machines)
```

so the original total-overload objective remains primary, while the number of
overloaded machines is a deterministic secondary level. The soft component is
the machine-load imbalance `max(load) - min(load)`. The complete cost is:

```text
hierarchical(
    hard = lexicographic(total_overload, overloaded_machines),
    soft = load_imbalance)
```

Hard improvement always dominates soft improvement.

Example:

```text
demand   = [4, 3, 2]
capacity = [5, 5]
solution = [0, 0, 1]
```

Loads are `[7, 2]`, so the capacity value is `CapacityValue{1, 2}`, the hard
cost is `HardCost{2, 1}`, and the soft load imbalance is `5`.

ReassignJobMove `(job=1, destination=1)` produces:

```text
solution = [0, 1, 1]
loads    = [4, 5]
hard cost = (0, 0)
soft cost = 1
```

## Cost composition

`CapacityCostComponent` performs full eager evaluation and returns a materialized
`CapacityValue`. The problem-side `AssignmentSolutionManager` remains responsible
for solution validity and aggregation policy, while cost components are attached to
the runner recipe compositionally:

```cpp
auto manager =
    solution_manager<AssignmentSolutionManager>()
    | component<CapacityCostComponent>();
```

The bound configured manager eagerly evaluates every attached component and
passes their materialized values to the problem-side aggregation policy. The
resulting `cost_type` remains the ordinary three-way-comparable value seen by
search algorithms. Each component type may occur at most once in a manager
recipe; semantically distinct parameterizations can use distinct wrapper or
subclass types and therefore distinct compile-time identities.

The framework cost models (`easylocal::cost`) provide:

- `cost::weighted_sum`, a configurable aggregator;
- `cost::lexicographic`, a lexicographically ordered cost;
- `cost::hierarchical`, a hard/soft cost.

`AssignmentCostAggregator` maps the structured capacity value to the
lexicographic `HardCost` through its optional `hard(...)` projection and combines
that branch with `LoadImbalanceCostComponent` into the full `hierarchical` cost.
The aggregator is attached explicitly to the SolutionManager recipe; the
SolutionManager itself does not define the modern aggregation contract. The two
branches remain independently typed and may themselves be aggregate or
lexicographic costs. Domain-specific projection remains explicit; no projection
DSL is introduced.

The generic cost models are part of the public framework API in
`<easylocal/cost.hpp>` under `easylocal::cost`. The Assignment
example supplies only the domain-specific projections from component values to
its hard and full hierarchical costs.

## Delta evaluation

The assignment MWE also prototypes a separate delta evaluator for the pair
`CapacityCostComponent x ReassignJobMove`:

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
    neighborhood<ReassignJobNeighborhoodExplorer>()
    | delta<CapacityCostComponent, ReassignCapacityDeltaEvaluator>();
```

The runner's internal evaluation facility matches active component types against
the deltas attached to that particular neighborhood recipe. A delta attached to
a component that is not active in the paired solution-manager recipe is a
compile-time error at bind. If an active component has no matching delta, the
framework materializes the candidate `AssignmentSolution` once and reuses it
for every fallback full-component evaluation. If all components have deltas, a rejected
candidate never requires `make_move`; an accepted candidate applies the move
exactly once. If fallback materialization was already necessary, acceptance
promotes that materialized candidate instead of applying the move again.

The search algorithms see only eager materialized evaluations and `cost_type`
values. They do not select deltas, distinguish fallback components, or manage
candidate materialization. `evaluate_move` computes a candidate without changing
the incumbent, while `commit` is the operation that promotes the accepted
candidate. Candidate storage is specialized at compile time: an all-delta path
keeps the `ReassignJobMove`, while any fallback path keeps the already
materialized `AssignmentSolution`, so neither path pays for an unused
`std::optional<AssignmentSolution>`.
Laziness, caching and proxy lifetime/invalidation remain postponed.

## Responsibilities

`AssignmentInstance`
: Plain problem data. The example publishes it as a const object during use.

`AssignmentSolution`
: Plain value representation. It does not store an `AssignmentInstance`
  pointer/reference.

`ReassignJobMove`
: Plain descriptive value. It does not store an `AssignmentSolution` or
  `AssignmentInstance` pointer/reference.

`CapacityCostComponent`
: Full evaluator for one structured cost component, bound to an
  `AssignmentInstance`.

`CapacityDelta`
: Materialized structured change applicable to `CapacityValue` with `operator+`.

`ReassignCapacityDeltaEvaluator`
: Separate evaluator specialized for the capacity component and assignment
  reassign move.

`HardCost`
: Lexicographic materialized hard branch `(total_overload, overloaded_machines)`.

`Cost`
: Hierarchical materialized value `hierarchical(HardCost, SoftCost)`.

`AssignmentSolutionManager`
: Problem-side service responsible for structural solution validation and the
  aggregation policy. Cost-component storage/evaluation is supplied by the
  framework from the components attached to the manager recipe.

`ReassignJobNeighborhoodExplorer`
: Problem-side service responsible for neighborhood traversal, move validity and
  `make_move`. Delta evaluators are optional capabilities attached externally to
  a particular neighborhood recipe, so the same explorer and move types can be
  reused by different runners with different incremental-evaluation sets.

Generic aggregation belongs to the framework. The Assignment-specific code
keeps only the projection from its materialized component values into the chosen
framework aggregation policy. Component and delta attachment remain part of the
public recipe composition surface.

## Neighborhood traversal

Deterministic traversal is lazy:

```cpp
auto candidates = easylocal::moves(neighborhood, solution);
```

The deterministic neighborhood is authored through the incremental
`first_move`/`next_move` cursor protocol. The framework customization point
`easylocal::moves(neighborhood, solution)` adapts that protocol to an
`input_range`; a neighborhood may alternatively provide a native `moves()`
range/generator. If both protocols are present, cursor traversal has precedence.
No candidate container is materialized.

Random proposals are a separate capability from deterministic traversal:

```cpp
auto candidate = neighborhood.random_move(solution, rng);
```

`random_move()` returns `std::optional<Move>` and returns `std::nullopt` when no
move exists. The RNG is explicit. There is currently no public random traversal
range, replacement-policy vocabulary, or Random First Improvement search method;
those semantics are deliberately deferred until a stronger sampling model is
designed.

Traversal ranges compose with standard views:

```cpp
auto filtered =
    neighborhood.moves(solution)
    | std::views::filter(predicate);
```

In debug builds, dereferencing a previously-created move range after the
underlying `AssignmentSolution` representation changes triggers an
assertion. The check
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

If every child exposes `random_move(solution, rng)`, the union exposes the same
capability. Child selection is uniform by default and may be biased explicitly:

```cpp
auto combined =
    neighborhood_union(
        neighborhood<RelocateNeighborhood>(),
        neighborhood<SwapNeighborhood>(),
        neighborhood<AnotherNeighborhood>())
    | random_biases(1.0, 2.0, 0.5);
```

The union chooses a child with probability proportional to its enabled bias and
then delegates move selection to that child. A zero bias disables a child for
random proposals without affecting deterministic `moves()` traversal. If a
selected child has no move for the current solution, it is removed from that
proposal attempt and the remaining biases are renormalized. The implementation
uses fixed-size storage and no heap allocation. Biases are currently recipe-time
configuration: runtime reconfiguration is intentionally deferred to the future
general parameter/configuration surface rather than exposed as a union-specific
mutable API.

Delta capability also composes per component. A union exposes a component delta
only when every child exposes that component; otherwise that component uses the
existing full-evaluation fallback. Tagged union moves dispatch the delta directly
to the originating child without virtual calls or heap allocation.

## Search runner

First Improvement, Best Improvement, and Simulated Annealing are public framework facilities under `easylocal::runners`; the Assignment example only supplies the model and services they consume.

Search algorithms are wired through the public recipe-based
`easylocal::Runner`. Service objects are not constructed by application code.
Instead, the runner records the concrete service types and any constructor
arguments:

```cpp
auto manager =
    solution_manager<AssignmentSolutionManager>()
        .with_component<CapacityCostComponent>()
        .with_component<LoadImbalanceCostComponent>()
        .with_aggregator(AssignmentCostAggregator{});

auto nhe =
    neighborhood<ReassignJobNeighborhoodExplorer>()
        .with_delta<
            CapacityCostComponent,
            ReassignCapacityDeltaEvaluator>();

auto runner =
    make_runner<runners::FirstImprovement>(params)
        .with_solution_manager(manager)
        .with_neighborhood(nhe);
```

The equivalent pipeline syntax is also supported:

```cpp
auto runner =
    make_runner<runners::FirstImprovement>(params)
    | (solution_manager<AssignmentSolutionManager>()
       | component<CapacityCostComponent>()
       | component<LoadImbalanceCostComponent>()
       | aggregator(AssignmentCostAggregator{}))
    | (neighborhood<ReassignJobNeighborhoodExplorer>()
       | delta<
             CapacityCostComponent,
             ReassignCapacityDeltaEvaluator>());

auto solver = make_solver<solvers::TwoStage>(
    std::move(runner),
    solvers::TwoStageConfig<initialization::Initial>{
        .initialization = initialization::initial,
    });

auto result = solver.solve(instance);
```

`bind(instance)` materializes an instance-bound graph owned by an internal,
non-movable bound runner: first the solution manager, then the neighborhood
explorer. For the hierarchical Assignment model, `solvers::TwoStage` derives the first-stage
hard view automatically and reuses the full runner for the second stage. When
both stages share the same runner, the one-runner factory overload copies the
configuration internally, keeping the user-facing construction concise.
Service construction state remains reusable before an instance is loaded, while
ownership and graph consistency remain internal to the runner/solver.

## Deferred

The current MWE deliberately does not define:

- public generic cost/component/aggregation concepts;
- lazy component or delta evaluation;
- evaluation-state caching;
- proxy lifetime/generation invalidation;
- move undo/reversibility;
- future random-traversal/replacement semantics;
- EL3 neighborhood adapters;
- runtime/adaptive neighborhood-selection parameters;
- generic indexed/combinatorial move-space helpers;
- floating-point semantics;
- generic parameter exposure through direct C++, CLI, and configuration files;
- tracing/logging.

## TextUI run-control demo

When the optional TextUI component is enabled,
`easylocal_assignment_tester_mwe` starts from
`instances/large.assignment` (250 jobs, 16 machines) and registers two runners:

- `fi`, the ordinary first-improvement runner;
- `slow-fi`, a demo-only decorator around the same first-improvement algorithm.

`slow-fi` delays each evaluation by 5 ms and uses a 2000-evaluation budget. The
delay exists only in Assignment demo/test code; it is not framework behavior and
does not modify the production search algorithm. The same decorator is shared by
the TextUI demo and REST HTTP integration test. In the TextUI, select `slow-fi`,
start a run, and use `X Stop` to exercise asynchronous progress, cooperative stop,
and partial-solution commit.

The ordinary Assignment MWE continues to use `instances/small.assignment`.

## REST MWE

When the optional REST component is enabled, `easylocal_assignment_rest_mwe`
mounts the generic EasyLocal Crow Blueprint at `/assignment` and listens on port
18080 by default. Optional arguments are the port and completed-run retention
capacity, for example `easylocal_assignment_rest_mwe 18081 16`. The retention
default is 64. The server registers both `fi` and the demo-only `slow-fi`.

Run creation uses the generic REST envelope. The Assignment codec accepts either
a structured JSON object:

```sh
curl -X POST http://localhost:18080/assignment/runners/fi/runs \
  -H 'Content-Type: application/json' \
  -d '{"input":{"demand":[4,4,2],"capacity":[5,5]}}'
```

or the existing text representation as an opaque JSON string:

```sh
curl -X POST http://localhost:18080/assignment/runners/fi/runs \
  -H 'Content-Type: application/json' \
  -d '{"input":"3 2 4 3 2 5 5"}'
```

The response is `202 Accepted`, contains `id`, and sets `Location` to the run
resource. Use `GET /assignment/runs/<id>` for status/progress and
`GET /assignment/runs/<id>/solution` after completion. Cooperative cancellation
is `POST /assignment/runs/<id>/cancel`; `DELETE /assignment/runs/<id>` is reserved
for forgetting a terminal run and returns `204`. The CTest `easylocal.rest-http`
exercises these semantics, error mapping, partial results, and bounded retention
with real `curl` requests.

The example deliberately leaves the Crow server visible: EasyLocal owns the
Blueprint and solver execution pool, while the application still owns Crow
port/concurrency/server lifecycle.
