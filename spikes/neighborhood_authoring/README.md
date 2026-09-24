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

`cursor_view.hpp` adapts the familiar stateful protocol

```cpp
bool first_move(const Solution&, Move&) const;
bool next_move(const Solution&, Move&) const;
```

into the same lazy input-range consumed by algorithms:

```cpp
auto moves(const Solution& solution) const
{
    return cursor_moves(*this, solution);
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

The spike-local cursor adapter currently assumes a default-constructible `Move`.
That is a prototype simplification, not a proposed public requirement.

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

## Runner-level comparison (S15f)

The traversal microbenchmark is not sufficient to choose the final authoring
contract by itself. S15f therefore adds a second benchmark that runs the three
relevant traversal paths inside the public `Runner`, using the real evaluation
facility and recipe-local delta dispatch:

```text
raw-cursor       FirstMove/NextMove consumed directly by the algorithm
cursor-range     the same FirstMove/NextMove adapted through cursor_view
coroutine-range  the custom coroutine range consumed by the same algorithm
```

The prototype algorithms are spike-local versions of First Improvement and Best
Improvement. They share the same search logic, evaluation budget semantics,
SolutionManager recipe, delta evaluators and move application. Only the
traversal mechanism differs.

Before timing a case, the benchmark requires all three variants to produce the
same final solution, cost, evaluation count, traversal count, full-scan count,
accepted-move count and termination reason. A mismatch aborts the benchmark.
This makes the raw cursor a semantic and performance baseline rather than a
separate algorithm.

The runner benchmark also counts allocations observed during one complete
`BoundRunner::run()` call. Those counts intentionally include common Runner
costs such as copying the input solution and any allocation performed by the
initial component evaluation; differences between variants therefore expose
additional traversal-specific allocation in the final execution setting.

CSV rows have the form:

```text
domain,algorithm,variant,trial,repetitions,evaluations_per_run,measured_evaluations,measured_traversals,measured_full_scans,measured_accepted_moves,allocations_per_run,allocated_bytes_per_run,ns_per_evaluation,ns_per_run,termination,checksum
```

Run it locally with:

```bash
./scripts/run-neighborhood-runner-spike.sh 5000000 5 123456789
```

The GitHub Actions spike workflow runs both the original traversal benchmark and
this runner-level benchmark and uploads `runner-check.log` and
`runner-benchmark.csv` beside the existing artifacts. `act` remains only a
functional/provisioning check; authoritative Linux timing comes from the native
GitHub-hosted runner.

The decision question for S15f is deliberately narrow: if `cursor-range` is
consistently indistinguishable or epsilon-close to `raw-cursor`, then the
framework can preserve the familiar EL3 FirstMove/NextMove authoring model while
presenting a modern lazy range to algorithms. If the adapter has material hot
loop overhead, the framework must not assume that it optimizes away and may
need a statically recognized direct-cursor fast path.

## Future public benchmark report

Once the neighborhood authoring API and benchmark methodology are stable, a
selected and reviewed subset of these measurements should become a reproducible
GitHub documentation report. Keep raw benchmark machinery separate from the
public report; report compiler/library versions, hardware, workload, trial
methodology, and relative ratios rather than presenting raw CI timing as a
portable performance guarantee.
