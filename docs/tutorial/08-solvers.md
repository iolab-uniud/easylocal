# 8. Solvers

A runner starts from a solution you supply. A **solver** handles the whole
search from an Input: it creates a starting solution, owns the RNG and
coordinates one or more runs. This makes random restarts and staged searches
available without writing the orchestration yourself.

<!-- snippet: tutorial/main.cpp:solvers -->
```cpp
el::solvers::MultiStart solver{fi, {.starts = 5}};
solver.initialization(el::initialization::random).seed(1);
const auto best = solver.solve(tsp);
```

Construct the solver from a runner, then choose its initialization and seed.
MultiStart also takes parameters: here it makes five starts.
`initialization::random` needs a random tour for each start, so add
`random_solution` to `TourManager`:

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

Use the RNG passed by the solver to keep random starts reproducible. The
solver seeds it once; a second `solve()` continues the stream rather than
repeating the first solve's starts.

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
  like a run ([chapter 16](16-observing-and-controlling.md)); the result counts
  the evaluations and iterations of all the runs.

## Hard and soft costs

For a `cost::hard_soft` objective, `solvers::two_stage()` separates the search
for feasibility from optimization of the full cost. It derives the hard-only
stage from the existing cost expression, so you can reuse the same components
([chapter 2](02-cost.md#cost-expressions)):

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

- `|` chains stages and `&` adds stage options. Parenthesize combinations of
  the two to avoid GCC warnings. The equivalent named members are
  `.until_feasible()`, `.with_attempts(10)`, `.with_target(0)` and
  `.then(stage)`.
- `until_feasible()` runs the stage on the hard cost until it is zero;
  `target(cost)` stops a stage at another target, in its own cost.
- `attempts(5)` tries the stage up to five times, stopping at its target and
  keeping the best result. Here each attempt starts from a new random tour.
  Later stages reuse the solution they received unless you add
  `restart(initialization::random)`. First and Best Improvement would repeat
  the same deterministic search from that solution, so repeated attempts
  require random restarts.
- `result.stages` reports each stage (attempts, effort, termination, cost),
  and the parameters of a stage are under its name (`climb.search.*`,
  `feasible.attempts`). `examples/tutorial/pipeline_main.cpp` is the complete
  program.

An app can register a pipeline alongside its runners. Define a stage with
`stage<Algorithm>(name, parameters)` to reuse the app's problem recipes:

<!-- snippet: tutorial/pipeline_main.cpp:app-pipeline -->
```cpp
// The same stages registered in an app, as algorithms on the app's
// recipes: its cost, so its cost.* parameters apply to every stage, and
// its neighborhood, or the stage's own. The app runs the pipeline from the
// current solution; restart() starts the first stage's other attempts from
// new random tours.
auto application = el::app("tsp") | sm | two_opt
    | el::pipeline(
        "cascade",
        stage<runners::FirstImprovement>("feasible") & until_feasible() & attempts(5)
            & restart(el::initialization::random),
        stage<runners::FirstImprovement>("descent"),
        stage<runners::HillClimbing>("climb", {.max_idle_iterations = 200}, both));
const auto initial = application.bind(tsp).solution_manager().initial_solution();
std::mt19937_64 rng{7};
// run() returns no result for a name that is not registered.
if (const auto cascade = application.run("cascade", tsp, initial, rng))
{
    std::cout << "cascade: violations " << cascade->cost.hard() << ", length "
              << cascade->cost.soft() << '\n';
}
```

- An algorithm stage shares the app's SolutionManager, cost and `cost.*`
  parameters. It also uses the app's neighborhood unless you supply one as
  the third argument, such as `both` for the climb.
- `until_feasible()` derives the hard-cost stage from the app's cost, and the
  stage options are those of a stage of a runner.
- In an app, the pipeline starts from the current solution.
  `restart(el::initialization::random)` gives later attempts of the first
  stage new random tours.
- `stage("climb", climbing)`, a stage of a runner, may still be registered: it
  keeps its own recipes and its own `cost.*` parameters.

The command line (`--runner cascade`), the TextUI and the REST service then run
it by name from the current solution, as they run a runner, and configure its
stages under `runners.cascade.*` ([Apps and tools](../reference/app-and-tools.md#pipelines)).

## See also

- [Solvers](../reference/solvers.md).

## Next steps

[Chapter 9](09-configuration.md) exposes the parameters to the command line.
