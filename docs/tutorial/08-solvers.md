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
| `TwoStage` | first stage on the hard cost, second on the full cost | a `cost::hierarchical` cost, an aggregator modelling `cost::hard_projection` |

- The initialization is chosen statically, with `initialization::initial` or
  `initialization::random` checked at compile time against the
  SolutionManager, or at runtime with `initialization::Mode`.
- `make_solver<Solver>(runner, config)` deduces the solver type from the runner.

## See also

- [Solvers](../reference/solvers.md).

## Next steps

[Chapter 9](09-configuration.md) exposes the parameters to the command line.
