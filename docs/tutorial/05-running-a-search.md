# 5. Running a search

## A runner

A **runner** couples a search algorithm with the recipes:

<!-- snippet: tutorial/main.cpp:first-improvement -->
```cpp
auto fi =
    el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
    | sm | nhe;

auto search = fi.bind(tsp);
const auto result = search.run(search.initial_solution());
```

`make_runner` builds the algorithm from its parameters; `bind` materializes the
services for an Input and returns the bound runner (`search`), and `run`
searches from a solution. Composition can also
be written with explicit `with_*` calls, which spell out each step:

<!-- snippet: tutorial/main.cpp:with-spelling -->
```cpp
auto same_runner =
    el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
        .with_solution_manager(
            el::solution_manager<TourManager>().with_cost(
                el::component<TourLength>()))
        .with_neighborhood(
            el::neighborhood<TwoOptExplorer>()
                .with_delta<TourLength, TwoOptLengthDelta>());
```

## Built-in algorithms

The algorithms live in `easylocal::runners`, one header each:

| Algorithm | Header | Needs | Parameters |
| --- | --- | --- | --- |
| `FirstImprovement` | `runners/first_improvement.hpp` | `moves` or cursor | `max_evaluations` (0: until a local optimum) |
| `BestImprovement` | `runners/best_improvement.hpp` | `moves` or cursor | `max_evaluations` (0: until a local optimum) |
| `SimulatedAnnealing<Temperature, Acceptance>` | `runners/simulated_annealing.hpp` | `random_move`, `cost::delta` | a temperature policy |

Simulated Annealing takes a temperature policy (`Classic`, `FixedLength`,
`Cutoff`, `Hybrid` in `runners::temperature`); acceptance defaults to
`runners::MetropolisAcceptance`. Stochastic algorithms receive the RNG as a
`run` argument:

<!-- snippet: tutorial/main.cpp:annealing -->
```cpp
using Classic = runners::temperature::Classic;

auto sa =
    el::make_runner<runners::SimulatedAnnealing<Classic>>(
        Classic{runners::temperature::ClassicParameters{
            .initial_temperature = 10.0,
            .final_temperature = 0.1,
            .cooling_rate = 0.95,
            .samples_per_temperature = 50}})
    | sm | nhe;

std::mt19937_64 rng{42};
auto sa_search = sa.bind(tsp);
const auto annealed = sa_search.run(sa_search.initial_solution(), rng);
```

Metropolis acceptance needs the numeric difference of two costs,
`cost::delta(candidate, current)`: arithmetic costs and `cost::hierarchical`
provide it, `cost::lexicographic` deliberately does not.

## Results

Built-in algorithms return an `easylocal::search_result`:

| Member | Meaning |
| --- | --- |
| `solution` | the final solution (for Simulated Annealing, the best found) |
| `cost` | its cost |
| `evaluations` | evaluations performed, including the initial one |
| `iterations` | committed moves (First/Best Improvement), proposed moves (Simulated Annealing) |
| `termination` | `local_optimum`, `evaluation_budget_exhausted`, `cancelled`, `target_reached` or `completed` |

Solvers and tools only rely on `solution` and `cost`, the
`easylocal::search_result_for` concept.

## See also

- [Runners](../reference/runners.md): `Runner`, the built-in algorithms and
  their parameters.

## Next steps

[Chapter 6](06-combining-neighborhoods.md) adds a second neighborhood.
