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
| `runners::FirstImprovement` | `moves` or cursor, `better` | `max_evaluations` (unlimited: until a local optimum) | committed moves |
| `runners::BestImprovement` | `moves` or cursor, `better` | `max_evaluations` (unlimited: until a local optimum) | committed moves |
| `runners::HillClimbing` | `random_move`, `better`, `better_or_equivalent` | `max_idle_iterations`, `max_evaluations` | proposed moves |
| `runners::LateAcceptanceHillClimbing` | `random_move`, `better`, `better_or_equivalent` | `history_length`, `max_idle_iterations`, `max_evaluations` | proposed moves |
| `runners::ParetoLateAcceptanceHillClimbing` | `random_move`, `better`, a `cost::pareto` cost, `random_solution` | `history_length`, `max_iterations`, `idle_ratio`, `second_chance`, `max_evaluations` | proposed moves |
| `runners::GreatDeluge` | `random_move`, `better`, an arithmetic cost | `initial_level`, `min_level`, `level_rate`, `neighbors_sampled`, `max_evaluations` | proposed moves |
| `runners::TabuSearch<List, Aspiration>` | `moves` or cursor, `better`, `inverse` | `max_idle_iterations`, `max_iterations`, `max_evaluations`, `tabu_list`: the list's | committed moves |
| `runners::FirstImprovementTabuSearch<List, Aspiration>` | as TabuSearch | as TabuSearch, plus `improve_on_best` | committed moves |
| `runners::AspirationPlusTabuSearch<List, Aspiration>` | as TabuSearch, an arithmetic cost | as TabuSearch, plus `min_moves`, `max_moves`, `plus`, `aspiration_level` | committed moves |
| `runners::EliteCandidateTabuSearch<List, Aspiration>` | as TabuSearch, an arithmetic cost | as TabuSearch, plus `elite_size`, `quality` | committed moves |
| `runners::SimulatedAnnealing<Temperature, Acceptance>` | `random_move`, `better`, an acceptance-compatible cost | a temperature policy (`temperature`), an acceptance policy, `max_evaluations` | proposed moves |

The limits on a count, `max_evaluations` and `max_iterations`, are of type
`easylocal::limit`: a number, or `easylocal::unlimited`, written `unlimited` in
a configuration file, on the command line and in the TextUI. They are
unlimited by default, and 0 is a limit of zero, not "no limit".

The budget is checked only before evaluating a move, so an empty neighborhood
is a local optimum even when the budget is exhausted.

Hill Climbing accepts a random move when its cost is better or equivalent to
the current one, so it moves across plateaus where a descent stops. It ends
with `idle_limit_reached` after `max_idle_iterations` consecutive proposals
without a strict improvement, or with `local_optimum` when the neighborhood
proposes no move. Its cost never worsens, so the final solution is the best.
Every runner that draws random moves (Late Acceptance, Great Deluge, Simulated
Annealing, Pareto Late Acceptance) also ends with `local_optimum` when the
neighborhood proposes none.

Late Acceptance Hill Climbing (Burke and Bykov) also accepts a move whose cost
is better than or equivalent to the cost the current solution had
`history_length` proposals earlier; the history starts filled with the initial
cost, and with `history_length` 1 it accepts the moves Hill Climbing accepts.
Its idle count runs from the last improvement of the best cost, and it returns
the best solution found. EasyLocal 3 recorded the best cost in the history;
EasyLocal 4 records the current cost, as in the original algorithm.

Pareto Late Acceptance Hill Climbing (Da Ros and Di Gaspero) is the
multi-objective variant, for a `cost::pareto` cost: its history holds
solutions, the initial one and `history_length - 1` drawn with the solution
manager's `random_solution`, visited in a circle. A random move of the current
solution replaces it in the history when the candidate dominates it, and the
search moves on to the next solution of the history. Otherwise, with
`second_chance`, a candidate that dominates the next solution replaces it, and
the search goes on from the solution it replaced, two positions on; else it
moves on to the next solution. Past `max_iterations` the search ends with
`idle_limit_reached` as soon as more than `idle_ratio` of the iterations went
without a replacement. Its result is the run's front, with the first solution
by objectives as `solution`.

Tabu Search applies at each iteration the best admissible move of the whole
neighborhood, even a worsening one, ties broken uniformly at random. A move is
admissible unless the tabu list forbids it, through the neighborhood's
`inverse` (see [NeighborhoodExplorer](neighborhood-explorer.md)), and the
aspiration criterion does not lift the prohibition:
`aspiration::ByObjective` (the default) admits a tabu move that would improve
the best cost, `aspiration::None` never does and then skips evaluating tabu
moves. When every move is tabu, the least tabu one (the shortest remaining
tenure) is applied. The search ends with `idle_limit_reached` after
`max_idle_iterations` iterations without improving the best cost, and returns
the best solution found. `FirstImprovementTabuSearch` stops the scan at the
first admissible move that improves the current cost (with `improve_on_best`,
the best cost); without one it applies the best admissible move. Both take the
RNG as a `run` argument, for the ties.

