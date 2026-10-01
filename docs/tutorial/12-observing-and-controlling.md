# 12. Observing and controlling a run

A run accepts a control and a tracer as its trailing argument:

<!-- snippet: tutorial/main.cpp:control -->
```cpp
std::stop_source stop;
auto observer = [](const el::run_progress& progress) {
    (void)progress.evaluations; // also: iterations, evaluation_limit
};
el::run_control control{stop.get_token(), observer};
el::trace::memory_recorder<double> trace;

auto descent_bound = descent.bind(tsp);
const auto observed = descent_bound.run(
    descent_bound.initial_solution(), rng, el::with(control, trace));
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

Recorders include `trace::memory_recorder`, `trace::jsonl_recorder` and the
binary `buffered_binary_recorder` and `async_binary_recorder`.

Diagnostic logging (`<easylocal/utils/logging.hpp>`) is separate from tracing.

## See also

- [Tracing](../tracing.md) and [Logging](../logging.md).
- [Runners](../reference/runners.md#run-options).

## Next steps

[Chapter 13](13-from-easylocal-3.md) maps EasyLocal 3 concepts to this
framework, or go to the [reference](../reference/README.md).
