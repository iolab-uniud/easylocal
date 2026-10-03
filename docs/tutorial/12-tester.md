# 12. The interactive tester

The **interactive tester** (the TextUI) is a Session with a terminal interface:
load an Input and a Solution, browse, evaluate and apply moves, run the
registered runners in the background with live progress and cancellation, and
run the checks of chapter 13. It is the optional `TUI` component (FTXUI):

```cmake
find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core TUI)
target_link_libraries(tsp_tui PRIVATE EasyLocal::TUI)
```

<!-- snippet: tutorial/tui_main.cpp:tui -->
```cpp
auto application = el::app("tsp")
    | (el::solution_manager<TourManager>() | el::component<TourLength>())
    | (el::neighborhood<TwoOptExplorer>()
        | el::delta<TourLength, TwoOptLengthDelta>())
    | el::runner<runners::FirstImprovement>("fi")
    | el::runner<runners::SimulatedAnnealing<Classic>>("sa");

el::tui::run(
    application,
    {
        .title = "TSP tester",
        .seed = 2026, // the RNG for random solutions, moves and stochastic runners
        .input_path = EASYLOCAL_TUTORIAL_INSTANCE, // loaded with the read_input hook
    });
```

`tui::run(application, options)` opens the tester on the app and returns when
the user quits. It loads the Input from `input_path` first, through the
`read_input` hook described below; without one, the user loads it from the
Input/Output page.

The program is `examples/tutorial/tui_main.cpp`, built when the TUI component
is enabled (`-DEASYLOCAL_ENABLE_TUI=ON`).

## A tour of the pages

The tester has three pages, switched with F3, F4 and F5; the header shows the
instance, the current seed and the cost of the current solution.

On the **Input/Output** page, `I` creates the initial solution and `C` runs the
app check of chapter 13:

![The Input/Output page after creating the initial solution and running the check](images/tui-check.svg)

On the **Move** page, `B` selects the best move; the panel shows its incremental
and full evaluation, and `A` would apply it:

![The Move page with the best 2-opt move and its delta check](images/tui-moves.svg)

On the **Run** page, a runner runs in the background with live progress and can
be stopped with `X`; here Simulated Annealing improves the tour from 29 to 26:

![The Run page after a Simulated Annealing run](images/tui-run.svg)

The screenshots are generated from the real program by
`uv run scripts/tui-snapshots.py`, which drives it in a pseudo-terminal. The
same driver (`scripts/tui_driver.py`) runs the end-to-end tests in `tests/tui`:
keys are sent, and each step waits for what the screen should show.

## Loading, saving and displaying

To load and save files and to display values, the TextUI uses optional hooks.
The tutorial provides them as free functions, found by argument-dependent
lookup:

<!-- snippet: tutorial/tsp.hpp:io -->
```cpp
// Optional hooks, found by ADL, that let the tools load, save and display
// (chapter 12).
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
| `read_input(std::type_identity<Input>, std::istream&)` (or `static Input::read`, or `operator>>`) | loading an Input |
| `read_solution(const Input&, std::istream&)` (or `static Solution::read`) | loading a Solution |
| `write_solution(const Input&, const Solution&, std::ostream&)` (or `Solution::write`, or `operator<<`) | saving a Solution |
| `describe(value)` (or a `describe()` member, or `operator<<`) | displaying Input, Solution and Move |
| `name()` on a NeighborhoodExplorer | naming neighborhoods |

The page title of the Move page, *Move - 2-opt*, comes from a static `name()`
member of `TwoOptExplorer`:

<!-- snippet: tutorial/tsp.hpp:two-opt-name -->
```cpp
// The name of the neighborhood in the interactive tester (chapter 12).
static std::string_view name()
{
    return "2-opt";
}
```

## Options

`tui::options` sets the `title`, the `seed` of the RNG used for random
solutions, random moves and stochastic runners, and the initial `input_path`
and `solution_path`. The seed can also be changed on the Run page ("Random
seed", then Enter or *Apply seed*): the RNG restarts from it, and the header
shows the current seed.

## See also

- [Apps and tools](../reference/app-and-tools.md).

## Next steps

[Chapter 13](13-checking.md) checks the composed problem, from code and from
the tester.
