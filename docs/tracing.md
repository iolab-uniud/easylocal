# Semantic search tracing

EasyLocal separates framework diagnostics (`<easylocal/logging.hpp>`) from
search instrumentation (`<easylocal/trace.hpp>`). Tracing produces typed search
events suitable for trajectory analysis; it is not a progress-log facility.

An ordinary `run(...)` call instantiates a `trace::null_tracer`. Its
`observes<Event>` value is `false`, so event construction and emission are
removed with `if constexpr`. Applications that need data pass a tracer explicitly
as the trailing run option:

```cpp
using cost_type = /* runner cost type */;
easylocal::trace::memory_recorder<cost_type> trace;
auto result = bound_runner.run(initial_solution, easylocal::with(trace));
```

Core events are emitted by the framework-owned `easylocal::search_run`, not by
the individual algorithms: `start()` emits `run_started`, `evaluate_move()`
emits `move_evaluated`, `commit()` emits `move_accepted`, `finish()` emits
`run_finished` (preceded by `local_optimum` when that is the termination reason)
and `random_move()` forwards `neighborhood_selection` events. Algorithms that
track a best-so-far solution, such as Simulated Annealing, call
`incumbent_updated()`; custom events can be sent with `run.emit(event)`.

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

With `EASYLOCAL_BUILD_BENCHMARKS=ON`, `easylocal_trace_benchmark` compares the
same First Improvement workload under:

- ordinary baseline execution;
- explicit `null_tracer`;
- counter-only tracing;
- the full in-memory recorder;
- block-buffered ELTR binary recording to a discard stream;
- asynchronous ELTR recording, with producer and total drain time reported
  separately;
- synchronous and asynchronous ELTR recording to temporary files.

JSONL is intentionally not part of the timing comparison. Its formatting cost is
a property of the textual serialization policy and obscures the lower-level trace
boundary that this regression probe is intended to track. JSONL functionality is
still covered by deterministic correctness tests.

The benchmark checksum consumes the final cost, termination reason, evaluation
count, and solution contents. This prevents optimizer dead-code elimination from
making tracer specializations incomparable. Temporary-file modes flush the C++
stream but deliberately do not call `fsync`; they measure cost visible to the
search process, including OS page-cache interaction, rather than a durability
guarantee.

A second target, `easylocal_trace_cost_encoding_benchmark`, isolates ELTR cost
encoding using equivalent `move_evaluated` events for three representative cost
shapes: scalar `int64`, a two-component lexicographic cost, and a hierarchical
cost containing that hard lexicographic branch plus one soft component. It
reports nanoseconds and serialized bytes per event.

The Trace Microbenchmarks GitHub Actions workflow runs both targets in Release on
Linux/GCC and macOS/AppleClang and publishes raw CSV results. These numbers are
regression diagnostics, not cross-machine performance promises.

## Deferred extensions

A future optional sized-neighborhood concept may expose the number of available
moves for a solution. That could support union sampling proportional to child
cardinality (and therefore uniform sampling across all component moves when the
children themselves sample uniformly). It is intentionally outside S42.
