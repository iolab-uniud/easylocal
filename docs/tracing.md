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
`run_finished`, with the termination reason (preceded by `local_optimum` when
that is the reason), and `random_move()` forwards `neighborhood_selection` events. Algorithms that
track a best-so-far solution, such as Simulated Annealing, call
`incumbent_updated()`; custom events can be sent with `run.emit(event)`.

`start()` and `commit()` also emit `solution_visited`, with the hash of the
solution reached (`solution_hash`, see
[SolutionManager](reference/solution-manager.md)) and its cost: the nodes of
Search Trajectory Networks and Local Optima Networks. It is emitted only when
the problem has a solution hash, and the hash is computed only when the tracer
observes the event, so the hash costs nothing to a run that does not record
it. The 64-bit hash is a `u64` in ELTR and, in JSONL and in the output of
`eltr.py`, a string of 16 hexadecimal digits (`"00000000feedface"`): as a JSON
number, JavaScript and jq would round it to a double. Every recorder observes every core event; a run that builds no trajectory
or local optima network leaves the visited solutions out at compile time with
`trace::without`, which wraps any tracer and hides the given event templates
from the search:

```cpp
easylocal::trace::binary_recorder<cost_type> recorder{out};
auto trace = easylocal::trace::without<easylocal::trace::event::solution_visited>(recorder);
runner.run(solution, rng, easylocal::with(trace));
```

The solvers emit `run_context` before each run they start, with the name and
index of the pipeline stage (empty and 0 outside a pipeline) and the attempt,
or the start of a MultiStart, from 0: in a trace of a whole solve, it tells the
runs apart. It carries no cost, so a tracer receives it from the stages that
run on another cost, such as a pipeline's `until_feasible()` stage on the hard
cost, whose other events a recorder of the full cost does not observe. A run
started outside a solver, with `runner.run(...)` or a registered runner of an
app, has no `run_context`.

Tabu search adds `aspiration_applied`, when the move just
applied was tabu and admitted by the aspiration criterion, `tabu_escape`,
after the random moves of a reactive list's escape, with the number applied
(the run may stop before the escape ends), and `tabu_tenure_changed`, with the
previous and the new tenure of a list with one tenure for all its moves, at the
start of the run (previous tenure 0) and at each change; `RandomFoo` draws its
first tenure at the first move, and traces it then.

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
routes are copied only when tracing is active. `recorder.replay(tracer)` emits
the recorded events again, in order, to another tracer: to a streaming recorder
below, it writes the completed trace in that recorder's format.
`trace::write_jsonl(out, recorder)` is the replay to a `jsonl_recorder`: one JSON
object per line, as during the run. File naming, directory layout and stream
ownership are left to the application.

For long runs, EasyLocal provides constant-memory streaming recorders.
`trace::jsonl_recorder<Cost>` writes a human-readable JSONL representation. Its
first line, written at construction, is a header, the same line `eltr.py`
starts its output with, without the ELTR cost layout: the format version and
the metadata of `jsonl_options`, whatever tells the run apart, as text.

```cpp
easylocal::trace::jsonl_recorder<cost_type> trace{
    out,
    {.metadata = {{"instance", "ta001"}, {"seed", "42"}}}};
// {"event":"trace","version":1,"metadata":{"instance":"ta001","seed":"42"}}
```

`write_jsonl(out, recorder, options)` takes the same options.
The canonical experimental format is the versioned `ELTR` binary stream.  It
describes itself: a header gives the metadata of the run, the layout of the
costs and the fields of every event, so a reader needs nothing but the file.
Its records are tagged and length-prefixed, so readers can skip event types
they do not understand.

`trace::binary_recorder<Cost>` is an alias for the synchronous
`trace::buffered_binary_recorder<Cost>`.  It encodes events directly into a
contiguous block (256 KiB by default) and writes a block at a time instead of
calling `std::ostream` for each field.  `trace::async_binary_recorder<Cost>` uses
the same encoder and byte format, but hands completed blocks to one background
writer thread.  The producer owns one block while a bounded pool of preallocated
blocks connects it to the writer; if the writer falls behind, the producer
blocks rather than losing events.  Event order is therefore deterministic and
tracing is lossless.

