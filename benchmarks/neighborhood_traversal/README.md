# Neighborhood traversal benchmarks

This directory contains the small permanent performance regression benchmark for
EasyLocal neighborhood traversal.

It is intentionally not a correctness test suite and does not impose automatic
performance thresholds. Each executable checks semantic equivalence before
reporting timings; the normal test suite remains authoritative for correctness.

The benchmark keeps the implementations that are architecturally relevant:

- `raw-cursor`: direct `first_move` / `next_move`, used only as a performance
  oracle in the search benchmark;
- `cursor`: the real Assignment/TSP production explorers, using the public
  `easylocal::cursor_moves` adapter for deterministic neighborhood authoring;
- `coroutine-custom`: a coroutine-backed input range, representing coroutine
  neighborhood authoring as an allowed alternative;
- `coroutine-std`: `std::generator`, when the active standard library provides
  it.

The benchmark deliberately does not duplicate production cursor explorers: raw-cursor
and cursor-range measurements instantiate the real Assignment/TSP explorers directly.
Only coroutine alternatives remain benchmark-local.

Historical S15/S16 source-shape diagnostics and experimental workaround
variants are deliberately not retained here. Their conclusions are recorded in
`.local/context.md`.

Run the complete benchmark locally with:

```bash
./scripts/run-neighborhood-benchmarks.sh \
    build/neighborhood-benchmark-results \
    5000000 5 123456789
```

Performance values are diagnostic. Compare ratios within the same machine and
toolchain; do not compare absolute timings across heterogeneous CI runners.

## Search tracing overhead

The opt-in build also provides `easylocal_trace_benchmark`, a focused regression
probe for semantic tracing. It reports nanoseconds per evaluation for:

- the ordinary baseline;
- an explicit `null_tracer`;
- a counter-only tracer;
- the full in-memory recorder;
- incremental EasyLocal JSONL serialization into a discard stream;
- block-buffered ELTR binary recording;
- asynchronous ELTR recording, with producer and total drain timings separated;
- synchronous and asynchronous ELTR recording to temporary files;
- when available, synchronous spdlog JSONL formatting into a formatting discard
  sink.

The discard sinks remove filesystem variability while retaining serialization
and formatting work. The file modes use temporary binary files and report
producer and total drain time for the asynchronous path; they intentionally do
not request `fsync`, so they measure application-visible persistence overhead
rather than physical-media durability. All modes execute the same search workload
and must report the same evaluation checksum. A small warm-up precedes each timed
mode.

spdlog is **benchmark-only**. EasyLocal never fetches it. A local build simply
omits the comparison if `find_package(spdlog CONFIG)` cannot find a system
installation; configure with `-DEASYLOCAL_BENCHMARK_REQUIRE_SPDLOG=ON` when the
comparison is mandatory. For example:

```bash
cmake -S . -B build/trace-benchmark -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DEASYLOCAL_BUILD_BENCHMARKS=ON \
  -DEASYLOCAL_BENCHMARK_REQUIRE_SPDLOG=ON
cmake --build build/trace-benchmark --target easylocal_trace_benchmark
./build/trace-benchmark/benchmarks/neighborhood_traversal/easylocal_trace_benchmark
```

The **Trace Microbenchmarks** GitHub Actions workflow installs spdlog explicitly
on Linux and macOS, runs ten process-level trials for pull requests and release
tags, writes the raw CSV into the job summary, and uploads the measurements as a
release/CI artifact. The benchmark remains dependency-free when that comparison
is not requested.

The trace microbenchmark additionally compares constant-memory JSONL, block-buffered
ELTR, and asynchronous ELTR recording against in-memory tracing and, when installed,
spdlog JSONL formatting.  Async output has separate producer and total timings so a
background writer cannot hide drain cost. The benchmark reports serialized
bytes/event and also exercises temporary-file output, while the discard modes
remain the serialization-only comparison.
