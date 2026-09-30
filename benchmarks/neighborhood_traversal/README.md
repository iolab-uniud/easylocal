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
  `easylocal::moves` adapter for deterministic neighborhood authoring;
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

The opt-in build provides two tracing probes:

- `easylocal_trace_benchmark`, an end-to-end First Improvement benchmark that
  compares the uninstrumented baseline, explicit `null_tracer`, counter-only
  tracing, in-memory tracing, block-buffered ELTR recording, asynchronous ELTR
  recording, and buffered/asynchronous temporary-file output;
- `easylocal_trace_cost_encoding_benchmark`, a focused ELTR encoding benchmark
  for scalar, lexicographic, and hierarchical cost payloads.

JSONL is deliberately excluded from the performance comparison: text formatting
mostly measures serialization policy rather than the tracing boundary itself.
JSONL remains a supported trace format and is covered by correctness tests.

The discard sinks remove filesystem variability while retaining binary encoding
and buffering work. File modes intentionally do not request `fsync`, so they
measure application-visible persistence overhead rather than physical-media
durability. All end-to-end modes execute the same search workload and must
produce the same checksum. That checksum consumes the final cost, termination,
evaluation count, and solution contents so the optimizer cannot discard
semantically relevant search work differently for different tracer types.

Run both probes with a Release build:

```bash
cmake -S . -B build/trace-benchmark -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DEASYLOCAL_BUILD_BENCHMARKS=ON
cmake --build build/trace-benchmark --target \
  easylocal_trace_benchmark \
  easylocal_trace_cost_encoding_benchmark
./build/trace-benchmark/benchmarks/neighborhood_traversal/easylocal_trace_benchmark
./build/trace-benchmark/benchmarks/neighborhood_traversal/easylocal_trace_cost_encoding_benchmark
```

The **Trace Microbenchmarks** GitHub Actions workflow runs ten process-level
end-to-end trials on Linux/GCC and macOS/AppleClang, runs the cost-encoding probe,
adds both CSV outputs to the job summary, and uploads the raw measurements as CI
artifacts. Performance values remain diagnostic: compare distributions and ratios
within one machine/toolchain rather than absolute timings across heterogeneous
runners.