Two runners implement Glover's candidate list strategies, whose levels are
values of the cost (an arithmetic cost):

- `AspirationPlusTabuSearch` examines admissible moves until `plus` more after
  the first one under the aspiration level (`aspiration_level` times the best
  cost), but at least `min_moves` and at most `max_moves`, and applies the best
  examined.
- `EliteCandidateTabuSearch` keeps, from a full scan, the `elite_size` best
  admissible moves besides the one applied; the next iterations evaluate only
  the kept moves still valid and apply the best admissible one while its cost
  is at most `quality` times the best cost, otherwise a full scan builds a new
  list.

Tabu lists in `runners::tabu`; their parameters are the group `tabu_list`:

| List | Parameters | A move stays tabu |
| --- | --- | --- |
| `FixedLength` (TS1) | `tenure` | for `tenure` iterations after the move it would undo |
| `RandomTenure` (TS2) | `min_tenure`, `max_tenure` | for a tenure drawn uniformly in `[min_tenure, max_tenure]` |
| `Cyclic` (TS3) | `period`, `tenures` | for the current tenure, which takes the `tenures` in turn every `period` iterations |
| `Reactive` (TS4) | `increase`, `decrease`, `repetitions`, `chaos`, `cycle_length`, `max_tenure`, `verify_equality` | for a tenure that reacts to revisited solutions; needs the solution hash and `random_move` |
| `Frequency` (TS5) | `threshold` | while its attribute was applied in more than `threshold` of the iterations; needs `tabu_attribute`, not `inverse` |
| `ObjectiveBased` | `tenure` | while its cost equals one reached in the last `tenure` iterations; needs neither, but `==` on costs |
| `LimDynamic` | `min_tenure`, `max_tenure`, `idle_threshold` | for a tenure that grows by one after `idle_threshold` idle iterations and falls back to `min_tenure` on an improvement or at `max_tenure` |
| `Foo` | `window`, `increment`, `fluctuation` | for a tenure that grows by `increment` when the costs of the last `window` iterations spread less than `fluctuation`, and shrinks by one otherwise; needs `cost::delta` |
| `RandomFoo` | ranges of the three | as `Foo`, drawing them again at each window |

Most lists forbid moves through the neighborhood's `inverse`, `Frequency`
through its `tabu_attribute`, `ObjectiveBased` through the candidate's cost,
evaluating each move before the tabu check (see
[NeighborhoodExplorer](neighborhood-explorer.md)); each requires only what it
uses. `Reactive` (Battiti and Tecchiolli) starts with tenure 1 and recognizes
solutions by `solution_hash` (see [SolutionManager](solution-manager.md)); a
hash collision counts as a revisit, unless `verify_equality` keeps a copy of
each visited solution and compares those with the same hash, which needs
solution equality (`has_solution_equality`, otherwise the run throws
`std::invalid_argument`). A solution revisited within `cycle_length`
iterations multiplies the tenure by `increase` (up to `max_tenure`) and updates
the average cycle length; without such cycles for longer than the average, the
tenure is multiplied by `decrease`. A solution visited more than `repetitions`
times counts as chaos; after more than `chaos` counts the memory is reset and
the search escapes with `1 + (1 + r) * average / 2` random moves (`r` uniform
in `[0, 1)`), applied whatever their cost, counted as iterations and
recorded in the list like the others.

A tabu list is a value with its parameters that makes, for each run, a state
with `make_state<Run>()`. `tabu_tenure(candidate)` gives the iterations left
before a candidate move is admissible, nothing when it is (`tabu_candidate`:
`move()`, `forbidden_by(tabu_move)` through the inverse, `attribute()`, and
`cost()` when the state declares `static constexpr bool needs_cost = true`);
`update(step, rng)` records an applied move (`tabu_step`: `move()`,
`solution()`, `cost()`, `iteration()`, `improved_best()`, `attribute()`,
`solution_hash()`, the last two when the problem has them). A state with
`escape_moves()` asks for that many random moves, and one with
`current_tenure()`, a tenure for all its moves, has it traced as
`tabu_tenure_changed` at the start of the run and whenever it changes.
`tabu_list_for` checks a custom list.

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
| `Cutoff` | as FixedLength, plus accepted ratio | max iterations; cools only after `accepted_ratio` of a level's share of them have been accepted, never on samples |
| `Hybrid` | as Cutoff | max iterations; cools on samples or acceptances |
| `FixedTemperature` | temperature, max iterations, accepted ratio | max iterations, or enough acceptances; never cools |
| `TimeBased` | initial/final temperature, cooling rate, running time, accepted per temperature (unlimited: cools only on time) | the running time is over or the final temperature is reached; levels share the time |
| `Reheating<Descent>` | `descent`: the schedule's, plus max reheats, reheat ratio, first-descent share | the last descent ends; each reheat restarts from `reheat_ratio` times T0 |

