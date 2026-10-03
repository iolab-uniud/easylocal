# Runners

`<easylocal/runners.hpp>`; `runners/runner.hpp`, `runners/search_run.hpp`,
`runners/run_control.hpp`, one header per algorithm

A **runner** couples a search algorithm with the recipes of the services. The
algorithm describes the search; the framework owns its execution.

## Composing a runner

```cpp
auto runner = make_runner<Algorithm>({...})            // the algorithm's parameters
            | sm_recipe | nhe_recipe;                  // or .with_solution_manager(sm).with_neighborhood(nhe)

auto search = runner.bind(input);                      // materializes the services
auto result = search.run(solution, algorithm_args..., with(control, tracer));
```

When the algorithm's `parameters_type` is a parameter block (a schema and
`validate()`), the runner holds the parameters and builds the algorithm when it
is bound: `make_runner<Algorithm>(parameters)` creates it, and
`Runner{Algorithm{...}}` is rejected at compile time. Any other algorithm is
held as an object: `make_runner<Algorithm>(args...)` or `Runner{Algorithm{...}}`.

| Member | Purpose |
| --- | --- |
| `parameters()` | the algorithm's parameters, to read or change (parameterized algorithms) |
| `bind(const Input&)` | build the services and the algorithm for an Input (temporaries are rejected) |
| `configuration()` | the parameters of the algorithm (`search`), the cost (`cost`) and the neighborhood (`neighborhood`), as a `config::parameter_set` |
| `with_hard_cost()` | the same runner on the hard branch of a hierarchical cost |
| bound runner: `run(solution, args..., [with(...)])` | run the algorithm |
| bound runner: `initial_solution()`, `random_solution(rng)`, `input()`, `better(a, b)` | helpers |

## Built-in algorithms

| Algorithm | Needs | Parameters | `iterations` counts |
| --- | --- | --- | --- |
| `runners::FirstImprovement` | `moves` or cursor, `better` | `max_evaluations` (0: until a local optimum) | committed moves |
| `runners::BestImprovement` | `moves` or cursor, `better` | `max_evaluations` (0: until a local optimum) | committed moves |
| `runners::HillClimbing` | `random_move`, `better`, `better_or_equivalent` | `max_idle_iterations`, `max_evaluations` (0: no budget) | proposed moves |
| `runners::LateAcceptanceHillClimbing` | `random_move`, `better`, `better_or_equivalent` | `history_length`, `max_idle_iterations`, `max_evaluations` (0: no budget) | proposed moves |
| `runners::GreatDeluge` | `random_move`, `better`, an arithmetic cost | `initial_level`, `min_level`, `level_rate`, `neighbors_sampled`, `max_evaluations` (0: no budget) | proposed moves |
| `runners::SimulatedAnnealing<Temperature, Acceptance>` | `random_move`, `better`, an acceptance-compatible cost | a temperature policy, an acceptance policy | proposed moves |

The budget is checked only before evaluating a move, so an empty neighborhood
is a local optimum even when the budget is exhausted.

Hill Climbing accepts a random move when its cost is better or equivalent to
the current one, so it moves across plateaus where a descent stops. It ends
with `idle_limit_reached` after `max_idle_iterations` consecutive proposals
without a strict improvement, or with `local_optimum` when the neighborhood
proposes no move. Its cost never worsens, so the final solution is the best.

Late Acceptance Hill Climbing (Burke and Bykov) also accepts a move whose cost
is better than or equivalent to the cost the current solution had
`history_length` proposals earlier; the history starts filled with the initial
cost, and with `history_length` 1 it accepts the moves Hill Climbing accepts.
Its idle count runs from the last improvement of the best cost, and it returns
the best solution found. EasyLocal 3 recorded the best cost in the history;
EasyLocal 4 records the current cost, as in the original algorithm.

Great Deluge accepts a move that improves the current cost or whose cost does
not exceed the water level. The level starts at `initial_level` times the
initial cost and is multiplied by `level_rate` every `neighbors_sampled`
proposals; the search ends, `completed`, when the level falls below
`min_level` times the best cost, and returns the best solution found. The level
is a value of the cost, so Great Deluge requires an arithmetic cost
(`cost::arithmetic`), and positive, since the levels are factors of it: a
best cost of zero ends the search.

