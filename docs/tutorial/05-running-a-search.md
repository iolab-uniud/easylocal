# 5. Running a search

## A runner

A **runner** couples a search algorithm with the recipes of the services it
uses. Here First Improvement is combined with the recipes `sm` (the
SolutionManager with `TourLength`, chapter 2) and `nhe` (the 2-opt neighborhood
with its delta cost component, chapter 4), then run on the five cities:

<!-- snippet: tutorial/main.cpp:first-improvement -->
```cpp
auto fi =
    el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
    | sm | nhe;

auto search = fi.bind(tsp);
const auto result = search.run(search.initial_solution());
```

Step by step:

1. `make_runner<runners::FirstImprovement>(parameters)` chooses the algorithm
   and holds its parameters. `FirstImprovementParameters{}` keeps the
   defaults: no budget on the evaluations (`max_evaluations` is
   `easylocal::unlimited`, `unlimited` in text), so the search runs until a
   local optimum. Written inline,
   `make_runner<runners::FirstImprovement>({.max_evaluations = 1000})` sets a
   budget; `fi.parameters()` reads and changes them later.
2. `| sm` gives the runner its SolutionManager, and with it the cost: the
   runner will build tours with `TourManager` and evaluate them with
   `TourLength`.
3. `| nhe` gives it the neighborhood: the moves come from `TwoOptExplorer`,
   and their cost from `TwoOptLengthDelta`.
4. The result, `fi`, is still a description: it holds the algorithm and the
   recipes, but no `TourManager` or `TourLength` exists yet, because they need
   an Input. Nothing has been evaluated.
5. `fi.bind(tsp)` builds the services for this Input: a `TourManager`, a
   `TourLength` and a `TwoOptLengthDelta` that refer to `tsp`, and a
   `TwoOptExplorer` that refers to the `TourManager`. It also builds First
   Improvement from the parameters the runner holds. It returns them as the
   *bound runner* `search`. `tsp` must outlive `search`,
   which only refers to it; `bind` does not accept a temporary Input for this
   reason. The same `fi` can be bound to other Inputs.
6. `search.initial_solution()` asks the bound `TourManager` for the initial
   tour, and `search.run(tour)` runs First Improvement from it: evaluate the
   tour, scan the 2-opt moves, apply the first that lowers the length, repeat
   until no move improves it.
