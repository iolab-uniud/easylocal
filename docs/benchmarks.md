# Benchmarks

EasyLocal 4 is measured at every release against EasyLocal 3 (the
[v3.4.1 tag](https://github.com/iolab-uniud/easylocal-legacy/tree/v3.4.1)
of `easylocal-legacy`) and on its own infrastructure. The numbers are produced
in [easylocal-benchmarks](https://github.com/iolab-uniud/easylocal-benchmarks)
on a GitHub-hosted runner (Ubuntu, GCC 16, Release); each EasyLocal release
starts a run.

- **EasyLocal 3 versus EasyLocal 4.** The three example problems (TSP with
  2-opt, Assignment with job reassignment, Exam Timetabling with exam moves)
  are written in both frameworks with the same cost functions, delta
  evaluations and neighborhood orders, and searched from the same initial
  solutions. The TSP is searched a second time with the union of the 2-opt and
  the swap neighborhoods (EasyLocal 3 `SetUnion`, EasyLocal 4
  `neighborhood_union`), which measures the cost of a compound neighborhood in
  both frameworks. Each is measured with delta evaluations for all its cost
  components, for some of them, and for none (the TSP has a single component,
  so no mixed mode): the trajectories are the same, only the speed changes.
  Both frameworks are measured at every release, in the same job on the same
  machine.
- **Infrastructure.** Neighborhood traversal, runner-level search and tracing
  overhead (`infrastructure/` of easylocal-benchmarks).
- **Compilers and architectures.** The EasyLocal 4 side of the comparison
  built by several toolchains, on Linux x86_64 and ARM64, macOS ARM64 and
  Windows x86_64: each platform in a job of its own, its toolchains
  alternating run by run.

The speed-up compares the times per evaluation, because the two frameworks
may explore different trajectories: EasyLocal 3 first descent starts each
scan from a random move, while EasyLocal 4 starts from the first;
EasyLocal 3 steepest descent breaks ties at random; simulated annealing uses
each framework's random numbers. Costs are means over the seeds.

No results have been published yet: they appear after the first run of the
Benchmarks workflow on a release.
