# PFSP example

The Permutation Flowshop Scheduling Problem: jobs go through the same machines
in the same order, and every machine processes the jobs in the same order. A
solution is that order; its cost is the makespan, the time at which the last
job leaves the last machine. The example solves it with Tabu Search, as in Da
Ros, Di Gaspero and Schaerf, "A performance analysis of tabu list strategies".

## The files

| Component | File | Class |
| --- | --- | --- |
| Input | `instance.hpp` | `PfspInstance`: the processing time of each job on each machine |
| Solution | `solution.hpp` | `Schedule`: the order of the jobs |
| Solution manager | `solution_manager.hpp` | `PfspSolutionManager`: the initial order (0, 1, 2, ...), a random order, validity, and a hash of the order |
| Cost component | `makespan_component.hpp` | `MakespanComponent`: the makespan |
| Move | `swap_move.hpp` | `SwapJobsMove`: exchange the jobs at two positions |
| Neighborhood explorer | `swap_neighborhood_explorer.hpp` | `SwapJobsNeighborhoodExplorer` and `SwapEitherJobNeighborhoodExplorer` |

There is no delta cost component: a swap changes the completion times of every
job from its first position on, so computing the change costs as much as
computing the makespan again. The runner evaluates each move on a copy of the
schedule.

`main.cpp` (`easylocal_pfsp_tabu`) puts them together: an app with a Tabu
Search runner `tabu` (a tabu list of 10 moves, a stop after 1000 iterations
without improvement), run from the command line by `cli::run` from a random
schedule.

## What to look at first

1. `instance.hpp` and `solution.hpp`: the problem and its solutions.
2. `makespan_component.hpp`: the completion time of each job on each machine,
   one job after the other.
3. `swap_neighborhood_explorer.hpp`: `moves()` lists every swap with
   `co_yield`, `random_move()` draws one, `make_move()` applies it. Tabu Search
   also needs two more members:
   - `inverse(solution, move, tabu_move)`: whether `move` is forbidden while
     `tabu_move` is in the tabu list. A move records the jobs it swaps,
     because the tabu definitions are on jobs: with
     `SwapJobsNeighborhoodExplorer` (IN1) a swap of jobs a and b forbids
     swapping a and b again; with `SwapEitherJobNeighborhoodExplorer` (IN2) it
     forbids any swap that moves a or b. The two are the same template,
     `BasicSwapJobsNeighborhoodExplorer<SwapInverse>`, chosen at compile time;
     `main.cpp` uses IN1;
   - `tabu_attribute(move)`: the pair of jobs, for frequency-based tabu memory.
4. `main.cpp`: how the pieces become a program.

## Building and running

From the repository root:

```sh
cmake --preset dev && cmake --build build/dev
./build/dev/examples/pfsp/easylocal_pfsp_tabu
```

The program reads `instances/medium.pfsp` (20 jobs, 5 machines) and prints the
best order found and its makespan. The file is in Taillard's layout: the
number of jobs and of machines, then one row per machine with the processing
time of each job; `instances/small.pfsp` has 6 jobs and 3 machines. Its
parameters are switches:

```sh
./build/dev/examples/pfsp/easylocal_pfsp_tabu --help    # every switch
./build/dev/examples/pfsp/easylocal_pfsp_tabu --runners.tabu.tabu_list.tenure=20
./build/dev/examples/pfsp/easylocal_pfsp_tabu --target=1250   # stop at makespan 1250
./build/dev/examples/pfsp/easylocal_pfsp_tabu --instance examples/pfsp/instances/small.pfsp
```

`--seed` changes the random seed (2026 by default).

`tests/pfsp.cpp` checks the makespan against a hand computation, the validity
and the hash of a schedule, both tabu definitions, and that Tabu Search
improves a random schedule with either of them.
