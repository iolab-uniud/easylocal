# EasyLocal

EasyLocal is a **C++23 header-only framework** for local search and
metaheuristics. Version 4 is a complete redesign of EasyLocal++, the
object-oriented framework first described in 2003.

A problem is described by a few components — a solution manager, cost
components, neighborhood explorers and their delta costs — and generic runners
(First and Best Improvement, Simulated Annealing) and solvers (LocalSearch,
MultiStart, TwoStage) search it. Optional components add an interactive
terminal tester, a REST service and TOML configuration.

## Where to start

- [Quick start](quick-start.md): a complete TSP solver in one file.
- [Tutorial](tutorial/README.md): the same solver built one capability per
  chapter, from the problem model to custom runners, solvers, testing, the
  TextUI and a REST service.
- [Reference](reference/README.md): the contract, API and design choices of
  each component.
- [API stability](stability.md): what the 4.x series promises.

## Getting EasyLocal

The source, build instructions and release notes are on
[GitHub](https://github.com/iolab-uniud/easylocal). EasyLocal is released under
the [MIT License](https://github.com/iolab-uniud/easylocal/blob/main/LICENSE).

## Citing EasyLocal

If you use EasyLocal in your research, please cite:

> S. Ceschia, F. Da Ros, L. Di Gaspero, A. Schaerf. *EasyLocal++ a 25-year
> Perspective on Local Search Frameworks: The Evolution of a Tool for the
> Design of Local Search Algorithm*. GECCO '24 Companion, 1658–1667, 2024.
> [doi:10.1145/3638530.3664140](https://doi.org/10.1145/3638530.3664140)

and, for the original design,

> L. Di Gaspero, A. Schaerf. *EasyLocal++: An object-oriented framework for
> flexible design of local search algorithms*. Software: Practice and
> Experience 33(8):733–765, 2003.
> [doi:10.1002/spe.524](https://doi.org/10.1002/spe.524)

The repository's `CITATION.cff` and README have the BibTeX entries.
