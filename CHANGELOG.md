# Changelog

All notable changes to EasyLocal++ will be documented in this file.

The project uses semantic versioning. Release entries are prepared from the
commits since the previous release and are reviewed manually before tagging.

### Unreleased — application adapters and isolated runs

- **Breaking:** headers are organized by component specialization (EL3 style):
  `core/` (cost, cost semantics, aggregation, logging), `helpers/` (problem
  components and recipes), `runners/` (runner framework and algorithms),
  `solvers/`, `app/` (app, check, Tester), `trace`, `config/`, `testing/`, and
  optional `adapters/` (`toml.hpp`, `tui/`, `rest/`). Search algorithms move
  from `easylocal::search` to `easylocal::runners`.
  Composition recipes move out of the runner into `helpers/recipes.hpp`;
  SolutionManager/NeighborhoodExplorer concepts, customization points and
  convenience bases are consolidated in `helpers/solution_manager.hpp` and
  `helpers/neighborhood_explorer.hpp`; Simulated Annealing policies live in
  `runners/simulated_annealing.hpp` (`runners::temperature`,
  `runners::MetropolisAcceptance`); `trace.hpp` is split into `trace/`.
  Directory layering is enforced by the architecture test.
- **Breaking:** solvers use class-as-key like runners: `solver::local_search`,
  `solver::multistart`, `solver::two_stage`, `make_local_search_solver` and
  `make_multi_start_solver` are removed in favour of
  `make_solver<solvers::LocalSearch|MultiStart|TwoStage>(runner, config)` (or
  direct construction with CTAD); `LocalSearchSolver`, `MultiStartSolver` and
  `TwoStageSolver` become `easylocal::solvers::LocalSearch`, `MultiStart` and
  `TwoStage`, with their configs in `easylocal::solvers`.
- **Breaking:** cost models move to `cost/` and `easylocal::cost`, replacing
  `easylocal::aggregation`, `easylocal::cost_semantics` and the cost concepts
  in `easylocal`: `aggregation::lexicographic_cost`/`hierarchical_cost` become
  `cost::lexicographic`/`cost::hierarchical`, constructed directly with CTAD
  (`cost::hierarchical{hard, soft}`) instead of the `lexicographic{}(...)` /
  `hierarchical{}(...)` builders; `arithmetic_cost` and `delta_cost` become
  `cost::arithmetic` and `cost::has_delta`; `delta(...)` becomes `cost::delta`
  and Metropolis acceptance now uses it instead of `operator-`;
  `runners::numeric_cost` is removed. Logging moves to `utils/logging.hpp`.
- **Breaking:** `max_evaluations` of First/Best Improvement defaults to 0 (no
  budget: run until a local optimum); Best Improvement is configurable like
  First Improvement.
- **Breaking:** the materialized app is the *runtime*: `app_instance` becomes
  `app_runtime` and the Tester exposes `runtime()` / `runtime_type`.
- `easylocal::search_result_for` formalizes the result contract consumed by
  solvers and tools; `cost::hard_projection` formalizes the aggregator's
  `hard(...)` projection used by TwoStage.
- `<easylocal/testing.hpp>` aggregates the component contract checks.
- EasyLocal's own targets build with strict warnings, as errors in the presets
  and CI.
- Documentation: `docs/quick-start.md`, a chapter-per-page tutorial in
  `docs/tutorial/` with a running TSP example, and per-component reference pages
  (contract, API, design choices) in `docs/reference/`. The quick start and the
  tutorial example are built and run as tests, and
  `scripts/sync-doc-snippets.py --check` keeps the snippets in the pages
  identical to them. Dedicated chapters cover checking a composed problem
  (`check`, Tester checks), the TextUI and a REST service; their programs are
  built with the optional components, and the REST one is exercised over HTTP.
  The TextUI chapter shows screenshots generated from the real program by
  `scripts/tui-snapshots.py`. The Python scripts run in a uv environment
  (`pyproject.toml`, `uv.lock`; `uv run scripts/<script>.py`).
