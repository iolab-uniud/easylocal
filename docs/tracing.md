# Semantic search tracing

EasyLocal separates framework diagnostics (`<easylocal/utils/logging.hpp>`) from
search instrumentation (`<easylocal/trace.hpp>`). Tracing produces typed search
events suitable for trajectory analysis; it is not a progress-log facility.

An ordinary `run(...)` call instantiates a `trace::null_tracer`. Its
`observes<Event>` value is `false`, so event construction and emission are
removed with `if constexpr`. Applications that need data pass a tracer explicitly
as the trailing run option:

```cpp
using cost_type = /* runner cost type */;
easylocal::trace::memory_recorder<cost_type> trace;
auto result = search.run(initial_solution, easylocal::with(trace));
```

Core events are emitted by the framework-owned `easylocal::search_run`, not by
the individual algorithms: `start()` emits `run_started`, `evaluate_move()`
emits `move_evaluated`, `commit()` emits `move_accepted`, `finish()` emits
`run_finished` (preceded by `local_optimum` when that is the termination reason)
and `random_move()` forwards `neighborhood_selection` events. Algorithms that
track a best-so-far solution, such as Simulated Annealing, call
`incumbent_updated()`; custom events can be sent with `run.emit(event)`.

`start()` and `commit()` also emit `solution_visited`, with the hash of the
solution reached (`solution_hash`, see
[SolutionManager](reference/solution-manager.md)) and its cost: the nodes of
Search Trajectory Networks and Local Optima Networks. It is emitted only when
the problem has a solution hash, and the hash is computed only when the tracer
observes the event. Tabu search adds `aspiration_applied`, when the move just
applied was tabu and admitted by the aspiration criterion, and `tabu_escape`,
before the random moves of a reactive list's escape.

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
auto result = search.run(initial_solution, trace);
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

### ELTR records

A stream starts with `ELTR` and the format version as a little-endian `u32`
(1). Each record is a tag (`u8`), the payload size (`u32`) and the payload; all
integers are little-endian, a route is a `u32` count followed by one `u32` per
level, and a cost is written by the recorder's cost writer (the default writes
an arithmetic cost as `i64`, `u64` or `f64`).

| Tag | Event | Payload |
| --- | --- | --- |
| 1 | `run_started` | cost |
| 2 | `move_evaluated` | evaluations `u64`, iterations `u64`, current cost, candidate cost, route |
| 3 | `move_accepted` | evaluations `u64`, iterations `u64`, previous cost, cost, route |
| 4 | `incumbent_updated` | evaluations `u64`, iterations `u64`, previous cost, cost |
| 5 | `local_optimum` | evaluations `u64`, iterations `u64`, cost |
| 6 | `neighborhood_selection` | attempt `u64`, child `u64`, bias `f64`, active bias total `f64`, conditional probability `f64`, produced move `u8`, route |
| 7 | `run_finished` | evaluations `u64`, iterations `u64`, cost |
| 8 | `solution_visited` | evaluations `u64`, iterations `u64`, hash `u64`, cost |
| 9 | `aspiration_applied` | evaluations `u64`, iterations `u64`, cost |
| 10 | `tabu_escape` | evaluations `u64`, iterations `u64`, moves `u64` |
| 128–255 | application events | as their encoder writes them |

### Decoding ELTR

`scripts/eltr.py` decodes a trace (standard-library Python, `python3` or
`uv run`). By default it writes JSON Lines with the field names of
`jsonl_recorder`, so a decoded binary trace feeds the same tools as a JSONL one:

```sh
scripts/eltr.py run-0042.eltrace > run-0042.jsonl
scripts/eltr.py run-0042.eltrace --events incumbent_updated,run_finished
scripts/eltr.py run-0042.eltrace --format summary   # event counts, costs per run
scripts/eltr.py run-0042.eltrace --format stn       # search trajectory network
```

The trace does not record how its costs are encoded. `--cost` gives the layout
written by the recorder's cost writer: `i64` (the default, a signed integral
cost), `u64`, `f64`, or the fields of a structured writer in order, for example
`--cost hard:i32,soft:i32` for the writer below, which decodes each cost to an
object. A record whose payload does not fit the layout is an error, but a
layout of the right size with the wrong types is not detected. An application
event is kept as its tag and the hexadecimal payload, and a run interrupted
mid-record is read up to its last whole record with `--allow-truncated`.

The `stn` format builds the network from the `solution_visited` events, which
are recorded when the problem has a [solution hash](reference/solution-manager.md):
one node per distinct hash, with its cost and number of visits, and one edge per
consecutive pair of visits within a run.

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

constexpr std::uint8_t binary_event_tag(const temperature_changed&)
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

The tracing overhead is measured in
[easylocal-benchmarks](https://github.com/iolab-uniud/easylocal-benchmarks)
(`infrastructure/`) at every release, and shown on the
[Benchmarks](benchmarks.md) page. `easylocal_trace_benchmark` compares the same
First Improvement workload under:

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

These numbers are regression diagnostics, not cross-machine performance
promises.

## Deferred extensions

A future optional sized-neighborhood concept may expose the number of available
moves for a solution. That could support union sampling proportional to child
cardinality (and therefore uniform sampling across all component moves when the
children themselves sample uniformly). It is intentionally outside S42.