Both recorders write and flush the header when they are constructed, so the
file of a run that crashes is still a trace: it decodes up to the last block
written, and loses only the events still buffered, at most a block
(`block_size`) for the synchronous recorder, the current block and the queued
ones for the asynchronous one.  `eltr.py` reports a file with no bytes at all
as an empty trace.

```cpp
std::ofstream out{"run-0042.eltrace", std::ios::binary};
easylocal::trace::async_binary_recorder<cost_type> trace{out};
auto result = search.run(initial_solution, easylocal::with(trace));
trace.flush();              // drain the writer and expose I/O failures
if (!trace.good()) { /* handle output failure */ }
```

An output error of the asynchronous recorder stops the recording, not the
search: from then on `emit` drops the events, `good()` is false and `flush()`
throws `std::ios_base::failure`, also when the final flush of the stream is
what fails.

Block size and the number of queued blocks are explicit policy knobs, not global
state, and the options carry the metadata written in the header: whatever tells
the run apart, as text.

```cpp
easylocal::trace::binary_buffer_options options{
    .block_size = 256 * 1024,
    .async_queue_blocks = 4,
    .metadata = {{"instance", "ta001"}, {"runner", "ts"}, {"seed", "42"}},
};
easylocal::trace::async_binary_recorder<cost_type> trace{out, options};
```

The asynchronous recorder allocates its queued blocks, and the one the search
fills, at construction: `async_queue_blocks` is finite, and
`easylocal::unlimited` throws `std::invalid_argument`.

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

### ELTR layout

All integers are little-endian; a string is a `u32` size and UTF-8 bytes. A
stream starts with `ELTR`, the format version as a `u32` (1) and the header: its
size (`u32`), then

- the metadata: a `u32` count, then a key and a value string each;
- the cost layout: a field list;
- the core event schemas: a `u32` count, then each event's tag (`u8`), name
  (string) and field list.

A field list is a `u32` count, then each field's name (string) and type
(`u8`): 1 `u8`, 2 `i8`, 3 `u16`, 4 `i16`, 5 `u32`, 6 `i32`, 7 `u64`, 8 `i64`,
9 `f32`, 10 `f64`, 11 `bool`, 12 string, 13 bytes (a `u32` size and the bytes),
14 route (a `u32` count and a `u32` per level), 15 cost (the fields of the cost
layout). A scalar cost is one field with an empty name; the fields of a
structured cost are named by their path, `hard.0` for the first level of the
hard part.

The records follow, each a tag (`u8`), its payload size (`u32`) and the
payload, the fields of its tag's schema in order. Tag 0 is a schema record (the
described tag, name and field list), written before the first record of an
application event that describes itself. The core events are:

| Tag | Event | Fields |
| --- | --- | --- |
| 1 | `run_started` | cost |
| 2 | `move_evaluated` | evaluations, iterations, current_cost, candidate_cost, neighborhood |
| 3 | `move_accepted` | evaluations, iterations, previous_cost, cost, neighborhood |
| 4 | `incumbent_updated` | evaluations, iterations, previous_cost, cost |
| 5 | `local_optimum` | evaluations, iterations, cost |
| 6 | `neighborhood_selection` | attempt, child, bias, active_bias_total, conditional_probability, produced_move, neighborhood |
| 7 | `run_finished` | evaluations, iterations, cost, termination (its name, as `to_string` writes it) |
| 8 | `solution_visited` | evaluations, iterations, hash, cost |
| 9 | `aspiration_applied` | evaluations, iterations, cost |
| 10 | `tabu_escape` | evaluations, iterations, moves |
| 11 | `tabu_tenure_changed` | evaluations, iterations, previous_tenure, tenure |
| 12 | `run_context` | stage (string), stage_index, attempt |
| 128–255 | application events | as their schema says, if they have one |

