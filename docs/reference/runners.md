# Runners

`<easylocal/runners.hpp>`; `runners/runner.hpp`, `runners/search_run.hpp`,
`runners/run_control.hpp`, one header per algorithm

A **runner** couples a search algorithm with the recipes of the services. The
algorithm describes the search; the framework owns its execution.

## Composing a runner

```cpp
auto runner = make_runner<Algorithm>(parameters...)   // the documented construction
            | sm_recipe | nhe_recipe;                  // or .with_solution_manager(sm).with_neighborhood(nhe)

auto bound = runner.bind(input);                       // materializes the services
auto result = bound.run(solution, algorithm_args..., with(control, tracer));
```

| Member | Purpose |
| --- | --- |
| `bind(const Input&)` | build the services for an Input (temporaries are rejected) |
| `configuration<"name">()` | configuration tree of the algorithm and recipes |
| `with_hard_cost()` | the same runner on the hard branch of a hierarchical cost |
| bound: `run(solution, args..., [with(...)])` | run the algorithm |
| bound: `initial_solution()`, `random_solution(rng)`, `input()`, `better(a, b)` | helpers |

## Built-in algorithms

| Algorithm | Needs | Parameters | `iterations` counts |
| --- | --- | --- | --- |
| `runners::FirstImprovement` | `moves` or cursor, `better` | `max_evaluations` (0: until a local optimum) | committed moves |
| `runners::BestImprovement` | `moves` or cursor, `better` | `max_evaluations` (0: until a local optimum) | committed moves |
| `runners::SimulatedAnnealing<Temperature, Acceptance>` | `random_move`, `better`, an acceptance-compatible cost | a temperature policy, an acceptance policy | proposed moves |

The budget is checked only before evaluating a move, so an empty neighborhood
is a local optimum even when the budget is exhausted.

Simulated Annealing returns the best solution found. Temperature policies in
`runners::temperature`, all configurable:

| Policy | Parameters | Stops when |
| --- | --- | --- |
| `Classic` | initial/final temperature, cooling rate, samples per temperature | the final temperature is reached |
| `FixedLength` | initial/final temperature, cooling rate, max iterations | max iterations are spent, spread over the levels |
| `Cutoff` | as FixedLength, plus accepted ratio | max iterations; cools early after enough acceptances |
| `Hybrid` | as Cutoff | max iterations; cools on samples or acceptances |

`runners::MetropolisAcceptance` (the default) requires `cost::delta` (see
[Cost](cost.md)). `SimulatedAnnealing<Policy>` exposes the policy's
`parameters_type` and is constructible from it, so it can be registered in
apps; the parameter blocks have defaults that pass validation.

## Results

Built-in algorithms return `search_result<Solution, Cost>`: `solution`, `cost`,
`evaluations`, `iterations`, `termination` (`termination_reason::completed`,
`local_optimum`, `evaluation_budget_exhausted`, `cancelled`, `target_reached`).
A reached target is the termination reason also when it coincides with a local
optimum or the end of the algorithm; a cancellation takes precedence. Solvers and tools
require only `search_result_for<Result, Solution, Cost>`: `solution` and `cost`.

## Writing an algorithm

An algorithm is a class with a `run` member taking the `search_run` first:

```cpp
class MySearch
{
public:
    using parameters_type = MyParameters;      // only for app registration
    explicit MySearch(MyParameters);

    template<class Run>
    auto run(Run& run, typename Run::solution_type solution /*, extra args */) const;
};
```

Extra `run` arguments (an RNG, for instance) are passed through
`bound.run(solution, extra...)`.

### search_run

| Member | Effect |
| --- | --- |
| `limit_evaluations(n)` | evaluation budget, including the initial evaluation |
| `start(solution) -> evaluation` | first evaluation, `run_started`, progress |
| `should_stop() -> bool` | cancellation, reached target or exhausted budget; the reason is recorded |
| `moves(solution)`, `random_move(solution, rng)` | neighborhood access; unions emit selection events |
| `evaluate_move(solution, current, move) -> candidate` | counts, `move_evaluated`, progress |
| `commit(solution, current, candidate, move)` | applies, `move_accepted` |
| `next_iteration()` | advances the iteration counter |
| `incumbent_updated(previous, cost)` | `incumbent_updated` event; the best cost checked against the target |
| `finish(solution, cost[, reason]) -> search_result` | `local_optimum` (if that is the reason), `run_finished` |
| `better`, `equivalent`, `better_or_equivalent` | cost semantics |
| `evaluations()`, `iterations()`, `target()` | counters, the caller's target cost |
| `context()`, `neighborhood_explorer()`, `input()`, `solution_manager()` | the search context |
| `evaluation()`, `emit(event)`, `tracer()`, `control()` | escape hatches (bypass counters and events) |
| `with_context(ctx)` | a run over a decorated context sharing control, tracer, budget and target |

`evaluation` / `candidate` expose `cost()`; `finish` without a reason uses the
one recorded by `should_stop()`, or `completed`.

## Run options

| Spelling | Effect |
| --- | --- |
| `with(control)` | cancellation (`std::stop_token`) and progress observer |
| `with(tracer)` | semantic trace events |
| `with(control, tracer)` | both |
| `stop_at(target)`, `with(...).stop_at(target)` | stop as soon as the best cost is at least as good as `target` |

`run_control{stop_token, observer}` calls `observer(const run_progress&)` with
`evaluations`, `iterations` and `evaluation_limit`.

The target converts to the runner's cost type and is compared with its cost
semantics (`better_or_equivalent`). `search_run` keeps the best cost from
`start`, `commit` and `incumbent_updated`, so every algorithm that checks
`should_stop()` honours a target without further code. The same options are
accepted by every solver's `solve(input, options)`.

## Design choices

- **One `run` member.** Algorithms describe their logic; counters, budget,
  cancellation, progress and events belong to `search_run`, so they behave the
  same in every algorithm and cannot be forgotten.
- **Every runner is cancellable.** A run always carries a `run_control`; checking
  `should_stop()` is the whole contract. The cost of an inactive control is a
  null check.
- **Tracing is compile-time.** Without a tracer, events are not even
  constructed.
- **Per-run state is local.** `run` is `const`; algorithms are reusable values.
- **The algorithm class is its own key.** No tag types: the class names the
  algorithm in `make_runner` and in app registrations.
