# Neighborhood authoring spike (S15)

This directory is an opt-in design experiment. It is not part of the public
EasyLocal++ API and is not built by the normal example/test workflow.

The question under test is deliberately narrow:

> Can `NeighborhoodExplorer::moves(solution)` remain a lazy input-range contract
> for consumers while giving problem authors a substantially simpler way to
> write deterministic neighborhoods?

The spike now compares three candidate authoring styles on both the Assignment
and TSP MWEs. The old index-oriented `current` implementation is deliberately
excluded from the candidate set: indexing is not required by deterministic
traversal and should not survive merely as a benchmark reference.
`std::generator` is included only when the active C++23 standard library exposes
it.

## Variants

### 1. Spike-local coroutine generator

A small spike-local C++20 coroutine generator lets the problem author write the
neighborhood directly as nested loops:

```cpp
auto moves(const Solution& solution) const -> generator<Move>
{
    for (std::size_t job = 0; job < solution.assignment.size(); ++job)
    {
        for (std::size_t destination = 0;
             destination < machine_count;
             ++destination)
        {
            if (destination != solution.assignment[job])
            {
                co_yield Move{job, destination};
            }
        }
    }
}
```

The generator is intentionally local to this spike. It must not be interpreted
as a decision to add a public EasyLocal++ generator type.

### 2. Standard-library coroutine generator, when available

When `<generator>` and `__cpp_lib_generator` are available, the same coroutine
body is benchmarked as `coroutine-std` using `std::generator<Move>`. This lets
us distinguish coroutine language/codegen effects from standard-library effects
without assuming that the spike-local generator represents `std::generator`.

### 3. EL3-style cursor adapter

The promoted public `<easylocal/cursor_moves.hpp>` adapter maps the familiar
stateful protocol

```cpp
bool first_move(const Solution&, Move&) const;
bool next_move(const Solution&, Move&) const;
```

into the same lazy input-range consumed by algorithms:

```cpp
auto moves(const Solution& solution) const
{
    return easylocal::cursor_moves(*this, solution);
}
```

This path tests migration ergonomics and the possibility of a near-zero-overhead
adapter for EL3-style explorers.

## Deliberate exclusions

This iteration does **not** redesign:

- `random_moves()`;
- `random_move()` adapters;
- with/without-replacement semantics;
- `move_count()` / `move_at()` indexability;
- invalidation/generation tracking;
- public generator or cursor APIs.

Those are orthogonal capabilities. In particular, indexability is not assumed
to be part of deterministic traversal.

Deterministic traversal itself is also an optional neighborhood capability. A
random-only algorithm such as a Simulated Annealing implementation must be able
to consume an explorer that provides only the random traversal capability it
needs, without forcing the problem author to implement `moves(solution)`.

The promoted cursor adapter requires a default-initializable `Move` because the
EL3-style protocol writes the first value into an already existing `Move`. Native
custom ranges are not subject to that authoring constraint.

## Correctness check

`easylocal_neighborhood_authoring_check` materializes small neighborhoods only
for verification and checks custom-coroutine, standard-coroutine (when
available), and cursor variants against explicit expected move sequences for
both MWEs. The discarded index-oriented explorer is not used as a correctness
oracle.

The custom coroutine and cursor variants are also checked to remain input ranges
rather than accidentally becoming forward ranges.

## Hardened microbenchmark (S15b-S15d)

`easylocal_neighborhood_authoring_benchmark` performs full neighborhood scans in
Release mode. S15b deliberately makes every produced move observable through a
GCC/Clang compiler barrier. The barrier emits no intended runtime memory access,
but prevents the optimizer from replacing an entire deterministic traversal by
a closed-form checksum, which happened to the first TSP cursor benchmark.

Compiler optimization of the traversal abstraction itself is still allowed and
wanted: if a cursor or coroutine abstraction can be inlined away, the benchmark
should reflect that.

Two workloads are reported:

- `traversal`: minimal move encoding plus the compiler barrier;
- `light-consumer`: a small domain-specific O(1) consumer that reads incumbent
  and instance data, approximating the setting where enumeration feeds a delta
  or move evaluator without letting that evaluator dominate the measurement.

The benchmark covers `tiny`, `small`, `medium`, and `large` neighborhoods. Tiny
cases emphasize fixed per-traversal costs such as coroutine-frame setup; larger
cases emphasize steady-state per-move overhead.

Each case is measured for multiple independent timing trials. CSV rows have the
form:

```text
domain,workload,variant,neighborhood_size,trial,repetitions,measured_moves,ns_per_move,checksum
```

There are deliberately **no pass/fail performance thresholds**. This is a design
measurement, not a performance regression test. Compare distributions across
trials, compilers, standard libraries, machines, and authoring styles.

S15c adds `coroutine-std` wherever `std::generator` is available and a
separate allocation diagnostic. S15d removes the old `current` implementation
from both benchmark and correctness comparison because its index-oriented
semantics are not required by deterministic traversal. The allocation diagnostic
counts ordinary global allocations and bytes observed during one complete
traversal. It is intended to reveal likely coroutine-frame heap allocation; it
is diagnostic only and is not a portable frame-size contract.

