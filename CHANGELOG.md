# Changelog

All notable changes to EasyLocal++ will be documented in this file.

The project uses semantic versioning. Release entries are prepared from the
commits since the previous release and are reviewed manually before tagging.

### Unreleased — application adapters and isolated runs

- Add an Assignment TextUI stress/demo runner and a deterministic 250-job instance for visibly exercising asynchronous progress and cooperative stop.
- optional ConfigTOML and TextUI components are exported as independent CMake
  target files and loaded only when requested by `find_package`;
- local `dev`/`release` presets are compiler/platform agnostic;
- `<easylocal/easylocal.hpp>` explicitly covers the dependency-free Core API,
  including app/check/Tester and the std-only configuration surface;
- materialized apps and bound runners expose canonical `input()` access and
  reject temporary Inputs to prevent dangling references;
- deterministic architecture tests enforce that Core never depends on optional
  adapters and adapters do not reach into `easylocal/detail/*`.
- `app.run(...)`/`run_at(...)` execute through a freshly materialized runtime,
  making one-run/one-mutable-runtime semantics explicit and deterministically
  tested, including concurrent calls sharing an immutable Input;
- TextUI runner execution is asynchronous: search runs on a background
  `std::jthread`, while solution commit and UI mutation remain on the FTXUI
  event thread;
- new optional `EasyLocal::REST` component exposes an app as a generic Crow
  Blueprint with a domain codec, asynchronous run IDs, status/solution routes,
  and a bounded solver execution pool separate from Crow HTTP workers;
- REST packaging is lazy and component-aware, with Crow 1.3.3 and standalone
  Asio 1.38.2 available through the explicit dependency-fetch path.
- local build/test profiles now compose optional adapters in one build by default;
  `--exhaustive` explicitly checks every feature subset of the requested profile;
- REST adds a real HTTP integration test that starts the Assignment Crow MWE,
  drives success/error/cancellation flows with `curl`, and verifies partial
  solution retrieval after cooperative cancellation.
- Core now provides a lightweight non-owning `run_control` based on
  `std::stop_token`, with progress snapshots and no overhead on ordinary
  uncontrolled runs; first/best improvement and simulated annealing opt in;
- TextUI exposes cooperative Stop and live evaluation/iteration progress, while
  REST maps active-run `DELETE` to stop requests and reports progress/status,
  preserving partial solutions when a cooperative run is cancelled.

### S20 — Simulated Annealing promotion

Simulated Annealing e' ora API pubblica header-only sotto `easylocal::search`.

- il Runner accetta un NeighborhoodExplorer core con `make_move`; l'enumerazione
  `moves()` e il proposal stocastico `random_move()` sono capability separate;
- `SimulatedAnnealing` usa `random_move(solution, rng) -> std::optional<Move>` e
  restituisce il best-so-far;
- temperature e acceptance sono policy statiche possedute per valore, senza
  metodi virtuali;
- il contratto `temperature_policy` e' `reset()`, `temperature()`,
  `on_iteration(bool accepted)`, `finished()`;
- sono disponibili `temperature::Classic`, `temperature::FixedLength`,
  `temperature::Cutoff` e `temperature::Hybrid`; le ultime tre
  rappresentano rispettivamente le strategie TL1/TL2/TL3 studiate durante S20,
  con redistribuzione del budget residuo nella policy ibrida;
- `MetropolisAcceptance` usa direttamente il costo numerico come energia e
  accetta sempre miglioramenti e uguaglianze;
- `SimulatedAnnealing` rifiuta a compile time costi strutturati, gerarchici o
  lessicografici nel contratto corrente;
- aggiunto l'MWE Exam Timetabling con tre componenti di costo, weighted sum e
  tre delta evaluator;
- aggiunti test deterministici di capability, temperature policy, Metropolis,
  best-so-far, delta e diagnostica compile-fail del costo non numerico.
