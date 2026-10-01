# Quick start

This page builds and runs a complete local search in one file: a 2-opt First
Improvement for the symmetric Travelling Salesperson Problem (TSP). The
[tutorial](tutorial/README.md) then explains every piece and adds the rest of
the framework one chapter at a time.

## Requirements

- a C++23 compiler: CI covers GCC 15 and 16, Clang 22 (libstdc++ and libc++)
  and AppleClang;
- CMake 3.25+;
- EasyLocal, either installed (`cmake --install <build> --prefix <prefix>`) or
  added to your project with `add_subdirectory`.

EasyLocal Core is header-only and has no dependencies beyond the standard
library.

## The program

<!-- snippet: quickstart/main.cpp -->
```cpp
// Quick start: a 2-opt First Improvement for the symmetric TSP.
// This is the program of docs/quick-start.md; it is built and run as a test.
#include <easylocal/easylocal.hpp>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <numeric>
#include <utility>
#include <vector>

// 1. The problem: Input, Solution and Move are plain values.
struct Tsp
{
    std::size_t cities{};
    std::vector<double> distance; // cities x cities, row-major

    [[nodiscard]] auto d(std::size_t from, std::size_t to) const -> double
    {
        return distance[from * cities + to];
    }
};

struct Tour
{
    std::vector<std::size_t> order;
};

struct TwoOpt
{
    std::size_t i; // reverse the segment order[i + 1 .. j]
    std::size_t j;
};

// 2. SolutionManager: what a valid solution is and how to build one.
class TourManager : public easylocal::solution_manager_base<Tsp, Tour>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]] auto initial_solution() const -> Tour
    {
        Tour tour{std::vector<std::size_t>(input_.cities)};
        std::iota(tour.order.begin(), tour.order.end(), std::size_t{0});
        return tour;
    }

    [[nodiscard]] auto is_valid(const Tour& tour) const -> bool
    {
        return tour.order.size() == input_.cities;
    }
};

// 3. A cost component: one term of the objective.
class TourLength
{
public:
    explicit TourLength(const Tsp& tsp) : tsp_{tsp} {}

    [[nodiscard]] auto evaluate(const Tour& tour) const -> double
    {
        double length = 0.0;
        for (std::size_t k = 0; k < tour.order.size(); ++k)
        {
            length += tsp_.d(tour.order[k], tour.order[(k + 1) % tour.order.size()]);
        }
        return length;
    }

private:
    const Tsp& tsp_;
};

// 4. NeighborhoodExplorer: which moves exist and how they change a solution.
class TwoOptExplorer
    : public easylocal::neighborhood_explorer_base<TourManager, TwoOpt>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]] auto moves(const Tour& tour) const -> std::vector<TwoOpt>
    {
        std::vector<TwoOpt> result;
        const auto n = tour.order.size();
        for (std::size_t i = 0; i + 2 < n; ++i)
        {
            for (std::size_t j = i + 2; j < n && !(i == 0 && j + 1 == n); ++j)
            {
                result.push_back({i, j});
            }
        }
        return result;
    }

    [[nodiscard]] auto is_valid(const Tour& tour, const TwoOpt& move) const -> bool
    {
        return move.i + 2 <= move.j && move.j < tour.order.size();
    }

    void make_move(Tour& tour, const TwoOpt& move) const
    {
        std::reverse(
            tour.order.begin() + static_cast<std::ptrdiff_t>(move.i + 1),
            tour.order.begin() + static_cast<std::ptrdiff_t>(move.j + 1));
    }
};

int main()
{
    const Tsp tsp{
        .cities = 5,
        .distance = {
            0, 2, 9, 10, 7,
            2, 0, 6, 4, 3,
            9, 6, 0, 8, 5,
            10, 4, 8, 0, 6,
            7, 3, 5, 6, 0,
        },
    };

    // 5. Compose a runner: algorithm | SolutionManager recipe | neighborhood.
    auto runner =
        easylocal::make_runner<easylocal::runners::FirstImprovement>(
            easylocal::runners::FirstImprovementParameters{})
        | (easylocal::solution_manager<TourManager>()
           | easylocal::component<TourLength>())
        | easylocal::neighborhood<TwoOptExplorer>();

    // 6. Bind it to an Input and run it from a solution.
    auto bound = runner.bind(tsp);
    const auto result = bound.run(bound.initial_solution());

    std::cout << "length " << result.cost << " after " << result.evaluations
              << " evaluations\n";
}
```

The program is the source of `examples/quickstart/main.cpp`, which is built and
run with the test suite.

## Build and run

```cmake
cmake_minimum_required(VERSION 3.25)
project(tsp LANGUAGES CXX)

find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core)

add_executable(tsp main.cpp)
target_link_libraries(tsp PRIVATE EasyLocal::Core)
```

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/easylocal-prefix
cmake --build build
./build/tsp
```

It prints:

```text
length 26 after 9 evaluations
```

## What happened

1. `Tsp`, `Tour` and `TwoOpt` are plain values: the Input, a Solution and a
   Move.
2. `TourManager` says which tours are valid and builds an initial one.
3. `TourLength` is a cost component; being the only one, its value is the cost.
4. `TwoOptExplorer` lists the 2-opt moves of a tour and applies them.
5. `make_runner<FirstImprovement>(...) | sm | nhe` composes the algorithm with
   the recipes of the services.
6. `bind(tsp)` materializes the services for this Input; `run` searches from
   the initial tour until a local optimum, since no evaluation budget was set.

## Next steps

- [Tutorial](tutorial/README.md): the same problem, extended chapter by chapter.
- [Reference](reference/README.md): the contract of each component.
