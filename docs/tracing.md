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

For long runs, `trace::jsonl_recorder<Cost>` writes the same JSONL representation
incrementally to an application-owned `std::ostream` and does not retain the
event history. The recorder never flushes per event; an `std::ofstream` therefore
uses the normal buffering of its stream buffer and recorder-owned memory remains
constant. `flush()` is available when the application needs an explicit
durability boundary.

```cpp
std::ofstream out{"run-0042.trace.jsonl"};
easylocal::trace::jsonl_recorder<cost_type> trace{out};
auto result = bound_runner.run(initial_solution, trace);
trace.flush();
```

The memory recorder is intended for tests, short traces and in-process analysis;
the streaming recorder is the appropriate default when a run may emit millions
of events. Core does not choose a trace path and does not depend on a JSON or
logging library.

By default, JSONL persistence accepts costs that can be inserted into an
`std::ostream`. Domain-specific or structured costs can instead provide a small
compile-time writer; the writer owns the JSON representation and adds no virtual
dispatch:

```cpp
struct cost_json
{
    void operator()(std::ostream& out, const Cost& cost) const
    {
        out << "{\"hard\":" << cost.hard_value()
            << ",\"soft\":" << cost.soft_value() << '}';
    }
};

easylocal::trace::jsonl_recorder<Cost, cost_json> trace{out, cost_json{}};
```

The same writer can be passed as the third argument to `write_jsonl(...)` when a
`memory_recorder` is serialized after the run.

## Overhead benchmark

With `EASYLOCAL_BUILD_BENCHMARKS=ON`, the `easylocal_trace_benchmark` target
compares the following modes on the same First Improvement workload:

- ordinary baseline run;
- explicit `null_tracer`;
- a counter-only tracer;
- the full in-memory recorder;
- incremental EasyLocal JSONL serialization into a discard stream, isolating
  serialization overhead from filesystem I/O;
- when a system spdlog installation is found, equivalent synchronous spdlog JSONL
  formatting into a formatting discard sink.

spdlog is used only by this benchmark. It is discovered with `find_package` and
is never fetched by EasyLocal; `EASYLOCAL_BENCHMARK_REQUIRE_SPDLOG=ON` makes it a
required benchmark dependency. The Trace Microbenchmarks GitHub Actions workflow
installs it explicitly on Linux and macOS, runs ten process-level Release trials
for pull requests and release tags, and publishes the raw CSV as both a job
summary and artifact.

This benchmark is a regression probe, not a promise about a particular machine.
The important contract is that the disabled path does not construct events or
dispatch through a runtime logging interface.

## Deferred extensions

A future optional sized-neighborhood concept may expose the number of available
moves for a solution. That could support union sampling proportional to child
cardinality (and therefore uniform sampling across all component moves when the
children themselves sample uniformly). It is intentionally outside S42.
