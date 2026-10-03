# 11. Applications

An **app** packages a problem: its SolutionManager, its neighborhood and the
runners available on it, each under a name. You can use it directly in your
program, and the tools of the next chapters (the Tester, the checks, the REST
service) all work on apps.

<!-- snippet: tutorial/main.cpp:app -->
```cpp
auto application =
    el::app("tsp")
        .with_solution_manager(sm)
        .with_neighborhood(nhe)
        .with_runner<runners::FirstImprovement>("fi")
        .with_runner<runners::SimulatedAnnealing<Classic>>(
            "sa",
            {.samples_per_temperature = 50});

auto piped_application = el::app("tsp") | sm | nhe
    | el::runner<runners::FirstImprovement>("fi")
    | el::runner<runners::SimulatedAnnealing<Classic>>(
        "sa",
        {.samples_per_temperature = 50});

const auto app_result =
    application.run<runners::FirstImprovement>(tsp, Tour{{0, 1, 2, 3, 4}});
```

- A runner is registered by its algorithm class and a name, optionally with its
  parameters (`parameters_type`). They are stored in the app and can be changed
  later with `runner_config<Algorithm>()` or `runner_config<Algorithm>("name")`.
- Simulated Annealing is registered with its temperature policy's parameters:
  `runners::SimulatedAnnealing<Classic>` takes `ClassicParameters`.
- Every `app.run(...)` builds fresh services for that run, so concurrent runs
  only share the immutable Input; `app.for_input(input)` returns a reusable
  *runtime* when you want to keep them.
- `app.make_runner<Algorithm>()` and `app.make_solver<Solver, Algorithm>(config)`
  build standalone runners and solvers from a registration.

## Using the app in your program

An app is not only for tools: your own `main` can use it to run the registered
runners. `for_input` builds the services once for an Input and returns a
*runtime*:

<!-- snippet: tutorial/main.cpp:app-in-main -->
```cpp
// A runtime holds the services of the app for one Input.
auto runtime = application.for_input(tsp);
const auto initial = runtime.solution_manager().initial_solution();

// Run the registered runners by algorithm, each from the same tour.
const auto by_descent = runtime.run<runners::FirstImprovement>(initial);
std::mt19937_64 annealing_rng{2026};
const auto by_annealing =
    runtime.run<runners::SimulatedAnnealing<Classic>>(initial, annealing_rng);
```

- `runtime.solution_manager()` is the SolutionManager bound to `tsp`; here it
  builds the initial tour.
- `runtime.run<Algorithm>(solution, ...)` runs the runner registered for that
  algorithm, with its stored parameters. Stochastic runners take the RNG as an
  argument, as in chapter 5: the program owns it, and its seed reproduces the
  run.
- `application.run<Algorithm>(input, solution)`, in the first snippet, is the
  one-shot form: it builds a runtime, runs, and throws the runtime away.
- When several runners of the same algorithm are registered, `run_at<Index>`
  chooses by position, in registration order.

The same app is what the tools of the next chapters take: the Tester and its
interactive interface (chapter 12), the checks (chapter 13) and the REST
service (chapter 14). They add no requirement to the app itself.

## See also

- [Apps and tools](../reference/app-and-tools.md).

## Next steps

[Chapter 12](12-tester.md) drives the app with the Tester.
