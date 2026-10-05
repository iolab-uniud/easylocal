# TSP example

The symmetric Travelling Salesman Problem: visit every city once and come back
to the start, along the shortest tour. The example solves it with two
neighborhoods, 2-opt and swap, each with a delta cost component, and runs them
with Simulated Annealing and with First Improvement.

## The files

| Component | File | Class |
| --- | --- | --- |
| Input | `instance.hpp` | `TspInstance`: the number of cities and the distance matrix |
| Solution | `solution.hpp` | `Tour`: the order in which the cities are visited |
| Solution manager | `solution_manager.hpp` | `TspSolutionManager`: the initial tour (0, 1, 2, ...), a random tour, validity |
| Cost component | `tour_length_component.hpp` | `TourLengthComponent`: the length of the tour, a `double` |
| Move | `move.hpp` | `TwoOptMove`: remove two edges, reverse the segment between them |
| Neighborhood explorer | `neighborhood_explorer.hpp` | `TwoOptNeighborhoodExplorer` |
| Delta cost component | `tour_length_delta.hpp` | `TwoOptTourLengthDelta`: the two edges removed and the two added |
| Move | `swap_move.hpp` | `SwapCitiesMove`: exchange the cities at two positions |
| Neighborhood explorer | `swap_neighborhood_explorer.hpp` | `SwapCitiesNeighborhoodExplorer` |
| Delta cost component | `swap_tour_length_delta.hpp` | `SwapTourLengthDelta`: the at most four edges around the two positions |

The programs put the components together:

- `apps.hpp`: two apps over the same solution manager and cost, one for each
  neighborhood, with a First Improvement runner `fi`, and `tsp_app()`, one
  app whose runner `fi` uses 2-opt, the app's neighborhood, and `fi-swap`
  swaps, a neighborhood of its own;
- `sa_main.cpp` (`easylocal_tsp_sa`): Simulated Annealing on the union of the
  two neighborhoods, which draws a 2-opt move three times as often as a swap
  (`random_biases(3.0, 1.0)`), run from the command line by `cli::run`;
- `two_neighborhoods.cpp` (`easylocal_tsp_two_neighborhoods`): the runners of
  `tsp_app()` in a row, First Improvement with 2-opt moves from the initial
  tour, then with swaps from the tour it found;
- `tui_main.cpp` (`easylocal_tsp_tui`): the two apps in the interactive
  terminal tester, built only with the TUI component
  (`-DEASYLOCAL_ENABLE_TUI=ON`).

## What to look at first

1. `instance.hpp`, `solution.hpp` and `solution_manager.hpp`: the problem and
   its solutions.
2. `tour_length_component.hpp`: the cost, a sum over the edges of the tour.
3. `move.hpp` and `neighborhood_explorer.hpp`: `moves()` lists every 2-opt
   move with `co_yield`, `random_move()` draws one, `make_move()` applies it.
4. `tour_length_delta.hpp`: how much a move changes the length, computed from
   the edges it replaces, without building the new tour.
5. `sa_main.cpp`: how the pieces become a program.

## Building and running

From the repository root:

```sh
cmake --preset dev && cmake --build build/dev
./build/dev/examples/tsp/easylocal_tsp_sa
```

The program reads `instances/small.tsp` (6 cities: the count, then the
distance matrix row by row), starts from the initial tour and prints the best
tour found and its length. Its parameters are switches:

```sh
./build/dev/examples/tsp/easylocal_tsp_sa --help    # every switch
./build/dev/examples/tsp/easylocal_tsp_sa \
  --runners.sa.temperature.allowed_iterations=50 \
  --neighborhood.random_biases='[1, 4]'
./build/dev/examples/tsp/easylocal_tsp_sa --target=26   # stop at length 26
./build/dev/examples/tsp/easylocal_tsp_sa --config examples/tsp/configs/small.cfg
```

`--instance` reads another instance and `--seed` changes the random seed
(2026 by default). A value in a `--config` file overrides the default, and a
switch overrides both.

The tests `tests/tsp_*.cpp` check the moves of both neighborhoods, the delta
cost components against a full evaluation of the cost, and the runners on
this model.