Simulated Annealing returns the best solution found. Temperature policies in
`runners::temperature`, all configurable:

| Policy | Parameters | Stops when |
| --- | --- | --- |
| `Classic` | initial/final temperature, cooling rate, samples per temperature | the final temperature is reached |
| `FixedLength` | initial/final temperature, cooling rate, max iterations | max iterations are spent, spread over the levels |
| `Cutoff` | as FixedLength, plus accepted ratio | max iterations; cools early after enough acceptances |
| `Hybrid` | as Cutoff | max iterations; cools on samples or acceptances |
| `FixedTemperature` | temperature, max iterations, accepted ratio | max iterations, or enough acceptances; never cools |
| `TimeBased` | initial/final temperature, cooling rate, running time, accepted per temperature | the running time is over or the final temperature is reached; levels share the time |
| `Reheating` | as Hybrid, plus max reheats, reheat ratio, first-descent share | the last descent ends; each reheat restarts from `reheat_ratio` times T0 |

`TimeBased` reads the clock (`std::chrono::steady_clock`; `BasicTimeBased<Clock>`
takes another one) once per proposal. Its trajectory depends on the speed of
the machine, so equal seeds no longer give equal runs.

`Reheating` runs a first Hybrid descent on `first_descent_share` of
`max_iterations`, then up to `max_reheats` descents that restart from
`reheat_ratio` times the initial temperature and divide the remaining
iterations evenly, as EasyLocal 3's annealing with reheating.

Every built-in policy can estimate its initial temperature (the constant one
for `FixedTemperature`) with `calibration_samples` > 0: before the run, Simulated
Annealing evaluates that many random moves at the initial solution, without
applying them, and the policy starts from the temperature at which a worsening
move of average size is accepted with probability `initial_acceptance` (0.5 by
default; 0.2 suits a good initial solution), as in Johnson et al. (1989).
Improving moves and infinite deltas (a hierarchical hard level) are ignored;
without worsening moves `initial_temperature` stays, and the estimate is kept
above the final temperature. The sampled moves count as evaluations, not as
iterations. A custom policy opts in by modelling
`calibrating_temperature_policy`: `calibration_samples()` and
`calibrate(std::span<const double> deltas)`.

`runners::MetropolisAcceptance` (the default) requires `cost::delta` (see
[Cost](cost.md)). The parameters of `SimulatedAnnealing<Policy>` are
`SimulatedAnnealingParameters<Policy::parameters_type>`, the policy's under the
group `temperature`: `make_runner<SimulatedAnnealing<Classic>>({.temperature =
{...}})`, `search.temperature.*` in a configuration. The parameter blocks have
defaults that pass validation.

## Results

Built-in algorithms return `search_result<Solution, Cost>`: `solution`, `cost`,
`evaluations`, `iterations`, `termination` (`termination_reason::completed`,
`local_optimum`, `evaluation_budget_exhausted`, `cancelled`, `target_reached`,
`idle_limit_reached`).
A reached target is the termination reason also when it coincides with a local
optimum or the end of the algorithm; a cancellation takes precedence. Solvers and tools
require only `search_result_for<Result, Solution, Cost>`: `solution` and `cost`.

## Writing an algorithm

An algorithm is a class with a `run` member taking the `search_run` first:

```cpp
class MySearch
{
public:
    using parameters_type = MyParameters;      // with a schema: configurable
    explicit MySearch(const MyParameters&);

    template<class Run>
    auto run(Run& run, typename Run::solution_type solution /*, extra args */) const;
};
```

With a schema and `validate()` on `MyParameters`, the runner holds them and
builds `MySearch` at bind; they are configurable as `search.*` with no other
member. Without one, `parameters_type` serves only app registration.

Extra `run` arguments (an RNG, for example) are passed through
`search.run(solution, extra...)`.

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
| `incumbent_updated(previous, cost)` | `incumbent_updated` event; the cost checked against the target |
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
semantics (`better_or_equivalent`). `search_run` checks the costs reached in
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
