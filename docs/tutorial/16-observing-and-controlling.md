# 16. Observing and controlling a run

You can monitor a search, stop it from another thread or record its decisions
without changing the algorithm. Pass a control and a tracer as the final
argument to the run:

<!-- snippet: tutorial/main.cpp:control -->
```cpp
std::stop_source stop;
auto observer = [](const el::run_progress& progress) {
    (void)progress.evaluations; // also: iterations, evaluation_limit
};
el::run_control control{stop.get_token(), observer};
el::trace::memory_recorder<double> trace;

auto descent_search = descent.bind(tsp);
const auto observed = descent_search.run(
    descent_search.initial_solution(),
    rng,
    el::with(control, trace));
```

**Control and progress.** Call `stop.request_stop()` from any thread to request
cancellation. Each runner checks the request through `search_run::should_stop()`
and reports `termination_reason::cancelled`. The observer receives
`run_progress`, with evaluations, iterations and any evaluation budget.

**Tracing.** A tracer receives typed events such as `run_started`,
`move_evaluated`, `move_accepted`, `incumbent_updated` and `run_finished`.
Other events describe local optima, neighborhood selection, visited solution
hashes, and Tabu Search's aspiration, escapes and tenure changes. Solvers add
`run_context` before each run. Without a tracer, event construction is removed
at compile time.

Match the recorder to the cost type: `memory_recorder<double>` records this
TSP's events, including events with no cost value. Pass a control, a tracer
or both with `el::with(control)`, `el::with(tracer)` or
`el::with(control, tracer)`.

**Run limits.** The same options work across algorithms:

| Option | Stops when | Termination reason |
| --- | --- | --- |
| `el::stop_at(cost)` | the best cost reaches the target or improves on it | `target_reached` |
| `el::timeout(5s)` or `el::timeout(2.5)` | the time limit expires; numeric values are seconds | `time_limit_reached` |
| `el::max_evaluations(n)` | the evaluation budget is exhausted | `evaluation_budget_exhausted` |

Timeouts accept any `std::chrono` duration. The run checks the clock alongside
its other stopping conditions; no timer thread is needed. An evaluation limit
can tighten, but cannot raise, the runner's own budget.

Combine options as needed, for example
`el::with(control, tracer).timeout(5s).stop_at(cost)`.
Solvers accept them too: `solver.solve(input, el::with(control))`. A solver's
time and evaluation limits cover all its runs together. Pipeline stages can
also have their own limits, such as `& el::timeout(10s)` or
`& el::max_evaluations(5000)`.

Use `trace::memory_recorder` to inspect events in code, `trace::jsonl_recorder`
for JSON Lines, or `binary_recorder` and `async_binary_recorder` for ELTR
files. `scripts/eltr.py` decodes ELTR traces; see [Tracing](../tracing.md).

Diagnostic logging (`<easylocal/utils/logging.hpp>`) is separate from tracing.

## See also

- [Tracing](../tracing.md) and [Logging](../logging.md).
- [Runners](../reference/runners.md#run-options).

## Next steps

The [reference](../reference/README.md) describes every component in full;
[Coming from EasyLocal 3](../from-easylocal-3.md) maps EasyLocal 3 to this
framework.