- Tools give stochastic runners an RNG they own, from a configurable seed:
  `Tester{app, seed}` / `set_seed`, the TextUI `seed` option (also editable on
  its Run page), and REST's per-run
  `seed` (default `blueprint_options::seed + run id`, reported in run status).
  `app.run_at_with_rng<Index>(input, solution, rng, options...)` passes the RNG
  only to algorithms that take one. Simulated Annealing is registrable in apps
  with its temperature policy's `parameters_type`, whose blocks now have valid
  defaults.
- Every Simulated Annealing temperature policy (`Classic`, `FixedLength`,
  `Cutoff`, `Hybrid`) is configurable.
- **Breaking:** the cost always comes from cost components. A SolutionManager
  only defines solution semantics (its `evaluate()` is no longer used), a
  recipe without components is rejected, and the `Runner::with_solution_manager<SM>(...)`
  / `app(...).solution_manager<SM>(...)` overloads for bare SolutionManagers
  are removed. `evaluable_solution_manager` becomes an internal concept.
- **Breaking:** the implicit aggregator is used only when unambiguous: identity
  for a single component (any value type, no warning), a unit-weight
  `weighted_sum` with a warning for several arithmetic components; several
  domain values require an explicit aggregator. TSP uses the explicit
  `TourLengthCost` and `TourLengthValue` no longer defines `operator*`.
- **Breaking:** one construction and two composition spellings. Runners and
  solvers are built with `make_runner` / `make_solver` and composed with pipes
  or equivalent `with_*` calls; `make_solution_manager` /
  `make_neighborhood_explorer` and `Runner::with_neighborhood<NHE>(...)` are
  removed. The app builder follows the same grammar:
  `.with_solution_manager(sm).with_neighborhood(nhe).with_runner<A>("name",
  parameters)` (formerly `.solution_manager` / `.neighborhood` / `.runner`), or
  `app("name") | sm | nhe | runner<A>("name", parameters)`; registration accepts
  the runner parameters, and `.neighborhood<NHE>(...)` is removed.
- **Breaking:** search algorithms define a single `run(Run&, solution, ...)`
  against the framework-owned `easylocal::search_run`, which owns counters,
  evaluation budget, cancellation, progress reporting and core trace events.
  Every runner is cancellable; `run_controlled*`, `supports_run_control` and the
  REST `stoppable` field/`run_not_cancellable` error are removed. Control and
  tracer are passed as `run(..., easylocal::with(control, tracer))`. Built-in
  algorithms return `easylocal::search_result` with `termination_reason`.
- **Breaking:** runner tags (`runner_tag.hpp`, `runner::algorithm_tag`,
  `runner::first_improvement`, ...) are removed; the algorithm class is its own
  key (`make_runner<search::FirstImprovement>(...)`,
  `app(...).runner<search::FirstImprovement>("fi")`) and exposes
  `parameters_type`.
- CI now keeps the compiler matrix focused on dependency-free Core, while a
  dedicated optional-components workflow covers forced FetchContent on Linux
  and installed system dependencies on Linux/macOS, including REST/Crow/Asio
  and the real REST HTTP integration test; timed trace microbenchmarks run only
  on release tags or explicit dispatch.
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
  drives success/error/cancellation/retention flows with `curl`, verifies both
  structured and opaque-text input decoding, and verifies partial solution
  retrieval after cooperative cancellation.
- Core now provides a lightweight non-owning `run_control` based on
  `std::stop_token`, with progress snapshots and no overhead on ordinary
  uncontrolled runs; first/best improvement and simulated annealing opt in;
- TextUI exposes cooperative Stop and live evaluation/iteration progress; REST
  reports the same progress in a stable status envelope, uses explicit
  `POST /runs/<id>/cancel` for cooperative stop, reserves `DELETE` for terminal
  cleanup, and preserves partial solutions after cancellation.
- REST run creation now wraps problem-specific opaque JSON under `input` and
  optional `initial_solution`, returns stable `id`/`Location` metadata, uses a
  uniform structured error envelope, and retains a configurable bounded history
  of terminal runs (default 64).

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