`TimeBased` reads the clock (`std::chrono::steady_clock`; `BasicTimeBased<Clock>`
takes another one) once per proposal. Its trajectory depends on the speed of
the machine, so equal seeds do not give equal runs.

`Reheating<Descent>` reheats any schedule whose parameters have an
`initial_temperature` (all but `FixedTemperature`): a first descent, then up to
`max_reheats` descents that restart from `reheat_ratio` times the initial
temperature. When the schedule has a budget, `max_iterations` or
`allowed_running_time`, the first descent spends `first_descent_share` of it
and the reheats divide the rest evenly; `Classic`, which has none, runs whole
at every descent. The schedule's parameters are the group `descent`:
`{.temperature = {.descent = {...}, .max_reheats = 2}}`,
`search.temperature.descent.*` in a configuration. It calibrates when the
schedule does, keeping the reheat temperature above the final one.
`Reheating<Hybrid>` is EasyLocal 3's annealing with reheating.

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
group `temperature` and the evaluation budget `max_evaluations`:
`make_runner<SimulatedAnnealing<Classic>>({.temperature = {...}})`,
`search.temperature.*` in a configuration. The parameter blocks have defaults
that pass validation. A calibrating schedule with `calibration_samples` above 0
needs a cost with `cost::delta`; with any other cost the run throws
`std::invalid_argument`.

## Results

Built-in algorithms return `search_result<Solution, Cost>`: `solution`, `cost`,
`evaluations`, `iterations`, `termination` (`termination_reason::completed`,
`local_optimum`, `evaluation_budget_exhausted`, `cancelled`, `target_reached`,
`idle_limit_reached`, `time_limit_reached`). With a `cost::pareto` cost they return
`pareto_search_result<Solution, Cost>`, which adds `front`: the non-dominated
solutions the run reached, as `pareto_point{solution, cost}`, ordered by their
objectives. The `search_run` keeps them in a `pareto_archive` as the run starts,
evaluates solutions and commits moves, so every algorithm has a front without
doing anything; a solution enters unless an archived one dominates it or is the
same (equal cost and, with solution equality, an equal solution), and removes
those it dominates.
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
| `limit_evaluations(n)` | evaluation budget, including the initial evaluation: a count, or `easylocal::unlimited` |
| `start(solution) -> evaluation` | first evaluation, `run_started`, progress |
| `evaluate_solution(solution) -> evaluation` | another solution (a population, a history): counts, traced as visited, offered to the archive |
| `front()` | with a `cost::pareto` cost, the archive of the non-dominated solutions reached |
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

An algorithm that returns the best solution it visited, rather than the last,
keeps it in a `best_so_far{solution, cost}`: `best.update(run, solution,
current)` copies the solution when its evaluation is better than the best,
emits `incumbent_updated`, and returns whether it did; at the end
`run.finish(std::move(best.solution), std::move(best.cost))`. Late Acceptance,
Great Deluge, Simulated Annealing and the tabu searches use it.

## Run options

| Spelling | Effect |
| --- | --- |
| `with(control)` | cancellation (`std::stop_token`) and progress observer |
| `with(tracer)` | semantic trace events |
| `with(control, tracer)` | both |
| `stop_at(target)`, `with(...).stop_at(target)` | stop as soon as the best cost is at least as good as `target` |
| `options.without_target()` | the same options without their target (a pipeline gives them to a stage that is not the last) |
| `max_evaluations(n)`, `with(...).max_evaluations(n)` | stop once the run has made `n` evaluations, the initial one included; a runner's own `max_evaluations`, if smaller, still applies; termination `evaluation_budget_exhausted` |
| `timeout(5s)`, `timeout(2.5)`, `with(...).timeout(...)` | stop once the time limit has passed since the run started: a `std::chrono` duration or a number of seconds; termination `time_limit_reached` |

`run_control{stop_token, observer}` calls `observer(const run_progress&)` with
`evaluations`, `iterations` and `evaluation_limit`. A frontend that shows the
progress from another thread stores it in a `shared_run_progress` from the
observer and loads a copy when it draws, as the TextUI and the REST server do.

The options combine in any order:
`with(control).timeout(30s).max_evaluations(100000).stop_at(0)`. A
negative time limit, a NaN one (a number of seconds or a floating-point
duration) and an infinite number of seconds throw `std::invalid_argument`; a
duration beyond what the clock counts is the longest it can. `search_run`
checks the deadline with the other stopping conditions, reading the clock at
an interval of checks that adapts so that readings come about a millisecond
apart: a loop that checks at every move reads it rarely, one that checks once
per long iteration reads it every time. There is no timer thread.

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