The command-line arguments are:

```text
easy...benchmark [target_moves_per_trial] [trials] [seed]
```

Defaults are 2,000,000 moves per row/trial, 3 trials, and seed 123456789.

## Running locally

From the repository root:

```bash
./scripts/run-neighborhood-authoring-spike.sh
```

For a longer local run, for example:

```bash
./scripts/run-neighborhood-authoring-spike.sh 10000000 5 123456789
```

The correctness check and, by default, the allocation diagnostic are written
to stderr and benchmark CSV to stdout, so a run can be captured cleanly with:

```bash
./scripts/run-neighborhood-authoring-spike.sh 10000000 5 \
    > benchmark.csv
```

The script configures a separate Release build directory:

```text
build/neighborhood-authoring-spike
```

## Cross-toolchain runs

A separate manual GitHub Actions workflow,
`.github/workflows/neighborhood-authoring-spike.yml`, runs the same correctness
check and benchmark on the active toolchain matrix:

- Linux / GCC 15 + libstdc++;
- Linux / GCC 16 + libstdc++;
- Linux / Clang 22 + libstdc++;
- Linux / Clang 22 + libc++;
- macOS ARM64 / AppleClang + libc++;
- macOS ARM64 / GCC 16 + libstdc++.

The workflow uploads environment metadata, the correctness log, allocation
CSV, and benchmark CSV as artifacts. Performance values remain diagnostic only.

The Linux matrix can be reproduced locally with `act`:

```bash
./scripts/act-neighborhood-authoring-spike.sh
```

or for one toolchain and a longer measurement:

```bash
./scripts/act-neighborhood-authoring-spike.sh clang22-libcxx 5000000 5 123456789
```

`act` cannot reproduce the native macOS ARM64 jobs; those remain GitHub-hosted
workflow runs.

## Reproducible result bundle (S16d)

Use the suite wrapper when the output is meant to be retained or compared:

```bash
./scripts/run-neighborhood-authoring-benchmarks.sh \
    build/neighborhood-authoring-results \
    5000000 5 123456789
```

The wrapper creates one self-contained result directory containing:

```text
metadata.csv
allocations.csv
benchmark.csv
runner-benchmark.csv
first-improvement-diagnostic.csv
check.log
runner-check.log
first-improvement-diagnostic.log
summary.md
```

`metadata.csv` records the selected toolchain label, OS, architecture and hardware model, exact
compiler banner/target, standard-library family and version macro, CMake/Ninja
versions, C++ flags, benchmark parameters, and Git commit/dirty state. The raw
benchmark CSV schemas are treated as stable inputs by the summarizer: an
unexpected header is an error rather than being silently accepted.

`summarize-neighborhood-authoring.py` is Python-standard-library only. It
computes per-case trial medians and min/max timing values, reports relative
ratios against the cursor traversal or raw-cursor runner oracle, and carries the
allocation diagnostics into `summary.md`. It deliberately applies no
performance threshold.

The lower-level `run-neighborhood-authoring-spike.sh` and
`run-neighborhood-runner-spike.sh` scripts still emit raw benchmark CSV on
stdout and all configure/build/check diagnostics on stderr, so they remain
usable independently without contaminating CSV captures.

## What to inspect after the run

The decision should not be based on speed alone. Review together:

1. problem-author code size and readability;
2. conceptual distance from EL3 `FirstMove`/`NextMove`;
3. diagnostics when author code is wrong;
4. per-move overhead on tiny through large neighborhoods;
5. fixed per-traversal cost, especially coroutine-frame creation;
6. sensitivity to compiler and standard-library implementation across the
   active GCC 15/16 and Clang 22 + libstdc++/libc++ matrix;
7. whether relative overhead remains visible with the light consumer;
8. allocation diagnostics for coroutine-frame setup and, if measurements
   remain surprising, generated assembly for representative Release builds;
9. how naturally each style can later accommodate debug invalidation checks.

The consumer-side contract remains the same in all variants: when present,
deterministic traversal is a lazy input range of moves. The spike does not imply
that every explorer must provide that capability.

## Runner-level comparison (S15f, promoted in S16c)

The traversal microbenchmark is not sufficient to choose the final authoring
contract by itself. The runner benchmark therefore measures the relevant paths
inside the public `Runner`, using the real evaluation facility, recipe-local
delta dispatch, and the real `FirstImprovement` / `BestImprovement` algorithms:

```text
raw-cursor       small benchmark-only direct FirstMove/NextMove oracle
cursor-range     public easylocal::cursor_moves adapter + real algorithm
coroutine-range  custom coroutine range + the same real algorithm
```

`raw-cursor` deliberately duplicates only the two small consumer loops needed to
provide a semantic/performance oracle. It is not a framework API or a fast path.
The range variants use the production search algorithms unchanged.

Before timing a case, the benchmark requires all variants to produce the same
final solution, cost, evaluation count and termination reason. A mismatch aborts
the benchmark. Prototype-only counters such as traversal count, full scans and
accepted moves were removed when the benchmark switched to the real result
types.

