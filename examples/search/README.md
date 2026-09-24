# Experimental search incubator

This directory contains search/metaheuristic implementations used to pressure-test
EasyLocal++ framework contracts before those algorithms are considered stable
public API.

The code here is intentionally **example-local and experimental**. In particular:

- `SimulatedAnnealing` is used to exercise random neighborhood traversal, explicit
  RNG ownership, evaluation services, and acceptance policies;
- `MetropolisAcceptance` is a provisional policy used by that experiment;
- their current parameter/result/termination shapes are not framework contracts;
- random proposal semantics, acceptance-policy requirements, and temperature
  scheduling are still open design questions.

Do not include these headers as public EasyLocal++ API. Stable search algorithms
live under `include/easylocal/search/` and are promoted there only after their
contracts have been pressure-tested and deliberately accepted.
