# 15. Observing and controlling a run

A run accepts a control and a tracer as its trailing argument:

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

- `stop.request_stop()`, from any thread, ends the run cooperatively; the result
  reports `termination_reason::cancelled`. Every runner honours it through
  `search_run::should_stop()`.
- The observer receives `run_progress` (evaluations, iterations and, when a
  budget is set, the evaluation limit).
- The tracer receives typed search events: `run_started`, `move_evaluated`,
  `move_accepted`, `incumbent_updated`, `local_optimum`,
  `neighborhood_selection`, `run_finished`. Without a tracer, their
  construction is removed at compile time.
- `easylocal::with(control)`, `easylocal::with(tracer)` and
  `easylocal::with(control, tracer)` are all accepted.
- A target cost ends the run as soon as the best cost is at least as good:
  `el::stop_at(cost)` alone, or `el::with(control, tracer).stop_at(cost)`. The
  result reports `termination_reason::target_reached`.
- A time limit ends the run once it has passed: `el::timeout(5s)` (any
  `std::chrono` duration) or `el::timeout(2.5)` (seconds), alone or as
  `el::with(control).timeout(5s).stop_at(cost)`. The result reports
  `termination_reason::time_limit_reached`; no thread is started, the run
  reads the clock among its other checks.
- Solvers take the same options, `solver.solve(input, el::with(control))`, and
  pass them to every run they make; a solve's time limit bounds all its runs
  together, and a pipeline stage may have its own (`& el::timeout(10s)`).

Recorders include `trace::memory_recorder`, `trace::jsonl_recorder` and the
binary `buffered_binary_recorder` and `async_binary_recorder`, whose ELTR traces
describe themselves and are decoded by `scripts/eltr.py` (see
[Tracing](../tracing.md)).

Diagnostic logging (`<easylocal/utils/logging.hpp>`) is separate from tracing.

## See also

- [Tracing](../tracing.md) and [Logging](../logging.md).
- [Runners](../reference/runners.md#run-options).

## Next steps

[Chapter 16](16-comparison-with-easylocal-3.md) compares this framework with
EasyLocal 3, or go to the [reference](../reference/README.md).