The runner benchmark also counts allocations observed during one complete
`BoundRunner::run()` call. Those counts intentionally include common Runner
costs such as copying the input solution and any allocation performed by the
initial component evaluation; differences between variants therefore expose
additional traversal-specific allocation in the final execution setting.

CSV rows have the form:

```text
domain,algorithm,variant,trial,repetitions,evaluations_per_run,measured_evaluations,allocations_per_run,allocated_bytes_per_run,ns_per_evaluation,ns_per_run,termination,checksum
```

Run it locally with:

```bash
./scripts/run-neighborhood-runner-spike.sh 5000000 5 123456789
```

The GitHub Actions spike workflow runs the complete S16d suite and uploads the
raw CSV files, correctness logs, structured metadata, and generated
`summary.md`. `act` remains only a functional/provisioning check; authoritative
Linux timing comes from the native GitHub-hosted runner.

The S15f decision question was deliberately narrow: whether `cursor-range` was
consistently indistinguishable or epsilon-close to `raw-cursor`. The S15g
measurements supported promoting the cursor adapter without adding a
framework-level raw-cursor fast path. S16c keeps the raw path only as a
benchmark oracle for that conclusion.


## Assignment First Improvement codegen diagnostic (S16e)

One S16d AppleClang ARM64 run showed a narrow anomaly: Assignment
First Improvement through `cursor-range` took roughly twice the time per
evaluation of the benchmark-only raw cursor, while Assignment Best Improvement
and both TSP algorithms remained epsilon-close. S16e does not change the public
API or production algorithm. It adds a controlled diagnostic around exactly that
case.

The diagnostic compares six semantically equivalent implementations:

```text
raw-cursor                direct FirstMove/NextMove oracle
range-current             production FirstImprovement unchanged
range-reference           bind the yielded Move by const reference
explicit-iterator         spell out begin/end/increment instead of range-for
range-deferred-accept     destroy the range before mutating Solution
range-reference-deferred  defer accept and bind Move by const reference
```

The last two variants test a specific lifetime/aliasing hypothesis. Best
Improvement naturally performs `accept()` only after the neighborhood range has
finished, whereas First Improvement accepts the improving candidate while the
range iterator/view is still alive and then breaks. With the cursor adapter the
iterator retains a pointer to the traversed `Solution`; a compiler may therefore
produce more conservative code around that mutation even though the iterator is
not used after the break. The deferred variants preserve First Improvement
semantics but move `accept()` after the range scope ends.

`range-reference` separately tests whether copying the small `Move` value from
the input iterator matters. `explicit-iterator` checks that the range-for
lowering itself is not responsible for the effect. All variants must match the
raw cursor on final solution, cost, evaluation count and termination before any
timing rows are emitted.

The complete S16d/S16e suite writes the raw diagnostic rows to
`first-improvement-diagnostic.csv` and includes their median/min/max ratios in
`summary.md`. As with the other spike measurements, the diagnostic has no
performance pass/fail threshold. The goal is to identify which source-level
property changes the generated code before considering any production change.

## Future public benchmark report

S16d establishes the machine-readable raw bundle and the per-toolchain Markdown
summary used as the input to that future report. A selected and reviewed subset
of measurements can later become public documentation without changing the raw
benchmark machinery. Compiler/library versions, hardware, workload, trial
methodology, and relative ratios should remain explicit; CI timing is not a
portable performance guarantee.

## Targeted cursor-view optimization (S15g)

S15f showed that Clang generally optimizes the FirstMove/NextMove range adapter
away, while GCC can retain a measurable runner-level penalty on some Assignment
cases. S15g therefore changes only `cursor_view` and keeps the Runner benchmark,
search logic, evaluation path and workloads unchanged.

For the S15g same-run A/B measurement, the runner benchmark temporarily exposed
four variants:

```text
raw-cursor          direct FirstMove/NextMove baseline
cursor-range-s15f   the original S15f cursor_view
cursor-range-opt    the optimized cursor_view
coroutine-range     the custom coroutine range
```

That historical A/B path was removed in S16c after the optimized adapter was
promoted. The current runner benchmark compares only `raw-cursor`,
`cursor-range`, and `coroutine-range`, with `cursor-range` using the public
adapter.

The optimized cursor view keeps the same input-range semantics but:

- encodes end-of-range as a null explorer pointer instead of a separate boolean;
- removes the redundant end-state branch from `operator++`;
- treats successful `next_move` as the overwhelmingly common branch;
- keeps invalid-end increment checks in debug builds only;
- force-inlines the tiny adapter operations on GCC/Clang Release builds.

The Release benchmark still uses the normal CMake `Release` optimization level.
It deliberately does not add `-march=native`: hardware-specific tuning would make
CI comparisons less portable. The aggressive optimization is local to the hot
adapter operations and is intended to be representative of a header-only kernel
that expects compiler-visible, fully inlineable code.

The S15g decision metric was the same runner-level comparison as S15f. For the
historical S15g measurements, `cursor-range-opt` was compared to both
`raw-cursor` and `cursor-range-s15f` in the same job rather than by comparing
absolute timings from different GitHub-hosted runners. Current measurements use
the promoted public adapter as `cursor-range`.
