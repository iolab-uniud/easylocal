# Changelog

All notable changes to EasyLocal are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project adheres to [Semantic Versioning](https://semver.org/). Entries
are drafted from the Conventional Commits since the previous release and
reviewed by hand before tagging.

## [Unreleased]

## [4.0.0-alpha.2] — not yet released

### Cost

- **Breaking:** unsigned integers are no longer costs. A cost component or a
  `cost::apply` function that returns one, and a `cost::lexicographic`,
  `cost::pareto` or `cost::hierarchical` level of an unsigned type, fail to
  compile with a message: the difference of two unsigned costs wraps around,
  so Simulated Annealing never accepted an improving move.
  `cost::arithmetic` excludes them, weights included.
### Fixed

- The randomized contract checks (`easylocal::testing` and `check(app, ...)`)
  draw from a `std::mt19937_64` seeded with the new `check_options::seed`,
  instead of `deterministic_rng`'s four fixed values: a `random_move()` that
  rejects draws until one fits, such as the tutorial's on 40 cities, no longer
  loops forever, and the random samples differ from each other.
  `deterministic_rng` remains for unit tests that script the draws.
- The ELTR recorders write and flush the header at construction: a run that
  crashes leaves a trace that decodes, without the events of the last block,
  instead of an empty file. `eltr.py` reports an empty file as an empty trace.
- An output error of `trace::async_binary_recorder` stops the recording, not
  the search: `emit` drops the events instead of throwing mid-search, and
  `good()` and `flush()` report the error, a failed final flush included.
- `trace::async_binary_recorder` rejects an unlimited `async_queue_blocks` with
  `std::invalid_argument`, instead of deadlocking at construction.
- A cost writer or an `encode_binary_event` that throws no longer leaves half a
  record in the trace, which made an ELTR file undecodable: the binary
  recorders drop the whole record, and `jsonl_recorder` writes each line at
  once.

## [4.0.0-alpha.1] — 2026-10-04

A pre-release: the API, header paths included, may still change before 4.0.0
([API stability](docs/stability.md#pre-releases)).

First release of **EasyLocal 4**, a complete redesign of EasyLocal++, the
object-oriented local search framework first described in 2003. The design
principles stay — a problem is described by a few components, and generic
algorithms are composed on top of them — but the implementation is new: a
C++23 header-only library based on concepts and value semantics instead of
class hierarchies and virtual dispatch. Code written for EasyLocal 3 needs
porting; [From EasyLocal 3](docs/from-easylocal-3.md) maps the
old concepts onto the new ones.

### Problem model

- A problem is an Input, a Solution, a **SolutionManager** (initial and random
  solutions, solution semantics) and one or more **NeighborhoodExplorers**
  (moves, their application, enumeration and random proposal), checked by
  concepts at compile time.
- Neighborhoods are enumerated lazily, as ranges (typically an
  `easylocal::generator<Move>`, which is `std::generator` where the standard
  library ships it and a minimal equivalent elsewhere) or as EL3-style cursors
  (`first_move` / `next_move`), and random moves are proposed with
  `random_move(solution, rng)`. Several neighborhoods combine into a
  **neighborhood union**.
- Optional **solution identity**: a hash and an equality of solutions, from
  the SolutionManager (`hash`, `equal`) or from the solution type
  (`std::hash`, `operator==`), with `hash_combine` and `hash_range` to write
  one.
- Optional **tabu customization points** of a neighborhood: `inverse`
  (whether a move is forbidden by an earlier one; its definition, such as the
  same pair of jobs or either job, belongs to the explorer) and
  `tabu_attribute` (the attribute frequency memory counts, by default the move
  itself); a neighborhood union dispatches both to its children.
- **Neighborhood parameters**: an explorer that declares a `parameters_type`
  gets its parameters from its recipe, which exposes them as configuration
  (`neighborhood.*`, and `neighborhood.<position>.*` in a union).

### Cost

- The cost always comes from **cost components**, and moves are evaluated
  incrementally by **delta cost components**, mirroring EasyLocal++'s
  CostComponent / DeltaCostComponent.
- Cost models in `easylocal::cost`: plain arithmetic costs,
  `lexicographic<...>`, `hierarchical<Hard, Soft>` and `pareto<...>`
  (Pareto dominance), with explicit better/equivalent semantics. A search
  with a pareto cost keeps the non-dominated solutions it reaches and returns
  them as its front.
- Cost expressions written in the recipe over the components:
  `cost::sum` with weighted terms (`component<C>() * w`, or
  `cost::weighted(component<C>(), w)`), `cost::in_order`,
  `cost::objectives`, `cost::hard_soft` and `cost::apply`, nested freely; their weights are
  configuration parameters, and a pipeline stage `until_feasible()` reads the
  hard components from a `cost::hard_soft` expression.

### Runners and solvers

- Runners: **First Improvement**, **Best Improvement**, **Hill Climbing**
  (random non-worsening moves, stopping after a number of idle iterations),
  **Late Acceptance Hill Climbing**, **Great Deluge** and **Simulated
  Annealing**, with the temperature policies `Classic`, `FixedLength`,
  `Cutoff`, `Hybrid`, `FixedTemperature`, `TimeBased` (cooling spread over a
  running time) and `Reheating<Descent>`, reheating any of them, each able
  to estimate its initial
  temperature from sampled moves, and Metropolis acceptance; **Tabu Search**
  and **First Improvement Tabu Search**, with Glover's aspiration plus and
  elite candidate list strategies, pluggable tabu lists (fixed
  length, random tenure, Taillard's cyclic tenures, reactive with escape,
  frequency-based, on cost values, idle-driven dynamic length, fluctuation of
  the objective) and aspiration criteria (by objective, none); **Pareto Late
  Acceptance Hill Climbing** for multi-objective problems. Writing a new
  runner means writing one `run(...)` function against
  `easylocal::search_run`, which owns counters, evaluation budget,
  cancellation, progress and trace events, with `best_so_far` for those that
  return the best solution they visited.
- Limits on a count (`max_evaluations`, `max_iterations`, TimeBased's
  `accepted_per_temperature`) are `easylocal::limit` values: a number or
  `easylocal::unlimited`, written `unlimited` in configuration files, on the
  command line and in the TextUI. They are unlimited by default, and 0 is a
  limit of zero.
- Every runner is cancellable through a `std::stop_token`, reports progress,
  can stop at a target cost (`stop_at(cost)`), after a time limit
  (`timeout(5s)`, or `timeout(2.5)` seconds) or an evaluation budget
  (`max_evaluations(n)`, which tightens a runner's own) and returns a
  `search_result` with its `termination_reason` (`to_string` gives a readable
  name). A solve's limits bound all its runs together, and a pipeline stage
  may have its own (`& timeout(d)`, `& max_evaluations(n)`, `<name>.timeout`,
  `<name>.max_evaluations`); `cli::run --timeout` and `--max_evaluations`, a
  REST run's `"timeout"` and `"max_evaluations"` and the TextUI's *Stop after*
  fields (seconds, evaluations) set them.
- Solvers: **LocalSearch**, **MultiStart** and **Pipeline**, with pluggable
  initialization. A pipeline (`stage(name, runner) | ...`, or
  `pipeline(stages...)`, or `.then(stage)`) runs runners with their own
  recipes in sequence over the same solution; a stage, with `&` or a method,
  may stop at a target (`target`, or `until_feasible()` on the hard cost until
  it is zero) and be repeated (`attempts`), and the result reports every
  stage. An app registers pipelines beside its runners
  (`pipeline("name", stages...)`): the command line, the TextUI and the REST
  service run them by name from the current solution and configure them under
  `runners.<name>.<stage>.*`. `two_stage(first,
  second)` is the pipeline of the hard/soft model: hard constraints first,
  until the hard cost is zero, then the full cost. Solvers take the same run options as runners —
  cancellation, tracer, target — and report the effort of all their runs.
- Runners and solvers are built with `make_runner<Algorithm>(...)` /
  `make_solver<Solver>(...)`, where the algorithm class is its own key, and
  composed either with pipes or with the equivalent `with_*` calls.

### Apps and tools

- An **app** names a composed problem and its runners
  (`app("tsp") | solution_manager | neighborhood | runner<A>("name", params)`),
  and every run binds fresh services over a shared immutable Input. Runners
  are run by name with `app.run("name", input, solution, rng, options...)`, the
  single entry point of the TextUI and the REST service; `app.bind(input)`
  keeps the services of one Input.
- `check(app)` and `<easylocal/testing.hpp>` verify the components' contracts:
  solution semantics, cost/delta consistency and neighborhood properties.
- A **Session**, `Session{app, input, seed}`, runs an app on one Input: a
  current solution changed by hand, move by move, or by a runner chosen by
  name, and checks of its neighborhood. The interactive tester,
  `tui::run(app, options)`, is a view on a Session, and every REST run has a
  Session of its own. Its pages scroll to the focused control in a terminal
  too small for them, and the result of a run is also in its status line.
  Tools give stochastic runners an RNG they own, seeded reproducibly.
- **Command-line programs**: `cli::run(app, argc, argv)`
  (`<easylocal/app/cli.hpp>`) runs an app from the command line: the instance,
  the seed, the runner by name, the starting solution, the output file and a
  target cost, with the app's parameters and the program's own, then prints
  the cost, the running time, the iterations, evaluations and termination
  when the runner reports them, and the solution; `options.defaults` gives
  the switches' values when the command line does not. The example programs
  of `examples/` are written with it.
- **Launcher**: `tui::run_launcher(options, apps...)` opens several apps of
  the same problem, for example one per neighborhood. The launcher owns the
  Input and the current solution: its *Input and solution* entry loads and
  saves them, each app opens on them, and what an app leaves is shared with
  the others. Apps with different SolutionManager recipes are rejected at
  compile time.
- **Tuning with irace**: `cli::run --tuning.irace=DIR` writes an irace
  scenario from the program's parameters (`<easylocal/app/tuning.hpp>`): the
  parameters with a domain, or a range given in `cli::options::tuning`, with a
  categorical runner and conditions when there are several, the fields'
  conditions as irace conditions and the requirements as forbidden
  combinations; the others commented out with a range to start from; the values given on the command
  line as the starting point of every run; a target runner, the instances and
  the scenario. The files are a stub that is never overwritten, but
  `configurations.txt`, which follows `parameters.txt` as edited.
  `--tuning.print=cost` prints only the cost as one number: a problem's
  `scalar_cost(input, cost)`, else `cost::scalar` (`hard * W + soft` for a
  hierarchical cost, with `--tuning.hard_weight`). A smoke test runs irace on
  the TSP example when R and irace are installed.
- **Cost reports**: a cost component may have a `name()` and a
  `describe(solution)` that explains its value, as EasyLocal 3's
  `PrintViolations` did; `Session::cost_report()`, `cli::run --report` and the
  TextUI's solution window show each component's value with them.
- **Reading and writing**: optional hooks on the problem's types read an
  Input, read and write a Solution and describe a value; `load_input`,
  `load_solution`, `save_solution` and `describe` (`<easylocal/app/io.hpp>`)
  use them in any program, as the Session and the TextUI do.
- **Configuration**: typed runner and solver parameters with validation,
  collected in parameter sets with dotted paths (blocks may nest groups),
  applied all or none from the command line, configuration files and TOML
  (optional `ConfigTOML` component). Components give relative paths; the
  program chooses the prefixes. A runner holds its algorithm's parameters and
  builds the algorithm when it is bound, so an algorithm whose parameters have
  a schema is configurable with no other member; solvers give theirs too.
  An app gives the parameters of its cost, neighborhood and runners
  (`cost.*`, `neighborhood.*`, `runners.<name>.*`), and a Session applies them.
  A field may declare its **domain** (`config::range(0.0, 1.0).open()`,
  `.log()`, `config::one_of("a", "b")`) and when it matters
  (`.only_if(config::value<"calibration_samples"> > 0)`), and a schema the
  **requirements** between its fields (`config::require(config::value<"a"> <
  config::value<"b">, "message")`, also on fields of nested groups); they are
  checked when the parameters are validated and by `config::check_schema` in a
  block's `validate()`. A parameter set lists the kind, the domain and the
  condition of each parameter, and the requirements. Every parameter declares
  its domain, a range with no upper bound (`config::range(1,
  easylocal::unlimited)`) or any value (`easylocal::unlimited`) included:
  `check(app, ...)` fails for one without, and so do the library's tests for
  the built-in blocks, whose validate() checks their schema. Simulated
  Annealing and Great Deluge declare their conditions and requirements.
  Costs are read as text (`cost::from_text`, or a problem's `read_cost`) and
  written back (`cost::to_text`) in one form, `[hard, soft]` for a
  hierarchical cost: for targets on the command line (`RunParameters`,
  `--run.target`), in the TextUI and in REST requests, and wherever the
  command line and the TextUI show a cost.
- **Tracing**: core search events to JSONL, self-describing binary (ELTR: the
  header gives the run's metadata, the cost layout and every event's fields)
  or in-memory recorders, with no overhead when unused, including the
  solutions visited, by their hash, for search trajectory and local optima
  networks (left out at compile time with `trace::without`), and tabu
  search's aspirations, escapes and tenure changes; `run_finished` says why
  the run ended (`"termination": "time limit reached"`); an in-memory trace is
  replayed to any recorder after the run (`replay`, `write_jsonl`);
  `scripts/eltr.py` decodes binary traces to JSONL, a summary or a search
  trajectory network; leveled logging.

### Optional components

- `EasyLocal::TUI` (FTXUI): an interactive terminal tester to load inputs,
  create, check and inspect solutions, explore moves and run runners in the
  background with live progress, stop and a configurable seed; the parameters
  of a runner, and of the problem, are edited and checked in a window, and a
  target cost stops a run.
- `EasyLocal::REST` (Crow, standalone Asio): serves an app over HTTP, with
  asynchronous runs, status and progress, cancellation, partial solutions, a
  target cost that stops a run (a lower bound, for example), per-run
  parameters (nested JSON or dotted paths, listed by `GET /parameters` and
  repeated in the run's status) and a bounded execution pool.
- `EasyLocal::ConfigTOML` (toml++).
- Each component is opt-in at configure time, uses an installed dependency
  when available or fetches a pinned one on request, and is loaded by
  `find_package(EasyLocal COMPONENTS ...)` only when asked for. Core depends
  only on the standard library.

### Documentation and examples

- A [quick start](docs/quick-start.md), a 16-chapter
  [tutorial](docs/tutorial/README.md) built around a TSP, and
  [reference pages](docs/reference/README.md) per component (contract, API,
  design choices). Their code is compiled and run as tests.
- A [generated API reference](https://iolab-uniud.github.io/easylocal/api/)
  of every public class, function and concept, built by MrDocs from the `///`
  comments of the headers; every public declaration has its comment, and the
  documentation build fails on one without. Its pages have the look of the
  documentation site: its header and tabs, fonts and colors, and the light or
  dark mode chosen there.
- Examples: TSP, Assignment, Exam Timetabling and PFSP (Tabu Search), each
  with its TUI or REST front-end where useful.
- [API stability](docs/stability.md): what is stable, extensible, experimental
  or internal in 4.x.
- [Benchmarks](docs/benchmarks.md) against EasyLocal 3 (`easylocal-legacy`
  v3.3.1) on the examples, ported to both frameworks, and of the infrastructure:
  every release starts them in
  [easylocal-benchmarks](https://github.com/iolab-uniud/easylocal-benchmarks),
  which measures both frameworks on the same machine.

### Platforms

- C++23 with GCC 15 and 16, Clang 22 and 23 (libstdc++ and libc++) on Linux, and
  AppleClang (tested with Xcode 26.6 and 27) and GCC 16 on macOS ARM64,
  clang-cl with the Microsoft STL on Windows; CMake 3.25+ and Ninja. CI
  covers the matrix, every optional component with
  installed and fetched dependencies, and reports test coverage.

[Unreleased]: https://github.com/iolab-uniud/easylocal/compare/v4.0.0-alpha.1...HEAD
[4.0.0-alpha.1]: https://github.com/iolab-uniud/easylocal/releases/tag/v4.0.0-alpha.1
