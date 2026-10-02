# Solvers

`<easylocal/solvers.hpp>`; `solvers/local_search.hpp`, `solvers/multi_start.hpp`,
`solvers/two_stage.hpp`, `solvers/initialization.hpp`

A solver goes from an Input to a final solution. It owns the RNG, builds the
initial solutions and orchestrates its runners.

## Construction

```cpp
auto solver = make_solver<solvers::X>(runner, solvers::XConfig<Initialization>{...});
auto result = solver.solve(input);
auto controlled = solver.solve(input, easylocal::with(control, tracer).stop_at(target));
```

The optional trailing [run options](runners.md#run-options) — cancellation,
tracer, target cost — reach every run of the solver.

`make_solver` takes the solver class template as its key and deduces the
runner type. A custom RNG type can be chosen by constructing the solver
directly: `solvers::X<Runner, RNG>{runner, ..., RNG{seed}}`.

## Built-in solvers

| Solver | Config | Behaviour |
| --- | --- | --- |
| `solvers::LocalSearch` | `LocalSearchConfig{initialization, seed}` | one initial solution, one run |
| `solvers::MultiStart` | `MultiStartConfig{parameters = {starts}, initialization, seed}` | up to `starts` runs from fresh solutions, keeps the best by cost semantics |
| `solvers::TwoStage` | `TwoStageConfig{initialization, seed}` | stage 1 on the hard cost (`runner.with_hard_cost()`) until it reaches zero, stage 2 on the full cost from the stage-1 solution |

Results carry the effort of the whole solve: `evaluations` and `iterations`
add up over MultiStart's starts and TwoStage's stages.

`MultiStart` ends early when a start is cancelled or reaches the target; its
termination is then `cancelled` or `target_reached`, otherwise `completed`.

`TwoStage` takes one runner (used for both stages) or two. It requires a
`cost::hierarchical` cost. With a `cost::hard_soft` cost expression the first
stage evaluates only the components of the hard branch; with another
expression producing a hierarchical cost it evaluates them all and keeps the
hard part. Stage 1 always stops
at `cost::zero` of the hard cost — a feasible solution — and a caller's target
applies to stage 2. After a cancellation in stage 1, stage 2 only evaluates
the solution, so the result still has its full cost.

All solvers expose `supports_initial`, `supports_random`,
`supports(initialization::Mode)`, `initialization_mode()` (get and set) and
`rng()`.

## Initialization

| Spelling | Checked |
| --- | --- |
| `initialization::initial`, `initialization::random` | at compile time against the SolutionManager |
| `initialization::Mode::initial`, `Mode::random` | at runtime, when set |

There is never an implicit fallback from one mode to the other.

## Design choices

- **The solver owns randomness.** One RNG feeds random initialization and
  random-aware runners, so a seed reproduces the whole solve.
- **Runners stay solution-to-solution.** Construction belongs to solvers, which
  keeps runners composable.
- **TwoStage is specific on purpose.** It targets the hierarchical hard/soft
  model rather than being a generic pipeline of runners.
