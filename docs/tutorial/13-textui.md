# 13. The interactive tester

The **TextUI** is a terminal user interface around the `Tester`: load an Input
and a Solution, browse and apply moves, run the registered runners in the
background with live progress and cancellation, and run the checks of
chapter 12. It is the optional `TUI` component (FTXUI):

```cmake
find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core TUI)
target_link_libraries(tsp_tui PRIVATE EasyLocal::TUI)
```

<!-- snippet: tutorial/tui_main.cpp:tui -->
```cpp
auto application = el::app("tsp")
    | (el::solution_manager<TourManager>() | el::component<TourLength>())
    | (el::neighborhood<TwoOptExplorer>() | el::delta<TourLength, TwoOptLengthDelta>())
    | el::runner<runners::FirstImprovement>("fi")
    | el::runner<runners::SimulatedAnnealing<Classic>>("sa");

el::Tester tester{std::move(application)};
tester.set_input(five_cities());

el::tui::run(
    tester,
    {
        .title = "TSP tester",
        .seed = 2026, // the RNG for random solutions, moves and stochastic runners
    });
```

The program is `examples/tutorial/tui_main.cpp`, built when the TUI component
is enabled (`-DEASYLOCAL_ENABLE_TUI=ON`).

## Loading, saving and displaying

To load and save files and to display values, the TextUI uses optional hooks.
The tutorial provides them as free functions, found by argument-dependent
lookup:

<!-- snippet: tutorial/tsp.hpp:io -->
```cpp
// Optional hooks, found by ADL, that let the tools load, save and display.
[[nodiscard]] inline auto read_input(std::type_identity<Tsp>, std::istream& in) -> Tsp
{
    Tsp tsp; // "n d00 d01 ... d(n-1)(n-1)"
    if (!(in >> tsp.cities))
    {
        throw std::runtime_error{"invalid TSP header"};
    }
    tsp.distance.resize(tsp.cities * tsp.cities);
    for (auto& value : tsp.distance)
    {
        if (!(in >> value))
        {
            throw std::runtime_error{"invalid TSP distances"};
        }
    }
    return tsp;
}

[[nodiscard]] inline auto read_solution(const Tsp& tsp, std::istream& in) -> Tour
{
    Tour tour{std::vector<std::size_t>(tsp.cities)};
    for (auto& city : tour.order)
    {
        if (!(in >> city))
        {
            throw std::runtime_error{"invalid tour"};
        }
    }
    return tour;
}

inline void write_solution(const Tsp&, const Tour& tour, std::ostream& out)
{
    for (const auto city : tour.order)
    {
        out << city << ' ';
    }
    out << '\n';
}

[[nodiscard]] inline auto describe(const Tour& tour) -> std::string
{
    std::string text;
    for (const auto city : tour.order)
    {
        text += std::to_string(city) + ' ';
    }
    return text;
}

[[nodiscard]] inline auto describe(const TwoOpt& move) -> std::string
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

The same hooks serve the headless `Tester` (`load_input`, `load_solution`,
`save_solution`).

## Options

`tui::tester_options` sets the `title`, the initial `seed` of the RNG used for
random solutions, random moves and stochastic runners, and the initial
`input_path` and `solution_path`. The seed can also be changed on the Run page
("Random seed", then Enter or *Apply seed*): the RNG restarts from it, and the
header shows the current seed. `tui::run_launcher(options, apps...)` starts a launcher that
lets the user choose among several apps.

## See also

- [Apps and tools](../reference/app-and-tools.md).

## Next steps

[Chapter 14](14-rest.md) offers the same searches over HTTP.
