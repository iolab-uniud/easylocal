# Solvers

`<easylocal/solvers.hpp>`; `solvers/local_search.hpp`, `solvers/multi_start.hpp`,
`solvers/pipeline.hpp`, `solvers/two_stage.hpp`, `solvers/initialization.hpp`

A solver goes from an Input to a final solution. It owns the RNG, builds the
initial solutions and orchestrates its runners.

## Construction

```cpp
auto solver = make_solver<solvers::X>(runner, solvers::XConfig<Initialization>{...});
auto result = solver.solve(input);
auto controlled = solver.solve(input, easylocal::with(control, tracer).stop_at(target));
```

The optional trailing [run options](runners.md#run-options) — cancellation,
tracer, target cost — reach every run of the solver. Before each run, the
solver sends the tracer a `run_context` event with the pipeline stage and the
attempt (or the start), which tells the runs of a solve apart in a trace
([Tracing](../tracing.md)).

`make_solver` takes the solver class template as its key and deduces the
runner type. A custom RNG type can be chosen by constructing the solver
directly: `solvers::X<Runner, RNG>{runner, ..., RNG{seed}}`.

## Built-in solvers

| Solver | Config | Behaviour |
| --- | --- | --- |
| `solvers::LocalSearch` | `LocalSearchConfig{initialization, seed}` | one initial solution, one run |
| `solvers::MultiStart` | `MultiStartConfig{parameters = {starts}, initialization, seed}` | up to `starts` runs from fresh solutions, keeps the best by cost semantics |
| `solvers::Pipeline` | built from stages with `\|` or `pipeline(...)` (below) | runners in sequence, each stage from the solution of the previous one |

Results carry the effort of the whole solve: `evaluations` and `iterations`
add up over MultiStart's starts and a pipeline's stages and attempts. With a
`cost::pareto` cost, the `front` of MultiStart's result merges the fronts of
all its starts, and so does the front of a stage over its attempts.

`MultiStart` ends early when a start is cancelled or reaches the target, or
when the solve's time or evaluations are spent; its termination is then
`cancelled`, `target_reached`, `time_limit_reached` or
`evaluation_budget_exhausted`, otherwise `completed`.

A solve's time limit and evaluation budget (`solve(input,
easylocal::timeout(30s))`, `easylocal::max_evaluations(1'000'000)`, or both,
`with(control).timeout(30s).max_evaluations(1'000'000)`) bound all its runs
together: `LocalSearch` gives them to its one run, `MultiStart` starts no run
once they are spent and gives each start what is left, and a pipeline gives
each stage what is left.

## Pipeline

A pipeline runs its stages in order over the same Input and Solution: the
first from an initial solution, each other from the solution of the previous
one. A stage is any runner, with its own recipes (cost, neighborhood), named:

```cpp
using namespace easylocal::solvers;
auto solver = (stage("feasible", descent) & until_feasible() & attempts(10))
    | stage("descent", descent)
    | stage("anneal", annealing);
auto result = solver.initialization(initialization::random).seed(7).solve(input);
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
| `& max_evaluations(n)` | `.max_evaluations(n)` | the stage stops once it has made `n` evaluations, its attempts together; the next stage runs with what is left of the solve's budget |
| `& timeout(d)` | `.timeout(d)` | the stage stops once `d` (a `std::chrono` duration or seconds) has passed since it started, its attempts together; the next stage runs with what is left of the solve's time |

With a `cost::hard_soft` cost expression, `until_feasible()` evaluates only the
components of the hard branch, and deltas attached to soft components are
ignored; with another expression producing a hierarchical cost it evaluates
them all and keeps the hard part.

The pipeline's `.initialization(...)` and `.seed(...)` return the pipeline;
by default it starts from a random solution when the first stage supports it,
with seed 0, as the other solvers.
A caller's target applies to the last stage, unless that stage has its own.
After a cancellation, or once the solve's time or evaluations are spent, the
stages between the first and the last are skipped without binding their
runner (their report has 0 attempts and the reason as termination), and the
last stage runs once, which only evaluates the solution, so the result still
has the last stage's cost.

`pipeline.run(input, solution, rng, options...)` runs the stages from a given
solution with the caller's RNG: the first stage's attempts all start from that
solution. It is how an app runs a pipeline registered by name.

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
Stage names must be distinct and non-empty.

### two_stage()

`solvers::two_stage(first, second)` is the pipeline of the hierarchical
hard/soft model, `(stage("first", first) & until_feasible()) | stage("second",
second)`: the second stage works on the whole cost. `two_stage(runner)` uses the same
runner for both. Its parameters are `first.*` and `second.*`.

All solvers expose `supports_initial`, `supports_random`,
`supports(initialization::Mode)`, `initialization_mode()` (get and set) and
`rng()`; for a pipeline they refer to its first stage.

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

| Spelling | Checked |
| --- | --- |
| `initialization::initial`, `initialization::random` | at compile time against the SolutionManager |
| `initialization::Mode::initial`, `Mode::random` | at runtime, when set |

There is never an implicit fallback from one mode to the other.

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
