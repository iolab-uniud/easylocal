# 8. Solvers

A runner starts from a solution you give it. A **solver** goes from an Input to
a final solution: it builds the initial solution, owns the RNG, so a seed
reproduces the run, and orchestrates one or more runners.

<!-- snippet: tutorial/main.cpp:solvers -->
```cpp
el::solvers::MultiStart solver{fi, {.starts = 5}};
solver.initialization(el::initialization::random).seed(1);
const auto best = solver.solve(tsp);
```

A solver is built from its runner (MultiStart also from its parameters, here
five starts), then `.initialization(...)` and `.seed(...)` configure it.
`initialization::random` starts every run from a random tour, so
`TourManager` needs a member it did not have yet, `random_solution`:

<!-- snippet: tutorial/tsp.hpp:random-solution -->
```cpp
// The same cities in a random order.
template<std::uniform_random_bit_generator RNG>
Tour random_solution(RNG& rng) const
{
    auto tour = initial_solution();
    std::shuffle(tour.order.begin(), tour.order.end(), rng);
    return tour;
}
```

The RNG is passed in, never created inside: the solver owns it, so its `seed`
reproduces every start. It is seeded once: a second `solve()` continues the
stream and finds other starts.

The built-in solvers live in `easylocal::solvers`:

| Solver | Does | Needs |
| --- | --- | --- |
| `LocalSearch` | builds an initial solution, runs once | `initial_solution` or `random_solution` |
| `MultiStart` | `starts` independent runs, keeps the best | `initial_solution` or `random_solution` |
| `Pipeline` | runners in sequence, each stage from the solution of the previous one | stages with the same Input and Solution |

- The initialization is a tag, `initialization::initial` or
  `initialization::random`, checked at compile time against the
  SolutionManager. Without one, a solver starts from a random solution when
  the SolutionManager builds one, else from its initial solution
  (`initialization::automatic`), with seed 0.
- `make_solver<Solver>(runner[, parameters])` deduces the solver type from the
  runner, as `solvers::MultiStart{runner, {...}}` does.
- `solver.solve(input, el::with(control, tracer))` cancels and traces a solve
  like a run ([chapter 15](15-observing-and-controlling.md)); the result counts
  the evaluations and iterations of all the runs.

## Hard and soft costs

`solvers::two_stage()` is for problems whose cost is written with
`cost::hard_soft` ([chapter 2](02-cost.md#cost-expressions)): it first removes
the violations, then optimizes the full cost from the feasible solution it
found.

```cpp
auto sm = el::solution_manager<TimetableManager>()
        | el::cost::hard_soft(
              el::cost::sum(el::component<Conflicts>(),
                            el::component<Unavailability>()),
              el::component<Compactness>() * 2);

auto solver = el::solvers::two_stage(
                  el::make_runner<runners::FirstImprovement>(
                      runners::FirstImprovementParameters{})
                  | sm | nhe)
                  .initialization(el::initialization::initial);
```

- The first stage evaluates only the components of the `hard` branch, read
  from the expression, and stops as soon as the hard cost is zero.
- The second stage starts from that solution with the full hierarchical cost,
  where a hard degradation is never accepted.
- The tutorial's `examples/tutorial/staged_main.cpp` is a complete program
  with `two_stage`.

## Several stages

`two_stage()` is a pipeline of two stages. A pipeline chains any number of
runners, each with its own name, cost and neighborhood, over the same
solution:

<!-- snippet: tutorial/pipeline_main.cpp:pipeline -->
```cpp
// | chains the stages, & gives a stage its options, in parentheses: the
// first one works on the hard cost until it is zero, in up to five
// descents from new random tours; the others continue from its tour on the
// whole cost.
using namespace el::solvers;
auto solver = (stage("feasible", descent) & until_feasible() & attempts(5))
    | stage("descent", descent) | stage("climb", climbing);
const auto result = solver.seed(7).solve(tsp);
```

- `|` chains the stages, `&` gives a stage its options, in parentheses (GCC
  warns about `&` and `|` mixed without them); the methods
  `.until_feasible()`, `.with_attempts(10)`, `.with_target(0)` and the
  pipeline's `.then(stage)` spell the same pipeline out.
- `until_feasible()` runs the stage on the hard cost until it is zero;
  `target(cost)` stops a stage at another target, in its own cost.
- `attempts(5)` repeats the stage, here from a new random tour, while it has
  not reached its target, and keeps the best run. The attempts of a later
  stage start from the solution it received, unless
  `restart(initialization::random)` starts them from new random tours.
- `result.stages` reports each stage (attempts, effort, termination, cost),
  and the parameters of a stage are under its name (`climb.search.*`,
  `feasible.attempts`). `examples/tutorial/pipeline_main.cpp` is the complete
  program.

An app registers a pipeline beside its runners, under a name of the same list:

```cpp
auto application = el::app("tsp") | sm | two_opt
    | el::runner<runners::FirstImprovement>("fi")
    | el::pipeline("cascade",
          stage("feasible", descent) & until_feasible() & attempts(5),
          stage("climb", climbing));
```

The command line (`--runner cascade`), the TextUI and the REST service then run
it by name from the current solution, as they run a runner, and configure its
stages under `runners.cascade.*` ([Apps and tools](../reference/app-and-tools.md#pipelines)).

## See also

- [Solvers](../reference/solvers.md).

## Next steps

[Chapter 9](09-configuration.md) exposes the parameters to the command line.