7. `run` returns the result, described under [Results](#results) below: the
   final tour, its cost and the counters of the search.

Composition can also be written with explicit `with_*` calls, which spell out
each step:

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
| `FirstImprovement` | `runners/first_improvement.hpp` | `moves` or cursor | `max_evaluations` (unlimited: until a local optimum) |
| `BestImprovement` | `runners/best_improvement.hpp` | `moves` or cursor | `max_evaluations` (unlimited: until a local optimum) |
| `HillClimbing` | `runners/hill_climbing.hpp` | `random_move` | `max_idle_iterations`, `max_evaluations` (unlimited by default) |
| `LateAcceptanceHillClimbing` | `runners/late_acceptance_hill_climbing.hpp` | `random_move` | `history_length`, `max_idle_iterations`, `max_evaluations` |
| `GreatDeluge` | `runners/great_deluge.hpp` | `random_move`, an arithmetic cost | `initial_level`, `min_level`, `level_rate`, `neighbors_sampled`, `max_evaluations` |
| `TabuSearch<List, Aspiration>`, `FirstImprovementTabuSearch<…>`, `AspirationPlusTabuSearch<…>`, `EliteCandidateTabuSearch<…>` | `runners/tabu_search.hpp` | `moves` or cursor, `inverse` | `max_idle_iterations`, `max_iterations`, `max_evaluations`, `tabu_list`, `candidates` (but `TabuSearch`) |
| `SimulatedAnnealing<Temperature, Acceptance>` | `runners/simulated_annealing.hpp` | `random_move`, `cost::delta` | `temperature`: the policy's |

Simulated Annealing takes a temperature policy (`Classic`, `FixedLength`,
`Cutoff`, `Hybrid`, `FixedTemperature`, `TimeBased` in `runners::temperature`,
and `Reheating<…>` over any of them but `FixedTemperature`); acceptance defaults to
`runners::MetropolisAcceptance`. Its parameters are the policy's, under
`temperature`: `{.temperature = {...}}`, and `search.temperature.*` in a
configuration (chapter 9). With `calibration_samples` > 0 a policy
estimates its initial temperature from moves sampled at the initial solution.
Stochastic algorithms receive the RNG as a `run` argument:

<!-- snippet: tutorial/main.cpp:annealing -->
```cpp
using Classic = runners::temperature::Classic;

auto sa =
    el::make_runner<runners::SimulatedAnnealing<Classic>>({
        .temperature =
            {
                .initial_temperature = 10.0,
                .final_temperature = 0.1,
                .cooling_rate = 0.95,
                .samples_per_temperature = 50,
            },
    })
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
| `solution` | the final solution (for Late Acceptance, Great Deluge, Simulated Annealing and Tabu Search, the best found) |
| `cost` | its cost |
| `evaluations` | evaluations performed, including the initial one |
| `iterations` | committed moves (First/Best Improvement, Tabu Search), proposed moves (Hill Climbing, Late Acceptance, Great Deluge, Simulated Annealing) |
| `termination` | `local_optimum`, `evaluation_budget_exhausted`, `cancelled`, `target_reached`, `idle_limit_reached` or `completed` |

Solvers and tools only rely on `solution` and `cost`, the
`easylocal::search_result_for` concept.

## Reading the instance, printing the solution

A program usually reads its Input from a file and prints the solution it
finds. Both go through optional hooks on your types. The tutorial writes them
as free functions next to the types, found by argument-dependent lookup:

<!-- snippet: tutorial/tsp.hpp:io -->
```cpp
// Optional hooks, found by ADL, that read, write and describe the values
// (chapter 5).
inline Tsp read_input(std::type_identity<Tsp>, std::istream& in)
{
    std::size_t cities = 0; // "n", then the n rows of the distance matrix
    if (!(in >> cities))
        throw std::runtime_error{"invalid TSP header"};
    Tsp tsp{.distance = std::vector(cities, std::vector<double>(cities))};
    for (auto& row : tsp.distance)
        for (auto& value : row)
            if (!(in >> value))
                throw std::runtime_error{"invalid TSP distances"};
    return tsp;
}

inline Tour read_solution(const Tsp& tsp, std::istream& in)
{
    Tour tour{std::vector<std::size_t>(tsp.cities())};
    for (auto& city : tour.order)
        if (!(in >> city))
            throw std::runtime_error{"invalid tour"};
    return tour;
}

inline void write_solution(const Tsp&, const Tour& tour, std::ostream& out)
{
    for (const auto city : tour.order)
        out << city << ' ';
    out << '\n';
}

inline std::string describe(const Tour& tour)
{
    std::string text;
    for (const auto city : tour.order)
        text += std::to_string(city) + ' ';
    return text;
}

inline std::string describe(const TwoOpt& move)
{
    return "2-opt(" + std::to_string(move.i) + ", " + std::to_string(move.j) + ")";
}
```

| Hook | Enables |
| --- | --- |
| `read_input(std::type_identity<Input>, std::istream&)` (or `static Input::read`, or `operator>>`) | reading an Input |
| `read_solution(const Input&, std::istream&)` (or `static Solution::read`, or `operator>>`) | reading a Solution |
| `write_solution(const Input&, const Solution&, std::ostream&)` (or `Solution::write`, or `operator<<`) | writing a Solution |
| `describe(value)` (or a `describe()` member, or `operator<<`) | the text of an Input, a Solution or a Move, for people |

`<easylocal/app/io.hpp>` reads and writes values through them. Here the five
cities come from `five.tsp`, next to the tutorial's sources, and the tour
found is printed:

<!-- snippet: tutorial/main.cpp:load-and-print -->
```cpp
// The same five cities, read from a file with the read_input hook.
const auto from_file = el::load_input<Tsp>(EASYLOCAL_TUTORIAL_INSTANCE);
auto file_search = fi.bind(from_file);
const auto file_result = file_search.run(file_search.initial_solution());
std::cout << "from file " << el::describe(file_result.solution) << '\n';
```

| Function | |
| --- | --- |
| `read_input<Input>(in)`, `load_input<Input>(path)` | an Input, from a stream or a file |
| `read_solution<Solution>(input, in)`, `load_solution<Solution>(input, path)` | a Solution of `input` |
| `write_solution(input, solution, out)`, `save_solution(input, solution, path)` | writes a Solution |
| `describe(value)` | the text of a value |

They throw `std::runtime_error` when a stream fails, and the file functions
name the file. Each hook is needed only where it is used: a program that
builds its Input in code, as the rest of this tutorial does, needs no
`read_input`. The Session (chapter 11) and the interactive tester (chapter 12)
use the same hooks.

## See also

- [Runners](../reference/runners.md): `Runner`, the built-in algorithms and
  their parameters.

## Next steps

[Chapter 6](06-combining-neighborhoods.md) adds a second neighborhood.
