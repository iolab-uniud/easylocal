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

For long runs, EasyLocal provides constant-memory streaming recorders.
`trace::jsonl_recorder<Cost>` writes a human-readable JSONL representation.
The canonical experimental format is the versioned `ELTR` binary stream.  Its
records are tagged and length-prefixed, so readers can skip event types they do
not understand.

`trace::binary_recorder<Cost>` is an alias for the synchronous
`trace::buffered_binary_recorder<Cost>`.  It encodes events directly into a
contiguous block (256 KiB by default) and writes a block at a time instead of
calling `std::ostream` for each field.  `trace::async_binary_recorder<Cost>` uses
the same encoder and byte format, but hands completed blocks to one background
writer thread.  The producer owns one block while a bounded pool of preallocated
blocks connects it to the writer; if the writer falls behind, the producer
blocks rather than losing events.  Event order is therefore deterministic and
tracing is lossless.

```cpp
std::ofstream out{"run-0042.eltrace", std::ios::binary};
easylocal::trace::async_binary_recorder<cost_type> trace{out};
auto result = bound_runner.run(initial_solution, trace);
trace.flush();              // drain the writer and expose I/O failures
if (!trace.good()) { /* handle output failure */ }
```

Block size and the number of queued blocks are explicit policy knobs, not global
state:

```cpp
easylocal::trace::binary_buffer_options options{
    .block_size = 256 * 1024,
    .async_queue_blocks = 4,
};
easylocal::trace::async_binary_recorder<cost_type> trace{out, options};
```

The async recorder is deliberately single-producer: it is designed to observe
one search trajectory.  Synchronization happens at block boundaries, not for
every event.  The writer thread may reduce search-thread stalls when persistence
is slower than encoding, but it does not make I/O free; benchmarks therefore
report producer time separately from total drain time.

The memory recorder is intended for tests, short traces and in-process analysis.
JSONL is convenient for inspection and interchange, while `ELTR` is preferred
when a run may emit millions of events and trace volume matters.  Core does not
choose a trace path and does not depend on a JSON, binary-serialization or
logging library.

### Extending ELTR without touching EasyLocal

The binary encoder is intentionally small enough to customize in application
code.  A structured cost writer receives a `binary_record_writer`, whose
primitive operations (`u8`, `u32`, `u64`, `i32`, `i64`, `f64`, `boolean`,
`bytes`, `string`, and `route`) always use the ELTR representation.  The writer
does not calculate payload sizes and does not know about block or thread
management:

```cpp
struct cost_binary
{
    void operator()(
        easylocal::trace::binary_record_writer& out,
        const Cost& cost) const
    {
        out.i64(cost.hard_value());
        out.i64(cost.soft_value());
    }
};

easylocal::trace::binary_recorder<Cost, cost_binary> trace{out, cost_binary{}};
```

Applications can also add their own semantic event types through ADL.  Tags
1..127 are reserved by EasyLocal; `user_binary_event_tag<N>()` maps application
indices 0..127 onto tags 128..255:

```cpp
namespace my_problem
{
struct temperature_changed
{
    std::uint64_t iteration;
    double temperature;
};

constexpr auto binary_event_tag(const temperature_changed&) -> std::uint8_t
{
    return easylocal::trace::user_binary_event_tag<0>();
}

void encode_binary_event(
    easylocal::trace::binary_record_writer& out,
    const temperature_changed& value)
{
    out.u64(value.iteration);
    out.f64(value.temperature);
}
} // namespace my_problem
```

No EasyLocal specialization is required.  A binary recorder automatically
advertises `observes<my_problem::temperature_changed>` and the ordinary
`trace::emit(...)` path can emit it.  This keeps problem-specific instrumentation
close to the problem code while the recorder owns buffering and persistence.
Tag ownership is application-level metadata; independent extensions should
coordinate their user-tag assignments when they share a trace schema.

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
- block-buffered EasyLocal binary serialization into the same kind of discard stream;
- asynchronous binary recording, reporting both producer time and total drain time;
- synchronous and asynchronous binary recording to temporary files, again keeping
  async producer and total drain timings distinct;
- when a system spdlog installation is found, equivalent synchronous spdlog JSONL
  formatting into a formatting discard sink.

spdlog is used only by this benchmark. It is discovered with `find_package` and
is never fetched by EasyLocal; `EASYLOCAL_BENCHMARK_REQUIRE_SPDLOG=ON` makes it a
required benchmark dependency. The Trace Microbenchmarks GitHub Actions workflow
installs it explicitly on Linux and macOS, runs ten process-level Release trials
for pull requests and release tags, and publishes the raw CSV as both a job
summary and artifact.

The benchmark also reports JSONL and binary bytes per emitted event on stderr, so
CPU overhead and trace volume can be considered together. Temporary-file modes
flush the C++ stream but deliberately do not call `fsync`; they measure the cost
visible to the search process, including OS page-cache interaction, rather than a
durability guarantee. It is a regression probe, not a promise about a particular
machine. The important contract is that
the disabled path does not construct events or dispatch through a runtime logging
interface.

## Deferred extensions

A future optional sized-neighborhood concept may expose the number of available
moves for a solution. That could support union sampling proportional to child
cardinality (and therefore uniform sampling across all component moves when the
children themselves sample uniformly). It is intentionally outside S42.
