# Assignment MWE

This concrete model is used to discover the EasyLocal++ API.

It is deliberately **outside** `include/easylocal/` and is not part of the
public framework API.

## Problem

Each job has a non-negative demand and is assigned to one machine. Each machine
has a non-negative capacity.

The current cost is total overload:

```text
sum_m max(0, load[m] - capacity[m])
```

Example:

```text
demand   = [4, 3, 2]
capacity = [5, 5]
solution = [0, 0, 1]
```

Loads are `[7, 2]`, so the cost is `2`.

Move `(job=1, destination=1)` produces:

```text
solution = [0, 1, 1]
loads    = [4, 5]
cost     = 0
```

## Current responsibilities

`Instance`
: Plain problem data. The example publishes it as a const object during use.

`Solution`
: Plain value representation. It does not store an `Instance` pointer/reference.

`Move`
: Plain descriptive value. It does not store a `Solution` or `Instance`
  pointer/reference.

`SolutionManager`
: Instance-bound service responsible for structural solution validation and full
  solution evaluation.

`NeighborhoodExplorer`
: Service responsible for move validity and move application. It is bound to a
  `SolutionManager`, which in turn determines the instance.

No generic EasyLocal++ concepts are extracted yet.

## Deferred

The current MWE deliberately does not define:

- generic cost structures;
- delta evaluation;
- integer overflow policy;
- floating-point semantics;
- hard/soft aggregation;
- CLI/configuration;
- search algorithms;
- tracing/logging;
- generic framework concepts.
