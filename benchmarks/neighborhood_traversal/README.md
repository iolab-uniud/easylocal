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
