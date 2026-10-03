# PFSP example

The Permutation Flowshop Scheduling Problem with makespan minimization, solved
by Tabu Search: the case study of Da Ros, Di Gaspero and Schaerf, "A
performance analysis of tabu list strategies". Like the other examples it is
outside `include/easylocal/` and adds no framework API.

## Model

- `PfspInstance`: the processing time of each job on each machine, read in
  Taillard's matrix layout (the number of jobs and of machines, then one row
  per machine). `instances/small.pfsp` (6 jobs, 3 machines) and
  `instances/medium.pfsp` (20 jobs, 5 machines) are generated with a fixed
  seed; Taillard's benchmark files use the same matrix after their header.
- `Schedule`: the order of the jobs, a permutation.
- `PfspSolutionManager`: the identity order, a random order, validity, and the
  solution identity, `hash(schedule)` over the order (`std::vector` has no
  `std::hash`), used by the features that recognize a schedule met before.
- `MakespanComponent`: the completion time of the last job on the last
  machine. A swap changes the completion times from its first position on, so
  there is no delta evaluator: the runner evaluates candidates in full.
- `SwapJobsNeighborhoodExplorer`: the swap of the jobs at two positions,
  enumerated with the cursor protocol and drawn uniformly at random. A move
  records the jobs it swaps, because the tabu definitions are on jobs:
  - `inverse`: with `SwapJobsNeighborhoodExplorer` (IN1) a swap of jobs a and
    b forbids swapping a and b again; with `SwapEitherJobNeighborhoodExplorer`
    (IN2) it forbids any swap moving a or b. The two are instances of
    `BasicSwapJobsNeighborhoodExplorer<SwapInverse>`, resolved at compile time
    because the inverse is called for every candidate move against every entry
    of the tabu list; `main.cpp` uses IN1;
  - `tabu_attribute`: the pair of jobs, whatever their positions, for
    frequency-based tabu memory.

## Running

`main.cpp` registers `runners::TabuSearch<>` (a fixed-length tabu list,
aspiration by objective, the whole neighborhood explored) as the runner `tabu`
of an app, and runs it with `easylocal::cli::run` from a random schedule:

```text
$ ./easylocal_pfsp_tabu --runners.tabu.tabu_list.tenure=20
$ ./easylocal_pfsp_tabu --target=1250         # stop at a makespan of 1250
$ ./easylocal_pfsp_tabu --help                # every switch
```

The switches are `cli::run`'s (`--instance`, by default
`instances/medium.pfsp`, `--seed`, 2026 by default, `--target`, ...) and the
search's (`--runners.tabu.max_idle_iterations`,
`--runners.tabu.tabu_list.tenure`, ...).

`tests/pfsp.cpp` checks the makespan against a hand computation, the solution
identity, both inverse definitions, and that tabu search improves a random
schedule with either of them.
