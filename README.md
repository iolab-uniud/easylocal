<h1>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/branding/easylocal-logo-dark.svg">
    <img alt="EasyLocal" src="docs/assets/branding/easylocal-logo.svg" height="64">
  </picture>
</h1>

[![CI](https://github.com/iolab-uniud/easylocal/actions/workflows/ci.yml/badge.svg)](https://github.com/iolab-uniud/easylocal/actions/workflows/ci.yml)
[![Optional Components](https://github.com/iolab-uniud/easylocal/actions/workflows/optional-components.yml/badge.svg)](https://github.com/iolab-uniud/easylocal/actions/workflows/optional-components.yml)
[![Coverage](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2Fiolab-uniud%2Feasylocal%2Fbadges%2Fcoverage.json)](https://github.com/iolab-uniud/easylocal/actions/workflows/ci.yml)
[![Documentation](https://github.com/iolab-uniud/easylocal/actions/workflows/docs.yml/badge.svg)](https://iolab-uniud.github.io/easylocal/)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

EasyLocal is a **C++23 header-only framework** for local search and
metaheuristics. Version 4 is a complete redesign of EasyLocal++, the
object-oriented framework first described in 2003 (see
[Citing EasyLocal](#citing-easylocal)).

A problem is described by a few components: a solution manager, cost
components, neighborhood explorers and their delta cost components. Generic
runners search it (First and Best Improvement, Hill Climbing, Late Acceptance,
Great Deluge, Simulated Annealing, Tabu Search, and Pareto Late Acceptance for
several objectives), and solvers combine runs (LocalSearch, MultiStart,
Pipeline). A program built on them gets a command line, parameter tuning with
[irace](https://mlopez-ibanez.github.io/irace/) and, as optional components,
an interactive terminal tester, a REST service and TOML configuration.

## A first look

A neighborhood explorer lists the moves of a solution, one at a time:

<!-- snippet: quickstart/main.cpp:neighborhood -->
```cpp
// NeighborhoodExplorer: which moves exist and how they change a solution.
class SwapExplorer : public easylocal::neighborhood_explorer_base<TourManager, SwapCities>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    // Every pair of positions i < j, one move at a time: the generator hands a
    // move to the runner at each co_yield and resumes when it asks for the
    // next one, so the neighborhood is never stored in memory.
    easylocal::generator<SwapCities> moves(const Tour& tour) const
    {
        const auto n = tour.order.size();
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j)
                co_yield SwapCities{i, j};
    }

    bool is_valid(const Tour& tour, const SwapCities& move) const
    {
        return move.i < move.j && move.j < tour.order.size();
    }

    void make_move(Tour& tour, const SwapCities& move) const
    {
        std::swap(tour.order[move.i], tour.order[move.j]);
    }
};
```

A runner is assembled from the algorithm and the problem's components, bound
to an instance and run:

<!-- snippet: quickstart/main.cpp:runner -->
```cpp
// A runner is assembled from recipes: descriptions of the services that
// are built later, by bind, when the Input is known.
auto runner =
    // The algorithm and its parameters: the defaults run First Improvement
    // until a local optimum, with no budget on the evaluations.
    easylocal::make_runner<easylocal::runners::FirstImprovement>(
        easylocal::runners::FirstImprovementParameters{})
    // The SolutionManager with its cost: the parentheses make one recipe
    // of the manager and the cost component attached to it.
    | (easylocal::solution_manager<TourManager>()
        | easylocal::component<TourLength>())
    // The neighborhood: the moves the algorithm explores.
    | easylocal::neighborhood<SwapExplorer>();

// bind builds TourManager, TourLength and SwapExplorer for this instance
// and returns the bound runner; run searches from the initial tour.
auto search = runner.bind(tsp);
const auto result = search.run(search.initial_solution());
```

The whole program, a swap-move First Improvement for the TSP, is
[`examples/quickstart/main.cpp`](examples/quickstart/main.cpp).

## Documentation

The documentation is published at <https://iolab-uniud.github.io/easylocal/>:

- the [quick start](docs/quick-start.md) builds and runs the program above;
- the [tutorial](docs/tutorial/README.md) builds a TSP solver one chapter at a
  time, from the problem model to the apps, the TextUI and the REST service;
- the [reference](docs/reference/README.md) describes the contract and the
  design of each component, and the
  [API reference](https://iolab-uniud.github.io/easylocal/api/) every
  declaration;
- [Coming from EasyLocal 3](docs/from-easylocal-3.md) maps the old classes to
  the new ones, and [API stability](docs/stability.md) says what 4.x promises.

The examples under [`examples/`](examples/) are complete programs: an
assignment problem, the TSP, exam timetabling and the permutation flow shop,
each with a README that guides through its files.

## Requirements

- a C++23 compiler: CI covers GCC 15 and 16, Clang 22 and 23 (libstdc++ and
  libc++), AppleClang and clang-cl; AppleClang needs Xcode 26 or later and a
  macOS deployment target (`CMAKE_OSX_DEPLOYMENT_TARGET`) of 26.0 or later, for
  the floating-point `std::from_chars` of its libc++, and CMake stops with a
  message otherwise;
- CMake 3.25 or later and Ninja.

The core uses only the standard library. The optional components need toml++,
FTXUI or Crow, found on the system or fetched by CMake when
`EASYLOCAL_FETCH_DEPENDENCIES=ON`; see
[Dependencies](docs/dependency-policy.md).

## Building and installing

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure -j
```

The `release` preset builds with optimizations. An optional component is
enabled with `-DEASYLOCAL_ENABLE_CONFIG_TOML=ON`, `-DEASYLOCAL_ENABLE_TUI=ON`
or `-DEASYLOCAL_ENABLE_REST=ON`.

A build tree installs to any prefix, and a CMake project then uses it through
`find_package`:

```sh
cmake --install build/release --prefix /path/to/prefix
```

```cmake
find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core)
target_link_libraries(my_solver PRIVATE EasyLocal::Core)
```

## Contributing

[`AGENTS.md`](AGENTS.md) describes how the repository is maintained, for people
and coding agents alike: its layout, the code style, the checks to run before a
commit, the CI and the releases. Every user-visible change goes in
[`CHANGELOG.md`](CHANGELOG.md); planned evolutions are in the
[roadmap](docs/roadmap.md).

## Citing EasyLocal

If you use EasyLocal in your research, please cite the 2024 overview paper; the
2003 article describes the original design. `CITATION.cff` carries the same
information for GitHub's "Cite this repository".

```bibtex
@inproceedings{CeschiaDaRosDiGasperoSchaerf2024,
  author    = {Ceschia, Sara and Da Ros, Francesca and Di Gaspero, Luca and Schaerf, Andrea},
  title     = {{EasyLocal++} a 25-year Perspective on Local Search Frameworks: The Evolution of a Tool for the Design of Local Search Algorithm},
  booktitle = {Proceedings of the Genetic and Evolutionary Computation Conference Companion},
  series    = {GECCO '24 Companion},
  pages     = {1658--1667},
  year      = {2024},
  publisher = {Association for Computing Machinery},
  address   = {New York, NY, USA},
  doi       = {10.1145/3638530.3664140},
}

@article{DiGasperoSchaerf2003,
  author  = {Di Gaspero, Luca and Schaerf, Andrea},
  title   = {{EasyLocal++}: An object-oriented framework for flexible design of local search algorithms},
  journal = {Software: Practice and Experience},
  volume  = {33},
  number  = {8},
  pages   = {733--765},
  year    = {2003},
  doi     = {10.1002/spe.524},
}
```

## Authors

EasyLocal is developed at the University of Udine by Sara Ceschia, Francesca
Da Ros, Luca Di Gaspero and Andrea Schaerf.

## License

EasyLocal is released under the [MIT License](LICENSE), as EasyLocal++ was.
The optional components use permissively licensed dependencies: toml++ and
FTXUI (MIT), Crow (BSD-3-Clause) and Asio (Boost Software License 1.0).
