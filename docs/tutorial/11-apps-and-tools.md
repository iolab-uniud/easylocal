# 11. Applications

An **app** names a problem and the runners available on it. The tools of the
next chapters (checking, the interactive tester, the REST service) all work on
apps.

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

## The Tester

The `Tester` drives an app without a user interface: it holds an Input and a
current Solution, inspects moves and runs the registered runners on the
current solution.

<!-- snippet: tutorial/main.cpp:tester -->
```cpp
el::Tester tester{application, /* seed */ 2026};
tester.set_input(tsp);
tester.use_initial_solution();
(void)tester.use_first_improving_move();
(void)tester.run_runner("sa"); // receives the Tester's RNG
(void)tester.run_runner("fi");
```

- Input and Solution come from `set_input` / `set_solution`, from
  `use_initial_solution()` / `use_random_solution(rng)`, or from files with
  `load_input` / `load_solution` when the problem provides the I/O hooks
  (chapter 13); `save_solution` writes the current one.
- Moves: `use_first_move`, `use_next_move`, `use_first_improving_move`,
  `use_best_move`, `use_random_move`; `evaluate()` gives the current cost.
- `run_runner(name)` replaces the current solution with the runner's result.

## Randomness in tools

Tools own an RNG and give it to stochastic runners; deterministic runners
ignore it. The seed is configurable everywhere:

| Tool | Seed |
| --- | --- |
| `Tester` | `Tester{app, seed}`, `set_seed(seed)`, `rng()` |
| TextUI | the `seed` option, changeable on the Run page |
| REST | the `seed` of a run request, or `blueprint_options::seed + run id` |

Your own code does the same with
`app.run_at_with_rng<Index>(input, solution, rng)`.

## See also

- [Apps and tools](../reference/app-and-tools.md).

## Next steps

[Chapter 12](12-checking.md) checks the composed problem.
