# Semantic search tracing

EasyLocal separates framework diagnostics (`<easylocal/logging.hpp>`) from
search instrumentation (`<easylocal/trace.hpp>`). Tracing produces typed search
events suitable for trajectory analysis; it is not a progress-log facility.

The ordinary `run(...)` overload instantiates a `trace::null_tracer`. Its
`observes<Event>` value is `false`, so event construction and emission are
removed with `if constexpr`. Applications that need data pass a tracer explicitly:

```cpp
using cost_type = /* runner cost type */;
easylocal::trace::memory_recorder<cost_type> trace;
auto result = bound_runner.run(initial_solution, trace);
```

First/Best Improvement emit run lifecycle, move evaluation/acceptance and local
optimum events. Simulated Annealing additionally emits incumbent updates.

## Composite-neighborhood provenance

Moves produced by `neighborhood_union` already carry the originating child in
their variant type. Tracing extracts that information recursively, so nested
unions produce routes such as `[0, 2, 1]` without adding runtime tags to normal
moves.

Random union sampling can also emit `neighborhood_selection` events containing:

- the selected child and hierarchical route;
- configured child bias;
- total active bias at that decision;
- conditional branch-selection probability;
- attempt number and whether the selected child produced a move.

This deliberately describes branch selection, not the probability of an
individual move: a child neighborhood is not required to sample its own moves
uniformly.

The data is intended to support trajectory statistics, Search Trajectory
Networks, Local Optima Networks, and later experiments with adaptive
neighborhood-sampling policies. Adaptive/RL policies are not part of the current
tracing machinery.

## Persistence

`trace::memory_recorder<Cost>` owns copies of recorded events. Its neighborhood
routes are copied only when tracing is active. `trace::write_jsonl(out, recorder)`
serializes the completed trace as one JSON object per line, leaving file naming,
directory layout and stream ownership to the application.

Serialization is intentionally outside the search loop. Core does not choose a
trace path and does not depend on a JSON or logging library.

## Overhead benchmark

With `EASYLOCAL_BUILD_BENCHMARKS=ON`, the `easylocal_trace_benchmark` target
compares four modes on the same First Improvement workload:

- ordinary baseline run;
- explicit `null_tracer`;
- a counter-only tracer;
- the full in-memory recorder.

This benchmark is a regression probe, not a promise about a particular machine.
The important contract is that the disabled path does not construct events or
dispatch through a runtime logging interface.

## Deferred extensions

A future optional sized-neighborhood concept may expose the number of available
moves for a solution. That could support union sampling proportional to child
cardinality (and therefore uniform sampling across all component moves when the
children themselves sample uniformly). It is intentionally outside S42.
