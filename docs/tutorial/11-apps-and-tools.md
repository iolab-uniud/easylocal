# 11. Applications and tools

An **app** names a problem and the runners available on it. Tools work on apps.

<!-- snippet: tutorial/main.cpp:app -->
```cpp
auto application = el::app("tsp")
    .with_solution_manager(sm)
    .with_neighborhood(nhe)
    .with_runner<runners::FirstImprovement>("fi")
    .with_runner<runners::SimulatedAnnealing<Classic>>(
        "sa", {.samples_per_temperature = 50});

auto piped_application = el::app("tsp") | sm | nhe
    | el::runner<runners::FirstImprovement>("fi")
    | el::runner<runners::SimulatedAnnealing<Classic>>(
          "sa", {.samples_per_temperature = 50});

const auto app_result =
    application.run<runners::FirstImprovement>(tsp, Tour{{0, 1, 2, 3, 4}});
```

- A runner is registered by its algorithm class and a name, optionally with its
  parameters (`parameters_type`). They are stored in the app and can be changed
  later with `runner_config<Algorithm>()` or `runner_config<Algorithm>("name")`.
- Every `app.run(...)` builds fresh services for that run, so concurrent runs
  only share the immutable Input; `app.for_input(input)` returns a reusable
  *runtime* when you want to keep them.
- `app.make_runner<Algorithm>()` and `app.make_solver<Solver, Algorithm>(config)`
  build standalone runners and solvers from a registration.

- Tools give stochastic runners an RNG they own, seeded from a configurable
  seed: the `Tester` takes it as a constructor argument (`Tester{app, seed}`,
  `set_seed`), the TextUI from its options, REST from the run request or the
  blueprint options. Deterministic runners ignore it. Your own code does the
  same with `app.run_at_with_rng<Index>(input, solution, rng)`.
- Simulated Annealing is registered with its temperature policy's parameters
  (`runners::SimulatedAnnealing<Classic>` takes `ClassicParameters`).

## Tools

| Tool | Header | Purpose |
| --- | --- | --- |
| `easylocal::check(app, input)` | `app/check.hpp` | contract checks of the composed problem |
| `easylocal::Tester` | `app/tester.hpp` | headless driver: load Input and Solution, inspect moves, run runners |
| `easylocal::tui::run(tester, options)` | `adapters/tui.hpp` | interactive terminal tester (FTXUI) |
| `easylocal::rest::blueprint(prefix, app, codec, options)` | `adapters/rest.hpp` | HTTP API with asynchronous, cancellable runs (Crow) |

<!-- snippet: tutorial/main.cpp:tester -->
```cpp
el::Tester tester{application, /* seed */ 2026};
tester.set_input(tsp);
tester.use_initial_solution();
(void)tester.use_first_improving_move();
(void)tester.run_runner("sa"); // receives the Tester's RNG
(void)tester.run_runner("fi");
```

The TextUI and REST adapters are optional CMake components (`TUI`, `REST`), kept
outside Core. The Assignment example (`examples/assignment/`) uses both.

## See also

- [Apps and tools](../reference/app-and-tools.md).
- [REST adapter](../rest.md).

## Next steps

[Chapter 12](12-observing-and-controlling.md) watches a run while it happens.