The counters, the stage index and the attempt are `u64`, the biases and the probability `f64`, `produced_move` a
`bool`, the neighborhoods routes; the header has the authoritative list.

### Decoding ELTR

`scripts/eltr.py` decodes a trace (standard-library Python, `python3` or
`uv run`) from its own description, with no options. By default it writes JSON
Lines: a first `trace` line with the format version, the metadata and the cost
layout, then the records with the field names of `jsonl_recorder`, so a decoded
binary trace feeds the same tools as a JSONL one:

```sh
scripts/eltr.py run-0042.eltrace > run-0042.jsonl
scripts/eltr.py run-0042.eltrace --events incumbent_updated,run_finished
scripts/eltr.py run-0042.eltrace --format summary   # metadata, event counts, runs
scripts/eltr.py run-0042.eltrace --format stn       # search trajectory network
scripts/eltr.py run-0042.eltrace --format schema    # what the trace records
```

A cost decodes to a number, or to an object nested as its fields are named,
with lists for numbered levels: `{"hard": [0, 2], "soft": 13.5}`; NaN and the
infinities become `null`, as in the JSONL recorder, so the output is strict
JSON. An application
event without a schema is kept as its tag and the hexadecimal payload, and a run
interrupted mid-record is read up to its last whole record with
`--allow-truncated`. The summary lists the runs in order, each with its initial
and final cost, its effort and its termination, and with its stage, stage index
and attempt when a `run_context` came before it. As a module, `eltr.Trace(stream)` reads the header
(`metadata`, `cost_fields`, `schemas`) and iterates over the records.

The `stn` format builds the network from the `solution_visited` events, which
are recorded when the problem has a [solution hash](reference/solution-manager.md):
one node per distinct hash, with its cost and number of visits, and one edge per
consecutive pair of visits within a run.

### Extending ELTR without touching EasyLocal

The binary encoder is intentionally small enough to customize in application
code.  The default cost writer, `default_binary_cost_writer<Cost>`, encodes an
arithmetic cost, a `cost::lexicographic` and a `cost::hierarchical`, nested as
they are. Another cost needs a writer, which receives a `binary_record_writer`,
whose primitive operations (`u8`, `i8`, `u16`, `i16`, `u32`, `i32`, `u64`,
`i64`, `f32`, `f64`, `boolean`, `bytes`, `string` and `route`) always use the
ELTR representation, and describes what it writes with `fields()`, for the
header.  The writer does not calculate payload sizes and does not know about
block or thread management:

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

    static std::vector<easylocal::trace::binary_field> fields()
    {
        using enum easylocal::trace::binary_type;
        return {{"hard", i64}, {"soft", i64}};
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

// Optional: the schema, written before the first record of the tag.
easylocal::trace::binary_event_schema describe_binary_event(
    std::type_identity<temperature_changed>)
{
    using enum easylocal::trace::binary_type;
    return {"temperature_changed", {{"iteration", u64}, {"temperature", f64}}};
}
} // namespace my_problem
```

No EasyLocal specialization is required.  A binary recorder automatically
advertises `observes<my_problem::temperature_changed>` and the ordinary
`trace::emit(...)` path can emit it.  This keeps problem-specific instrumentation
close to the problem code while the recorder owns buffering and persistence.
Tag ownership is application-level metadata; independent extensions should
coordinate their user-tag assignments when they share a trace schema.

A cost writer or an `encode_binary_event` that throws leaves no part of its
record in the trace: the binary recorders drop the whole record (and the schema
written before it, which the next record of the tag writes again), and the
JSONL recorder formats a line before writing it at once.  The exception reaches
the caller of `emit`.

By default, JSONL persistence accepts costs that can be inserted into an
`std::ostream`. A number is written as the shortest text that reads back to the
same value, and NaN and the infinities, which JSON has no numbers for, as
`null`; any other cost is inserted with the precision that keeps its numbers.
Domain-specific or structured costs can instead provide a small compile-time
writer; the writer owns the JSON representation and adds no virtual dispatch:

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
