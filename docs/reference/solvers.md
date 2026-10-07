# Solvers

`<easylocal/solvers.hpp>`; `solvers/local_search.hpp`, `solvers/multi_start.hpp`,
`solvers/pipeline.hpp`, `solvers/two_stage.hpp`, `solvers/initialization.hpp`

A solver goes from an Input to a final solution. It owns the RNG, builds the
initial solutions and orchestrates its runners.

## Construction

```cpp
auto solver = make_solver<solvers::MultiStart>(runner, solvers::MultiStartParameters{.starts = 5})
                  .initialization(initialization::random)
                  .seed(7);
auto result = solver.solve(input);
auto controlled = solver.solve(input, easylocal::with(control, tracer).stop_at(target));
```

Every solver is built from its runner (and, for MultiStart, its parameters)
and configured with the same two builders, which return the solver: itself on
an lvalue, by value on a temporary.

| Builder | Default | Effect |
| --- | --- | --- |
| `.seed(n)` | 0 | seeds the solver's RNG |
| `.initialization(tag)` | `initialization::automatic` | how the initial solutions are built ([Initialization](#initialization)) |

The optional trailing [run options](runners.md#run-options) — cancellation,
tracer, target cost — reach every run of the solver. Before each run, the
solver sends the tracer a `run_context` event with the pipeline stage and the
attempt (or the start), which tells the runs of a solve apart in a trace
([Tracing](../tracing.md)).

`make_solver` takes the solver class template as its key and deduces the
runner type. A custom RNG type is the last constructor argument, from which
the solver deduces it: `solvers::LocalSearch{runner, RNG{seed}}`,
`solvers::MultiStart{runner, parameters, RNG{seed}}`, and for a pipeline
`solvers::pipeline<RNG>(stages...)`.

## Built-in solvers

| Solver | Built with | Behaviour |
| --- | --- | --- |
| `solvers::LocalSearch` | `LocalSearch{runner}` | one initial solution, one run |
| `solvers::MultiStart` | `MultiStart{runner, {.starts = n}}` (`MultiStartParameters`) | up to `starts` runs from fresh solutions, keeps the best by cost semantics |
| `solvers::Pipeline` | stages with `\|` or `pipeline(...)` (below) | runners in sequence, each stage from the solution of the previous one |

Results carry the effort of the whole solve: `evaluations` and `iterations`
add up over MultiStart's starts and a pipeline's stages and attempts. With a
`cost::pareto` cost, the `front` of MultiStart's result merges the fronts of
all its starts, and so does the front of a stage over its attempts.

`MultiStart`'s starts and a pipeline stage's attempts run in the same loop.
It ends early when a run is cancelled, reaches the target or its time limit,
or when the solve's time or evaluations are spent; the termination is then
`cancelled`, `target_reached`, `time_limit_reached` or
`evaluation_budget_exhausted`, otherwise the last run's.

A solve's time limit and evaluation budget (`solve(input,
easylocal::timeout(30s))`, `easylocal::max_evaluations(1'000'000)`, or both,
`with(control).timeout(30s).max_evaluations(1'000'000)`) bound all its runs
together: `LocalSearch` gives them to its one run, `MultiStart` starts no run
once they are spent and gives each start what is left, and a pipeline gives
each stage what is left.

The observer of the caller's `run_control` sees the progress of the whole
solve: MultiStart and a pipeline give each run the caller's stop token with an
observer that adds the evaluations and iterations of the runs before it, and
the evaluation limit it reports is the solve's, so a progress bar never goes
back from one start, attempt or stage to the next. Its observer of the best
costs gets only the costs better than those of the runs before, by the cost
semantics of the runner of each run: a new start does not report a worse
first cost. A stage on another cost than the observed one reports none.

## Pipeline

A pipeline runs its stages in order over the same Input and Solution: the
first from an initial solution, each other from the solution of the previous
one. A stage is any runner, with its own recipes (cost, neighborhood), named:

```cpp
using namespace easylocal::solvers;
auto solver = (stage("feasible", descent) & until_feasible() & attempts(10))
    | stage("descent", descent)
    | stage("anneal", annealing);
auto result = solver.seed(7).solve(input);
```

`&` gives a stage its options and `|` chains the stages. `&` binds tighter
than `|`, but GCC warns about `a & b | c` (`-Wparentheses`): put a stage and
its options in parentheses. The same pipeline spelled out, with methods:

```cpp
auto solver = pipeline(stage("feasible", descent).until_feasible().with_attempts(10))
                  .then(stage("descent", descent))
                  .then(stage("anneal", annealing));
```

`pipeline(a, b, c)` is `a | b | c`, and `pipeline(a)` a pipeline of one stage.
The stages must have the same Input and Solution (checked at compile time);
their costs may differ.

| Stage option | Method | Effect |
| --- | --- | --- |
| `& target(cost)` | `.with_target(cost)` | the stage stops as soon as its best cost reaches `cost`, in the stage's own cost |
| `& until_feasible()` | `.until_feasible()` | the stage runs `runner.with_hard_cost()` until the hard cost is zero; requires a `cost::hierarchical` cost |
| `& attempts(n)` | `.with_attempts(n)` | up to `n` runs while the target is not reached, keeping the best; the first stage starts each from a new initial solution, the others from the solution they received |
| `& restart(tag)` | `.with_restart(tag)` | the attempts after the first start from a new solution built as the [initialization](#initialization) tag says, rather than as the first; checked at compile time against the stage's SolutionManager |
| `& max_evaluations(n)` | `.max_evaluations(n)` | the stage stops once it has made `n` evaluations, its attempts together; the next stage runs with what is left of the solve's budget |
| `& timeout(d)` | `.timeout(d)` | the stage stops once `d` (a `std::chrono` duration or seconds) has passed since it started, its attempts together; the next stage runs with what is left of the solve's time |

With a `cost::hard_soft` cost expression, `until_feasible()` evaluates only the
components of the hard branch, and deltas attached to soft components are
ignored; with another expression producing a hierarchical cost it evaluates
them all and keeps the hard part.

The pipeline's `.initialization(...)` and `.seed(...)` are those of every
solver, and refer to its first stage.
A caller's target applies to the last stage, unless that stage has its own:
the solve then ends `target_reached` at the stage's own target, whatever the
caller's. Stage names are checked when the pipeline is built, and a stage
rejects run options that carry a control (`stage & with(control)`, which it
would drop: the control goes to `solve()`), and a floating-point target for an
integer cost, which would be truncated.
After a cancellation, or once the solve's time or evaluations are spent, the
stages between the first and the last are skipped without binding their
runner (their report has 0 attempts and the reason as termination), and the
last stage runs once, which only evaluates the solution, so the result still
has the last stage's cost.

`pipeline.run(input, solution, rng, options...)` runs the stages from a given
solution with the caller's RNG: the first stage's attempts all start from that
solution, unless the stage restarts them (`& restart(initialization::random)`,
a multi-start). It is how an app runs a pipeline registered by name.

An algorithm that declares itself deterministic (`static constexpr bool
deterministic = true`, as First and Best Improvement do) repeats the same run
from the same solution: a stage of one with more than one attempt that would
all start from the same solution, without `restart(...)`, is rejected with
`std::invalid_argument`, which suggests `restart(initialization::random)`.
That is a stage after the first, at the pipeline's construction, or the first
stage when it starts from the initial solution or, in an app, from the current
solution, at the run; a custom algorithm without the trait is not checked.

The result is the last stage's, with the effort of every stage and attempt,
and `result.stages`: per stage its name, attempts, evaluations, iterations,
termination and cost (as `cost::to_text` writes it). A stage's termination is
why it stopped, not why its best attempt did: a run cancelled, out of time or
at the target, the stage's or the solve's budget spent, or else the end of its
last attempt; the result's termination is the last stage's.

The parameters (`configuration()`) are each stage's under its name: its
runner's (`<name>.search.*`, `<name>.cost.*`, ...), `<name>.attempts` and
`<name>.timeout` (seconds; `inf`, the default, for no limit of its own) and
`<name>.max_evaluations` (`unlimited` by default).
Stage names must be distinct and non-empty: the pipeline's construction throws
`std::invalid_argument` otherwise.

### Algorithm stages

A pipeline [registered in an app](app-and-tools.md#pipelines) may name its
stages by algorithm rather than by runner: `stage<Algorithm>(name,
parameters)`, or `stage<Algorithm>(name, parameters, neighborhood)` with a
neighborhood recipe of its own. Such a stage has no recipes: when the app runs
the pipeline, it becomes the stage of the runner of `Algorithm` on the app's
SolutionManager and cost, and on the app's neighborhood or its own, with the
recipes and the parameters the app had when it was bound.

```cpp
using namespace easylocal::solvers;
auto application = app("tsp") | sm | two_opt
    | pipeline("cascade",
          stage<FirstImprovement>("feasible") & until_feasible() & attempts(5)
              & restart(initialization::random),
          stage<SimulatedAnnealing<Classic>>("anneal", {...}),
          stage("polish", polishing));  // a stage of a runner, with its own recipes
```

| | Stage of a runner, `stage(name, runner)` | Stage of an algorithm, `stage<A>(name, ...)` |
| --- | --- | --- |
| Where | any pipeline | a pipeline registered in an app |
| Cost and SolutionManager | the runner's own | the app's |
| Neighborhood | the runner's own | the app's, or the third argument |
| Options | `&` and the `with_*` methods | `&`: `attempts`, `timeout`, `max_evaluations`, `target`, `until_feasible`, `restart` |
| Parameters, under `runners.<pipeline>.<stage>` | `attempts`, `timeout`, `max_evaluations`, `search.*`, `cost.*`, `neighborhood.*` | `attempts`, `timeout`, `max_evaluations`, `search.*` and, with its own neighborhood, `neighborhood.*` |

The two kinds mix in one pipeline. The app's `cost.*` parameters apply to
every stage of an algorithm, where each stage of a runner has a copy of its
own. `until_feasible()` derives the hard-cost stage from the app's cost, which
must be hierarchical; a `target` is written in the stage's cost, as for a
stage of a runner; `restart` is checked against the app's SolutionManager. A
stage of an algorithm with its own neighborhood is checked when it is
registered, as a [runner with its own neighborhood](app-and-tools.md) is: the
neighborhood explores the app's Solution and the algorithm runs on it.

### two_stage()

`solvers::two_stage(first, second)` is the pipeline of the hierarchical
hard/soft model, `(stage("first", first) & until_feasible()) | stage("second",
second)`: the second stage works on the whole cost. `two_stage(runner)` uses the same
runner for both. Its parameters are `first.*` and `second.*`.

All solvers expose `supports_initial`, `supports_random`, `seed(n)`,
`initialization(tag)` and `rng()`; for a pipeline they refer to its first
stage.

## Exceptions

An exception thrown during a run, by a hook of the problem (a cost component,
a move of the neighborhood explorer, the SolutionManager) or by the tracer,
is not caught: it ends the run and leaves `runner.run()` and `solver.solve()`
as it is. A solver loses the runs before it: MultiStart's best start so far, a
pipeline's earlier stages and their reports. The run emits no `run_finished`,
so a trace ends with an unfinished run ([Tracing](../tracing.md)). An app
passes it on as well: `Session::run` leaves the current solution as it was and
`last_run_effort()` empty, and `cli::run` writes `error: <what>` (`unknown
exception` for what is not a `std::exception`) and returns 1.

## Initialization

| Tag | Starts from | Requires |
| --- | --- | --- |
| `initialization::automatic` (the default) | a random solution when the SolutionManager builds one, else its initial solution | either |
| `initialization::initial` | `initial_solution()` | `initial_solution()` |
| `initialization::random` | `random_solution(rng)`, with the solver's RNG | `random_solution(rng)` |

A tag is checked at compile time against the SolutionManager: a solver given
one it does not support does not compile. Besides `automatic`, there is never a
fallback from one to the other.

## Design choices

- **The solver owns randomness.** One RNG feeds random initialization and
  random-aware runners, so a seed reproduces the whole solve. The RNG is
  seeded once, at construction or with `.seed(...)`: each `solve()`
  continues its stream, so a seed reproduces the sequence of solves, and a
  second `solve()` differs from the first. Seed again to repeat one.
- **Runners stay solution-to-solution.** Construction belongs to solvers, which
  keeps runners composable.
- **One way to chain runners.** The hierarchical hard/soft model is a pipeline
  of two stages, so `two_stage()` and a longer pipeline share their options,
  their result and their parameters.
