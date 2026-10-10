# 5. Running a search

## A runner

A **runner** combines an algorithm with the problem recipes. We now have both:
`sm` describes the SolutionManager and cost (chapter 2), and `nhe` describes
2-opt moves and their delta (chapter 4). Attach them to First Improvement and
run it on the five-city instance:

<!-- snippet: tutorial/main.cpp:first-improvement -->
```cpp
auto fi =
    el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
    | sm | nhe;

auto search = fi.bind(tsp);
const auto result = search.run(search.initial_solution());
```

There are three steps:

1. **Compose.** `make_runner` selects the algorithm and stores its parameters;
   `| sm | nhe` attaches the services in that order. The resulting `fi` still
   needs no Input and has constructed no services.
2. **Bind.** `fi.bind(tsp)` constructs the algorithm, `TourManager`,
   `TourLength`, `TwoOptExplorer` and `TwoOptLengthDelta`. The returned
   *bound runner*, `search`, owns these objects and borrows `tsp`, which must
   outlive it. For this reason, `bind` rejects a temporary Input.
3. **Run.** `search.initial_solution()` creates a starting tour.
   `search.run(tour)` repeatedly scans the moves and takes the first
   improvement, stopping when none remains. It returns the final tour, cost
   and search counters (see [Results](#results)).

`FirstImprovementParameters{}` leaves `max_evaluations` at
`easylocal::unlimited` (`unlimited` in text), so this run reaches a local
optimum. To cap its work, pass `{.max_evaluations = 1000}` to `make_runner`.
You can also change the parameters through `fi.parameters()` before binding.

The same `fi` can be bound to other Inputs. Reusing the recipes also makes it
easy to try another algorithm on exactly the same problem.

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

Algorithms live in `easylocal::runners`, with headers under `runners/`.
Choose one that supports your neighborhood and cost. The
[Runners reference](../reference/runners.md#built-in-algorithms) lists their
parameters and requirements:

| Algorithm | Header | Needs |
| --- | --- | --- |
| `FirstImprovement`, `BestImprovement` | `first_improvement.hpp`, `best_improvement.hpp` | `moves` or cursor |
| `HillClimbing`, `LateAcceptanceHillClimbing` | `hill_climbing.hpp`, `late_acceptance_hill_climbing.hpp` | `random_move` |
| `GreatDeluge` | `great_deluge.hpp` | `random_move`, an arithmetic cost |
| `SimulatedAnnealing<Temperature, Acceptance>` | `simulated_annealing.hpp` | `random_move`, `cost::delta` |
| `TabuSearch<List, Aspiration>` and its three variants | `tabu_search.hpp` | `moves` or cursor, `inverse` |
| `ParetoLateAcceptanceHillClimbing` | `pareto_late_acceptance_hill_climbing.hpp` | `random_move`, a `cost::pareto` cost, `random_solution` |

Every one bounds its run with `max_evaluations` (unlimited by default: a
descent stops at a local optimum).

Simulated Annealing lets you choose a temperature policy from
`runners::temperature`: `Classic`, `FixedLength`, `Cutoff`, `Hybrid`,
`FixedTemperature` or `TimeBased`. `Reheating<…>` wraps any of these except
`FixedTemperature`. Acceptance defaults to `runners::MetropolisAcceptance`.

For `Classic`, the parameter block is
`SimulatedAnnealingParameters<ClassicParameters>`. It groups schedule settings
under `temperature` and also holds the run's evaluation budget:
`{.temperature = {...}, .max_evaluations = 10000}`. Configuration paths use
`search.temperature.*` (chapter 9). Setting `calibration_samples` above zero
estimates the initial temperature from sampled moves.

Pass an RNG to `run` for a stochastic algorithm:

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
provide it, `cost::lexicographic` deliberately does not. The delta of a
`cost::hard_soft` cost is infinite when the hard cost worsens, so Simulated
Annealing never accepts such a move: the hard cost of its current solution
never increases. To let it cross infeasible regions, as EasyLocal 3's
`HARD_WEIGHT` did, write the cost as one weighted sum,
`cost::sum(component<Hard>() * 1000, component<Soft>())`.

## Results

Built-in algorithms return an `easylocal::search_result` (Pareto Late
Acceptance a `pareto_search_result`, which adds the front):

| Member | Meaning |
| --- | --- |
| `solution` | the final solution (for Late Acceptance, Great Deluge, Simulated Annealing and Tabu Search, the best found) |
| `cost` | its cost |
| `evaluations` | evaluations performed, including the initial one |
| `iterations` | committed moves (First/Best Improvement, Tabu Search), proposed moves (Hill Climbing, Late Acceptance, Great Deluge, Simulated Annealing) |
| `termination` | `local_optimum`, `evaluation_budget_exhausted`, `time_limit_reached`, `cancelled`, `target_reached`, `idle_limit_reached` or `completed` |

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
    // The 2-opt delta reverses a segment without its cost: symmetric only.
    for (std::size_t i = 0; i < cities; ++i)
        for (std::size_t j = 0; j < i; ++j)
            if (tsp.distance[i][j] != tsp.distance[j][i])
                throw std::runtime_error{"the TSP distances are not symmetric"};
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

These helpers throw `std::runtime_error` on stream failures; file errors name
the affected path. Implement only the hooks you use: a program that constructs
its Input in code needs no `read_input`.

The Session (chapter 11) and interactive tester (chapter 13) reuse these hooks,
so adding a frontend does not require another file parser or solution writer.

## See also

- [Runners](../reference/runners.md): `Runner`, the built-in algorithms and
  their parameters.

## Next steps

[Chapter 6](06-combining-neighborhoods.md) adds a second neighborhood.
