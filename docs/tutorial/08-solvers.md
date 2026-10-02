# 8. Solvers

A runner starts from a solution you give it. A **solver** goes from an Input to
a final solution: it builds the initial solution, owns the RNG, so a seed
reproduces the run, and orchestrates one or more runners.

<!-- snippet: tutorial/main.cpp:solvers -->
```cpp
auto solver = el::make_solver<el::solvers::MultiStart>(
    fi,
    el::solvers::MultiStartConfig<el::initialization::Random>{
        .parameters = {.starts = 5},
        .initialization = el::initialization::random,
        .seed = 1,
    });
const auto best = solver.solve(tsp);
```

The built-in solvers live in `easylocal::solvers`:

| Solver | Does | Needs |
| --- | --- | --- |
| `LocalSearch` | builds an initial solution, runs once | the chosen initialization |
| `MultiStart` | `starts` independent runs, keeps the best | the chosen initialization |
| `TwoStage` | first stage on the hard cost until it is zero, second on the full cost | a `cost::hierarchical` cost, best from a `cost::hard_soft` expression |

- The initialization is chosen statically, with `initialization::initial` or
  `initialization::random` checked at compile time against the
  SolutionManager, or at runtime with `initialization::Mode`.
- `make_solver<Solver>(runner, config)` deduces the solver type from the runner.
- `solver.solve(input, el::with(control, tracer))` cancels and traces a solve
  like a run ([chapter 15](15-observing-and-controlling.md)); the result counts
  the evaluations and iterations of all the runs.

## Hard and soft costs

`TwoStage` is for problems whose cost is written with `cost::hard_soft`
([chapter 2](02-cost.md#cost-expressions)): it first removes the violations,
then optimizes the full cost from the feasible solution it found.

```cpp
auto sm = el::solution_manager<TimetableManager>()
        | el::cost::hard_soft(
              el::cost::sum(el::component<Conflicts>(),
                            el::component<Unavailability>()),
              el::component<Compactness>() * 2);

auto solver = el::make_solver<el::solvers::TwoStage>(
    el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
        | sm | nhe,
    el::solvers::TwoStageConfig<el::initialization::Initial>{
        .initialization = el::initialization::initial,
    });
```

- The first stage evaluates only the components of the `hard` branch, read
  from the expression, and stops as soon as the hard cost is zero.
- The second stage starts from that solution with the full hierarchical cost,
  where a hard degradation is never accepted.
- The Assignment example (`examples/assignment/main.cpp`) is a complete
  program.

## See also

- [Solvers](../reference/solvers.md).

## Next steps

[Chapter 9](09-configuration.md) exposes the parameters to the command line.
