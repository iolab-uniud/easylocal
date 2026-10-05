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

### Parameters

- **Breaking:** one rule makes a class configurable: its `parameters_type` is
  a parameter block, and it is constructed from it, at the path of its role
  (a runner's algorithm, a neighborhood explorer). A class that declares its
  parameters another way, with `parameters()` returning a block or with
  `configuration()`, and no such `parameters_type`, no longer compiles, with
  a message saying what to write: an algorithm's `configuration()` was
  silently put under `search.*`. The concept `parameterized_neighborhood` is
  removed, and `config::configurable_endpoint`,
  `config::configurable_parameters_t` and `config::configuration_provider`
  move to `detail`; `parameter_set::add` still takes an object with
  `parameters()` and `configure()`.
- **Breaking:** cost components and `cost::apply` functions follow the rule:
  a component whose `parameters_type` is a parameter block is built from it,
  `component<C>(parameters, args...)`, and a function from
  `cost::apply<F>(parameters, children...)`; each is configured under its
  static `name()`, `cost.<name>.*` in a runner or an app, wherever it is in
  the expression. A function's `configuration()`, which `cost::apply` used to
  read, no longer compiles. The tutorial's hierarchical cost has its bound of
  8 as a parameter, `cost.excess.bound` (chapter 9).
- A SolutionManager follows the rule too: one whose `parameters_type` is a
  parameter block is constructed from the Input and it,
  `solution_manager<SM>(parameters, args...)`, and its parameters are at the
  root of a runner and of an app, `solution_manager.*`, beside `cost.*`;
  the TextUI's problem parameters (`P`) show them.
- **Breaking:** `easylocal::unlimited` is a tag of its own type,
  `unlimited_t`, which converts to the unlimited `limit`, rather than a
  `limit`. `config::field` and `config::range` take the tag: a count where a
  domain goes (`field<...>("...", 5)`) and a range of two types
  (`range(0.0, 1)`, `range(1, std::size_t{1000})`), which compiled and threw
  `std::invalid_argument`, or picked the unlimited overload, no longer
  compile. `limit x = unlimited;` and `limit{unlimited}` are unchanged.
- `check(app, ...)` reports, as `runner parameters`, a registered runner whose
  `parameters_type` is not a parameter block (and not empty): the app runs it,
  but no frontend can change its parameters. The tutorial's `RandomDescent`
  declares a schema for its `max_evaluations`, configurable as
  `search.max_evaluations` in a runner and `runners.<name>.max_evaluations`
  in an app.

### Apps and tools

- **Breaking:** the types of an app are public and documented: `App`, what
  `app()` and each `|` return (formerly `detail::app_builder`), `BoundApp`,
  what `App::bind` returns (formerly `detail::bound_app`), and `BoundRunner`,
  what `Runner::bind` returns (formerly `detail::bound_runner`). The name of a
  registration is the key of their members: `runner_names()` lists the
  runners and pipelines, `runner_parameters<A>("name")` replaces
  `runner_config<A>([name])`, `make_runner<A>("name")` replaces
  `make_runner<A>([name])`, and `BoundApp::run("name", solution, rng,
  options...)` runs a runner on services built once. The members keyed by
  algorithm type or by registration index (`run<A>`, `run_at<I>`,
  `run_at_with_rng<I>`, `runner<A>()`, `runner_at<I>()`, `runner_name<A>()`,
  `make_solver<Solver, A>`, `for_each_registration_name`,
  `for_each_runner_registration`) are no longer public: an algorithm
  registered twice (`"sa-fast"`, `"sa-slow"`) had no single runner. A solver
  is `make_solver<Solver>(application.make_runner<A>("name"))`.
- A runner registered in an app may bring its own neighborhood, the third
  argument of `runner` (`runner<SA>("sa", {...}, neighborhood<Swap>() |
  delta<...>())`, or of `with_runner`): it is built over the app's
  SolutionManager next to the app's neighborhood, its parameters are under
  `runners.<name>.neighborhood.*`, the registration is checked at compile
  time (the neighborhood explores the app's Solution, the algorithm runs on
  it), and `check(app, ...)` checks it. Runners on different neighborhoods
  no longer need two apps: the TSP example's `two_apps.cpp` becomes
  `two_neighborhoods.cpp`, the runners of one app.
- A pipeline registered in an app may have algorithm stages,
  `stage<FirstImprovement>("descent", {...}[, neighborhood])`, which run on
  the app's recipes: its SolutionManager and cost (so the app's `cost.*`
  apply to every stage, instead of a copy per stage), and the app's
  neighborhood or their own. Their parameters are
  `runners.<pipeline>.<stage>.search.*`, the stage's own `attempts`,
  `timeout` and `max_evaluations`, and `.neighborhood.*` for their own
  neighborhood; stages of runners stay, with their own recipes.
- **Breaking:** a `BoundApp` holds the Input, its services and a copy of the
  registrations, and builds the algorithm of each run from the registration's
  parameters: it no longer builds an instance of every registered algorithm
  at bind (which the Session built and never ran), and runs on the same
  bound app no longer share an algorithm's state.
- **Breaking:** `RunParameters` is the block of a run's limits: `target`,
  `timeout` (seconds, as text) and `max_evaluations`, with
  `options<Cost>(input[, base])`, which gives their run options. `cli::run`
  reads its `--target`, `--timeout` and `--max_evaluations` through it
  (`cli::parameters::run_parameters()`), and `cli::parameters::timeout_seconds()`
  moves to `RunParameters`.
- **Breaking:** `Session::run` gives each run a generator of its own, seeded
  with one draw of the session's RNG, as the TextUI already did: the same seed
  and the same commands now give the same runs in `Session`, `cli::run`, the
  TextUI and REST, where the TextUI's runs differed from the others'. Runs of
  stochastic runners differ from those of alpha.1 with the same seed.
  `docs/stability.md` lists the conditions of reproducibility (no time limit,
  the same commands in order, a solver's stream across `solve()` calls, REST
  seeds, the same standard library).

- **Breaking:** the cost semantics are defined with one hook: the function of
  a root `cost::apply` defines `compare(a, b)`, returning a
  `std::partial_ordering`, from which `better`, `equivalent` and
  `better_or_equivalent` all follow. A function that defined only some of the
  three mixed them silently with the defaults (a maximizing `better()` with the
  default `<=` made Hill Climbing walk downhill while it recorded the bests
  uphill); its `better`, `equivalent` or `better_or_equivalent` members now
  fail to compile with a message, as does a `compare` below the root of the
  expression. The sign of `cost::delta` must agree with `better()` for the
  algorithms that read it (Simulated Annealing, Great Deluge, the aspiration
  levels of Tabu Search): Simulated Annealing and Great Deluge reject a
  `compare` that provably finds a larger cost better.
### Checking tools

- **Breaking:** the contract checks compare floating-point values within a
  tolerance: a cost updated by deltas drifts from its full evaluation in the
  last bits, and the checks reported false delta mismatches on a TSP with
  decimal distances. The new `cost::tolerance{.relative, .absolute}` (1e-9
  each by default), with `cost::approximately_equal` and
  `cost::approximate_compare`, compares numbers within it, integers exactly
  and structured costs level by level. `testing::approximately` (the same
  type) is the default comparison of `testing::fixture`, instead of
  `std::equal_to<>`, with the new `check_options::tolerance`;
  `check(app, input[, solution], options)` takes the options (their seed and
  tolerance), and the Session's `check_neighborhood_costs(tolerance)` and
  `move_evaluation_matches_full(tolerance)` accept a cost agreeing by
  `equivalent()` or within it. `{0, 0}` compares exactly.
- `cost::approximately(expression, {.relative, .absolute})`, a root node of
  the cost expression whose costs the search compares within a tolerance:
  equal within it is equivalent, better is better by more than it. It keeps a
  `cost::hard_soft` below it visible, so `until_feasible()` evaluates only the
  hard components and compares the hard costs within the same tolerance; the
  tolerance is configurable as `cost.tolerance.relative` and
  `cost.tolerance.absolute`.
- The move checks (`check_neighborhood`, `check_delta_evaluator` and
  `check(app)`) start from the fixture's Solution and from random solutions,
  the new `check_options::random_solutions` (default 4) drawn with
  `random_solution()` and walked by `check_options::walk_length` (default 8)
  random moves: a delta that mixes up positions and cities, right on the
  identity tour, passed every check. Beyond `max_enumerated_moves` they visit
  a uniform sample of the enumerated moves instead of the first ones, and
  `check(app)` takes its limits from the options instead of 128 and 16 moves.
  The tutorial's fixture is no longer the identity tour.
- A failed check names the move (with its `describe` hook or `operator<<`),
  the solution it starts from and the values that disagree, and
  `print_report` groups the failures of a check: their number and the first
  three, instead of one line per failure. A hook that throws fails its check,
  with what the exception says, instead of leaving the checks without a
  report.
- `check_neighborhood` reports a neighborhood whose moves all leave the
  Solution unchanged, as a `make_move` taking it by value does, when
  solutions compare; with moves that compare, it reports random moves the
  enumeration does not contain, a `random_move` that finds no move while the
  neighborhood has valid ones, and one that draws from another source than
  its generator. `check(app)` reports the same about its neighborhoods, and
  random solutions that are not valid.
- **Breaking:** `check(app)`'s `app_check_coverage` and `coverage()` are
  `app_check_composition` and `composition()`, its `neighborhood_graphs`
  `neighborhoods`, and `print_report` writes `composition:`: they count what
  the app composes, which both forms of `check` now fill. The form without a
  solution binds the app once, and checks the runners once. `check(app)`
  compares the repeated evaluation with the cost's equivalence, and reports
  each invalid parameter block of the app with its path.
- `testing::run_checks` takes the report of `check(app)` with the component
  reports; `check_cost_component` no longer counts a check that cannot fail,
  and values its comparison cannot compare are a compile error.
- `Session::check_move_independence` compares solutions as the search does,
  with `solutions_equal` (the SolutionManager's `equal`, or `==`), and is
  available whenever solutions compare so; with a solution hash it compares a
  solution only with those of the same hash, in linear time instead of
  quadratic (minutes on a 400-city 2-opt neighborhood).
- `testing::check_report::check(condition, name, message)` also takes the
  message as a callable, called only when the check fails: the move checks
  build no text for the moves that pass.

### Runners and solvers

- **Breaking:** the Pareto archive keeps one point per non-dominated cost by
  default, the first reached: it kept every distinct solution of equal cost,
  an unbounded front that made each offer scan a whole plateau (Hill Climbing
  on a plateau ran thousands of times slower). `run_options::keep_front(
  {.keep_equivalent = true, .max_front_size = n})`, the new
  `pareto_archive_parameters`, keeps them all, up to `n` points; the solvers
  merge the fronts of their runs with the same parameters.
  `pareto_archive::offer` takes the relations to compare with, and `first()`
  returns the first point of `sorted()` without sorting.
- The Pareto archive compares costs with the run's `better()` and
  `equivalent()` instead of the cost's `<` and `==`: with a root `compare`
  that maximizes, Pareto Late Acceptance returned the worst point of its
  front.
- `named_run_result` has the `front` of a run with a `cost::pareto` cost, so
  `app.run("name", ...)` no longer drops it; the TextUI does not show it yet.
- The front of a run with a `cost::pareto` cost reaches the tools: the
  Session keeps the front of its last run (`last_run_front()`, a `front_type`
  of `pareto_point{solution, cost}`, empty when `last_run_effort()` is);
  `cli::run` writes `front <n>` after the solution, then each point as
  `point <i> cost <cost>` followed by its solution, or with `--output
  best.txt` saves the solutions to `best.1.txt`, `best.2.txt`, ...; the REST
  solution resource has a `front` array of `{cost, solution}`, encoded as the
  run's cost and solution are.
  `app.run("name", ...)` no longer drops it; the Session, `cli::run`, REST and
  the TextUI do not show it yet.
- **Breaking:** the solvers are configured one way: `LocalSearch{runner}`,
  `MultiStart{runner, {.starts = n}}` and a pipeline all take `.seed(n)` and
  `.initialization(tag)`, which return the solver (itself on an lvalue, by
  value on a temporary). `LocalSearchConfig` and `MultiStartConfig` are
  removed, and so are the runtime `initialization::Mode`, `supports(Mode)`
  and `initialization_mode()`: an initialization is a tag, checked at compile
  time. A custom RNG is the solver's last constructor argument
  (`MultiStart{runner, parameters, RNG{seed}}`).
- **Breaking:** every solver starts by default from `initialization::automatic`,
  a random solution when the SolutionManager builds one, else its initial
  solution, with seed 0: LocalSearch and MultiStart required
  `random_solution` unless told otherwise, while a pipeline adapted.
- **Breaking:** MultiStart and the attempts of a pipeline stage run in one
  loop, so they stop, count their budget and merge their fronts the same way;
  MultiStart's termination is now, like a stage's, the last start's when no
  start, cancellation or budget ended it early (it was always `completed`).
- A pipeline stage may restart its attempts: `stage(...) & attempts(10) &
  restart(initialization::random)` starts the attempts after the first from a
  new solution rather than from the one the stage received, so a pipeline
  registered in an app, which runs from the current solution, can
  multi-start.
- A complete `Runner` and `LocalSearch` name their `cost_type`, the Cost of a
  recorder for their runs; a const `Runner` of a parameterized algorithm that
  cannot be copied binds, since it holds only the parameters.
  **Breaking:** `run_control::stop_possible()`, which nothing used, is
  removed.
- Great Deluge, the aspiration plus and elite candidate Tabu Searches and
  Pareto Late Acceptance reject a cost they cannot use with one message that
  names what they need (an arithmetic cost, a `cost::pareto` cost), as
  Simulated Annealing does, whose message now names `cost::delta` rather than
  a difference of costs; the rejected run no longer adds a second error.
- The TimeBased annealing schedule reads its clock as a run checks its time
  limit, at an interval of proposals that adapts to about a millisecond
  between readings, and at each early cooling, instead of at every proposal.
- A move whose cost needs the candidate solution (a component without a
  delta) is evaluated on a scratch solution reused from one move to the next,
  instead of a new copy per move; a candidate keeps its move, and its commit
  swaps the scratch solution in when it is the last one evaluated, or makes
  the move again otherwise (Best Improvement, Tabu Search). Debug builds no
  longer check the whole current solution at every evaluation.
- `easylocal::moves()` and a run's `moves()` keep the reference an explorer's
  `moves()` returns to moves it keeps, which they copied at every call, and
  `random_move()` moves the move it gets instead of copying it.
- **Breaking:** `tabu_candidate<Run, WithCost>` has `cost()` and
  `equivalent_cost()` only `WithCost`, the candidate a list whose state
  declares `needs_cost` receives: a custom list that read the cost without
  declaring it dereferenced a null pointer in a Release build. When every
  move is tabu, Tabu Search applies the least tabu move with the evaluation
  its scan made, instead of evaluating (and tracing) it again.
- **Breaking:** a pipeline checks the names of its stages when it is built,
  once, instead of at every `solve()` and `configuration()`, which no longer
  throw. A stage rejects run options with a control (`stage & with(control)`
  dropped it: the control goes to `solve()`), with `std::invalid_argument`,
  and a floating-point target for an integer cost (`target(0.5)` was
  truncated), at compile time. `pipeline_stage::limits()`, which returned a
  detail type, is private.

### Added

- `trace::event::run_context`, a core event without a cost (ELTR tag 12): the
  solvers emit it before each run, with the pipeline stage's name and index
  and the attempt (or MultiStart's start), so the runs of a solve can be told
  apart in a trace, the stages on the hard cost included. The memory and JSONL
  recorders keep it, and `eltr.py --format summary` gives each run its stage
  and attempt.
- **Breaking:** a REST run starts as `cli::run` does: the new `start` field
  chooses `"random"` (drawn from the run's seed) or `"initial"`, and without
  it or an `initial_solution` a problem with random solutions starts from a
  random one, where every run started from `initial_solution()`. Run
  resources report the `start`.
- The REST examples listen on `127.0.0.1` only, not on every interface, and
  exit with status 1 when they cannot listen on their port, where they exited
  with 0.
- `rest::blueprint_options::max_timeout` bounds the time of the runs of a REST
  service: a request with a longer `timeout` is rejected with `422`, and a run
  without one gets that limit.
- REST: the status of a run that ended gives its `termination`
  (`target_reached`, `cancelled`, ...), its `cost` and its final counts. A run
  is `cancelled` when its runner says so, no longer when a cancellation came
  after it had ended on its own terms.
- TOML: a parse error says where it is: `config::toml_config_diagnostic` has
  the `line` and the `column` of the error, and its message starts with
  `file:line:column:`. An array of arrays, `[[1, 2], [3]]`, sets a list of
  lists, as on the command line, and an unsupported value says what it is (a
  date or time, or an array holding strings) instead of naming the "textual
  override mapper".
- **Breaking:** REST: the `parameters` of a run accept arrays of arrays, and
  reject a string inside an array with `422`, as a TOML file does: `["a, b"]`
  was split at the comma into two elements. The tutorial's TOML program
  prints the number of values read and a parse error without an empty path.
- **Breaking:** REST: the solution of a run that ended without one (cancelled
  while queued) is `409 no_solution`, where it was `result_not_ready` as for a
  run still active. The REST reference no longer calls the status shape
  stable, and its result example lists every field.
- The REST blueprint needs no codec for a problem with text hooks:
  `rest::blueprint(prefix, app, options)` serves it with `rest::text_codec`,
  the Input and the solutions as JSON strings in the text of `read_input`,
  `read_solution` and `write_solution`, the costs as JSON numbers (or
  `cost::to_text`). A codec may leave out any of its members, and that value
  goes through the hook. The tutorial's service shows it first (`/tsp-text`).
- `eltr.py --format summary` marks `"unfinished": true` a run without
  `run_finished`, which an exception ended or a truncated trace cut. The
  solvers reference says how an exception thrown mid-run propagates.
- `cli::run` records the trace of the run with `--trace <file>`: JSON Lines
  for a `.jsonl` name, ELTR otherwise, with timestamps and with the program and
  its parameters as metadata. A pipeline stage on another cost than the app's
  (`until_feasible()`) records only its `run_context`.
- Timestamps in traces, as a recorder option: with `timestamps` in
  `binary_buffer_options` or `jsonl_options`, each core event ends with
  `elapsed_ns`, the nanoseconds since the recorder was constructed (a `u64`
  field of the ELTR schemas, a field of the JSONL line), for anytime and
  time-to-target analyses. Without it, no clock is read.
- `trace::without<Types...>(tracer)`, with the new `trace::without_event_types`,
  hides events by their type: the events without a cost
  (`neighborhood_selection`, `tabu_escape`, `tabu_tenure_changed`,
  `run_context`) and application events, which the template form
  (`without<event::solution_visited>`) could not name.
- `eltr.py --format stn` attributes the network to the runs of the trace:
  each node and edge lists the runs that visit it (their indices, as the
  summary lists them), and `starts` and `ends` give the first and the last
  solution each run visits.

### Platforms

- AppleClang needs Xcode 26 or later and a macOS deployment target of 26.0 or
  later, where its libc++ has the floating-point `std::from_chars`: the
  README, the quick start and the stability page say so, and CMake (the
  project and `find_package(EasyLocal)`) stops with a message that says how to
  fix it, instead of failing on every number parsed.
- The headers compile after `windows.h` without `NOMINMAX`: they call
  `(std::max)(a, b)` and `(std::numeric_limits<T>::max)()`, which its `min`
  and `max` macros do not expand.
- `long double` costs and parameters compile with libc++, which has no
  `std::from_chars` for them: they are read through `double` where the two
  types are the same (Apple's arm64), with a stream in the classic locale
  elsewhere.
- `EASYLOCAL_SANITIZERS` (such as `address;undefined`) builds EasyLocal's own
  tests and examples with those sanitizers, a report stopping the program;
  the `asan` preset uses it, and the full CI runs it with GCC 16.
- Through `add_subdirectory`, EasyLocal no longer installs its headers and
  package into the consumer's prefix: `EASYLOCAL_INSTALL`, on by default only
  when EasyLocal is the top-level project, enables its install rules. The
  installed package version file is architecture-independent unless it
  bundles the compiled FTXUI of the TUI.

### Documentation and examples

- The tutorial no longer takes the launcher of chapter 12 from `examples/tsp`:
  `examples/tutorial/launcher_main.cpp` opens a 2-opt and a swap app of the
  tutorial's own TSP. `AGENTS.md` says what each example directory is.
- The TSP example's TextUI saves its solution (`W`) to a copy of
  `instances/small.sol` in the build directory, no longer over the
  checked-in file.
- The API reference lists `Session` and `rest::app_blueprint` once each:
  constrained by requires-clauses, they appeared twice.
- The REST examples check what clients send: the assignment codec rejects a
  fractional or quoted quantity (`2.5` was read as 2, `"2"` as 2), any error
  of a textual instance is a `422` (a negative count in its header was a
  `500`), and the instance reader allocates only the values it reads. The
  tutorial's service rejects a port outside 1 to 65535 with status 2, where
  `70000` wrapped around and a word ended it with an uncaught exception.
- The assignment README starts its TextUI walkthrough from the initial
  solution and limits its example run to 3 evaluations (25 limited nothing);
  the REST chapter's target, 26, is reachable, and its test checks the cost
  26 exactly.
- The assignment example writes its hooks in one style, as members: its
  instance is read by `AssignmentInstance::read`, and `instance_io.hpp`, with
  the free `read_input`, is gone.
- The tutorial's pipeline example registers its stages in an app as
  algorithm stages (`stage<A>(...)`, one on its own neighborhood, the first
  restarted from random tours), and chapter 8 and the solvers reference
  describe them.
- The API reference has a page for each overload of a function
  (`make_runner`, `check`, `timeout`, the `|` operators...), which MrDocs
  dropped when it grouped them, and comments on the members of `Runner`, on
  the special members and on the trait specializations. Its build fails on a
  public declaration without a comment, read from the XML that MrDocs writes
  (class template specializations included), and on a link to a missing
  page; an adapter enabled in the build is always in it.

### Changed

- The stability levels no longer contradict each other (`docs/stability.md`):
  the Stable level excludes what the Experimental one lists, which now names
  the adapters' headers, tracing beyond the tracer protocol, `tuning_range`
  and `cli::options::tuning`, logging and the `with_*` spellings of the
  composition.
- The logging API (`<easylocal/utils/logging.hpp>`) is Experimental, since the
  library emits no records yet; the documentation no longer presents it as
  framework diagnostics. `stderr_sink` writes a record in one write (up to 1
  KiB), and `set_sink` is no longer `[[nodiscard]]`.
- **Breaking:** `trace::jsonl_recorder` starts with a header line,
  `{"event":"trace","version":1,"metadata":{...}}`, the first line of
  `eltr.py`'s JSONL output without the cost layout; the new
  `trace::jsonl_options` gives its metadata, to the recorder and to
  `write_jsonl`. A reader of the events skips the line with event `trace`.
- **Breaking:** the solution hashes of `solution_visited` are strings of 16
  hexadecimal digits in JSONL (`jsonl_recorder`) and in the output of
  `eltr.py`, the STN's nodes and edges included: as JSON numbers, JavaScript
  and jq rounded them to doubles. ELTR keeps them as `u64`.
- **Breaking:** one naming rule for counts and budgets. A `max_<count>` of a
  run is an `easylocal::limit` cap that ends it: `max_idle_iterations` of Hill
  Climbing, Late Acceptance and the tabu searches is a limit (`unlimited` can
  be written in a configuration), and `FixedTemperature`'s `accepted_ratio` is
  `max_accepted`, unlimited by default. The size of a schedule is
  `allowed_<x>`: the `max_iterations` of `FixedLength`, `Cutoff`, `Hybrid` and
  `FixedTemperature` is `allowed_iterations`
  (`runners.sa.temperature.allowed_iterations`), Reheating's `max_reheats` is
  `allowed_reheats`, and `Hybrid::sample_limit()` is
  `samples_per_temperature()`, as in `FixedLength`. Pareto Late Acceptance's
  `max_iterations`, the iterations before it may stop, is `min_iterations`.
  The evaluation budget of `run_options` is the field `evaluation_limit` (a
  limit, unlimited by default) instead of the optional `evaluation_budget`; it
  is set with `max_evaluations(n)`, as the time limit `time_limit` with
  `timeout(...)`, and `search_run::no_evaluation_limit` is gone:
  `easylocal::unlimited`.
- The briefs of the `LimDynamic`, `Foo` and `RandomFoo` tabu lists and the
  runners reference expand their names (limited dynamic tenure, Fluctuation Of
  the Objective) and give their sources.
- **Breaking:** the four tabu searches share one parameter block,
  `TabuSearchParameters<ListParameters, CandidateParameters>`: the parameters
  of the candidate strategy are the group `candidates`
  (`runners::candidates::FirstImprovementParameters`,
  `AspirationPlusParameters`, `EliteListParameters`; none for `TabuSearch`),
  `{.tabu_list = {...}, .candidates = {.min_moves = 10}}` in the code and
  `search.candidates.min_moves` in a configuration.
  `FirstImprovementTabuSearchParameters`, `AspirationPlusTabuSearchParameters`
  and `EliteCandidateTabuSearchParameters` are gone.
- **Breaking:** invalid parameters throw `std::invalid_argument` in every
  build, instead of an assert that Release builds skipped (a Simulated
  Annealing schedule then reached undefined behaviour, or never ended): the
  constructors of the runners, of the temperature schedules and of the tabu
  lists, `make_runner`, the neighborhood recipes, the binding of a runner or
  of an app, and the construction of a `Session` and of a REST blueprint. The
  message names the parameter, `runners.sa.temperature.cooling_rate: expected
  a value in (0, 1), got 2`. The constructors that validate are no longer
  `noexcept`. `config::require_valid(block)` and `config::require_valid(set)`
  give the same check to custom code, and `check(app, ...)` reports invalid
  app parameters as `app configuration` before binding.
- **Breaking:** `search_run::with_context(ctx)` is replaced by
  `run.with_evaluation(wrap)`, which keeps the run's context and swaps only its
  evaluation facility for `wrap(run.evaluation())`. A decorated context
  without `better_or_equivalent()` or a SolutionManager silently dropped the
  target, the solution events and the archive identity: the assignment
  example's `slow-fi` ignored `--target`. `search_run` takes the facility as a
  third template parameter, and `run.evaluation()` returns the one it
  evaluates with.
- **Breaking:** a recorder observes the core events of its cost type, its new
  `cost_type`, and those without a cost, instead of declaring every event
  observed: the memory and JSONL recorders silently dropped an event of
  another cost type, and the binary recorders converted its cost to theirs.
  The memory and JSONL recorders no longer claim the application events,
  which are ELTR records only, and `memory_recorder::stored_as_is` is
  private. `trace::emit` rejects at compile time a tracer whose
  `observes<Event>` is true but that has no `emit()` taking the event, such
  as an `emit` declared for another cost type.
- **Breaking:** the JSONL recorder writes the library's structured costs:
  its default cost writer, `trace::default_json_cost_writer` (was
  `ostream_json_cost_writer`), writes a `cost::lexicographic` or a
  `cost::pareto` as the array of its levels and a `cost::hierarchical` as
  `{"hard": ..., "soft": ...}`, nested as the types are, the shape `eltr.py`
  decodes ELTR costs to; a JSONL trace of such a cost, and `cli::run`'s
  `--trace file.jsonl`, no longer fail to compile or stop with an error. It
  no longer falls back to `operator<<`, whose text need not be JSON: another
  cost type takes a cost writer of its own.
- **Breaking:** `solution_visited` has a `previous_hash`, the hash of the
  solution the move was applied to, 0 at the start of a run and for a
  solution no move reached (`evaluate_solution()`, such as a sample of
  Pareto Late Acceptance's history); it ends the event in ELTR and in JSONL.
  `eltr.py --format stn` draws an edge per move, from `previous_hash` to
  `hash`, instead of one per pair of consecutive visits, which was wrong for
  an algorithm that keeps several solutions. A run that traces visits hashes
  the solution before and after each move.

### Fixed

- The Metropolis criterion of Simulated Annealing computes in `double`, and
  `cost::delta` of a `cost::hierarchical` returns a `double` (a `long double`
  only when the soft delta is one): every worsening move did `long double`
  arithmetic, software on aarch64 Linux and x87 on x86-64.
- `trace::jsonl_recorder` writes its numbers in the classic locale: a stream
  imbued with a locale that groups digits wrote `"evaluations":1,234`, which
  is not JSON. The line keeps the stream's flags and precision only.
- A floating-point cost updated by deltas reaches its target: deltas that
  leave it a rounding error away (`0.1 + 0.2 - 0.1 - 0.2` is 2.8e-17) kept a
  run, and `until_feasible()` with its zero hard cost, from ever stopping. A
  committed cost that misses the target only within `cost::tolerance{}` is
  evaluated in full before the run decides, without counting in the budget.
- The aspiration level of `AspirationPlusTabuSearch` and the quality level of
  `EliteCandidateTabuSearch` are `b + (factor - 1) * |b|` for the best cost
  `b`, unchanged for a positive one: `factor * b` put them below a negative or
  zero best cost, so the elite list evaluated more moves than plain Tabu
  Search.
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
- `eltr.py` writes NaN and the infinities as `null`, as `jsonl_recorder` does,
  instead of `NaN` and `Infinity`, which are not JSON; a missing file or a
  string that is not UTF-8 is reported in one line, without a traceback.
- `neighborhood_union(...) | random_biases(...)` compiles for explorers that
  derive from no EasyLocal base, in a namespace that does not use `easylocal`:
  the operator is a hidden friend of the union's recipe, found by ADL.
- `solution_manager_base` and `neighborhood_explorer_base` no longer bind a
  temporary Input or SolutionManager, which dangled: their constructors from
  an rvalue are deleted.

- `config::check_schema`, and so the `validate()` of a parameter block, checks
  the block's nested groups: an invalid temperature schedule of Simulated
  Annealing or tabu list of Tabu Search is rejected by the runner's
  parameters, so `check(app)` reports it as "runner configuration" instead of
  aborting. A parameter set reports such a group once, under its own path.
- `config::one_of` holds numbers of different types as their common type:
  `one_of(1, 1.5, 2)` holds 1.5, where it narrowed it to 1.
- **Breaking:** the expressions of conditions and requirements compute
  numbers in `double`, as their irace export does: `7 / 2` is 3.5 (it was 3),
  `unlimited` is +infinity (`unlimited + 1` overflowed), and a minus applies
  after the conversion (`-count` wrapped an unsigned count). Their R text
  writes infinity and NaN as `Inf` and `NaN` and escapes the quotes and
  backslashes of a text; `text_with`'s second parameter is now `r_syntax`.
- The weights of a `cost::sum` must be finite: a configuration that sets one
  to NaN or infinity is rejected (a negative weight is still accepted).
- `cost::hierarchical` compares a branch that has only `<` and `==`, as its
  `delta` already accepted: its `operator<=>` was deleted for such a hard
  cost.
- The recipes construct their components with parentheses, as their
  `std::constructible_from` checks do: `neighborhood<X>(2)` for a `double`
  parameter failed with a narrowing error inside the library (also in
  `solution_manager`, `component`, `delta` and the `random_move` of a move
  built from another type).
- The TOML adapter reads each value by its TOML type: `true` read as the
  integer 1, so no boolean parameter could be set from TOML (nor an array of
  booleans), and a float such as `3.0` set an integer parameter.
- A configuration file (`--config`) that is a directory is an error, where it
  read as empty, and a UTF-8 byte order mark at its start is skipped instead
  of becoming part of the first path.

- `timeout(d)` with a floating-point `std::chrono` duration that is NaN throws
  `std::invalid_argument`, as `timeout(seconds)` does, instead of converting
  NaN to the clock's integer ticks (undefined behaviour).
- `with(control)` and `with(control, tracer)` with a temporary `run_control`,
  and a `search_run` built over a temporary context, no longer compile: each
  kept a reference that dangled once the expression ended.
- A run with a `cost::pareto` cost that reached its target returned the first
  point of its front, which need not meet the target, and reported
  `target_reached`: it returns a point of the front that meets it, and
  `best_so_far` keeps the first cost that meets the target even when it does
  not dominate the best one.
- The `Classic` and `TimeBased` annealing schedules ran one temperature level
  more than they count with decimal cooling rates (1 to 0.001 by 0.1 took four
  levels, as 0.1 * 0.1 * 0.1 is a little above 0.001): they end after the
  levels counted, and the count is robust to rounding.
- `Reheating` over a schedule with an iteration budget gave each reheat at
  least one iteration, so with fewer iterations left than `max_reheats` the
  descents spent more than the budget: the parameters require `max_reheats`
  to be at most the iterations the first descent leaves.
- The rules between the parameters of `RandomTenure`, `LimDynamic`,
  `RandomFoo`, `AspirationPlusTabuSearch` and `Reheating` (a minimum not above
  its maximum, a reheat temperature above the final one) were checked only by
  `validate()`, so `--tuning.irace` wrote no `[forbidden]` line for them and
  irace sampled configurations that the target runner rejected, which stopped
  the tuning: they are `config::require` requirements of the schemas.
- The `Foo`, `RandomFoo` and `Reactive` tabu lists forgot the moves older than
  the current tenure, so a growth by more than one did not reach the moves it
  covers, as the tenure applies to every move held: each list keeps the moves
  its next growth can reach.
- The `ObjectiveBased` tabu list compared costs with `==`, ignoring a custom
  `equivalent()` of the cost: it uses the cost semantics, through
  `tabu_candidate::equivalent_cost(other)`, and no longer requires `==` on
  the cost.
- A Tabu Search whose list cannot run (`Reactive` with `verify_equality`
  and no solution equality) throws before the run starts, instead of after
  `run_started`, which left a trace without `run_finished`.
- `RandomFoo` traced a start tenure of 1 that no move got: its tenure is 0
  until the first move draws it, and the first `tabu_tenure_changed` is that
  draw. `tabu_escape` is emitted after the escape's random moves, with the
  number applied, instead of before them with the number asked, which the
  run's limits could cut short.
- With a `cost::pareto` cost, Tabu Search counted an unordered candidate as a
  tie with the chosen one, so it could apply a move dominated by one it had
  seen: a tie is an equivalent cost (`run.equivalent`), as before for total
  orders. The cost reference says which runners accept a pareto cost. A run
  with a pareto cost on a problem without solution equality no longer warns
  about an unused lambda capture.

- `with_hard_cost()`, `until_feasible()` and `two_stage()` compile with
  co-located deltas (`delta<C>()`): every SolutionManager layer, the hard-cost
  projection included, reaches the cost components.
- With a `cost::pareto` cost, MultiStart and the attempts of a pipeline stage
  return the front merged from all their runs, not the first run's front.
- A pipeline stage's termination, and the solve's, say why the stage stopped
  (cancelled, time or budget out, target reached), not why its best attempt
  ended.
- After a cancellation, or once the solve's budget is spent, a pipeline skips
  the stages between the first and the last (0 attempts in their report)
  instead of binding and running each one past the budget.
- `Pipeline::seed()` and `Pipeline::initialization()` on a temporary return the
  pipeline by value, not a `Pipeline&&` that could dangle.
- `configuration()` does not compile on a temporary (a runner, an app, a
  pipeline, a recipe, a solver or a Session), whose parameter set would refer
  to an object already gone.
- `Session::run` throws `std::invalid_argument`, and changes nothing, when the
  current solution is not valid for the Input, and `cli::run` rejects such a
  `--solution` with `error: solution: ...` and status 2, instead of running
  deltas that may index out of bounds.
- `Session::last_run_effort()` is empty after a new Input and after a run that
  does not complete, instead of keeping an older run's effort.
- `cli::run` exits with status 1 and `error: unknown exception` when a run
  throws something other than a `std::exception`, instead of terminating.
- `save_solution` (and `Session::save_solution` to a file) report the errors
  of the final flush and close, and every write error names the file.
- `easylocal::describe` and `easylocal::write_solution` are function objects,
  which the lookup of a problem's own hooks does not find: they work for types
  with `easylocal` among their associated namespaces, which recursed into their
  own constraints.
- The names of an app's runners and pipelines are validated: non-empty,
  distinct, of letters, digits, `_` and `-`; `configuration()` and `bind()`
  throw `std::invalid_argument` otherwise, and `check(app, ...)` reports it
  (and the real cause of an invalid app configuration) instead of blaming the
  pipelines.
- `--tuning.irace` leaves `--solution` out of fixed.conf (a starting solution
  belongs to an instance) and writes path parameters there as absolute paths.
- `--tuning.hard_weight`, given when the irace stub is written, goes to
  fixed.conf: irace runs no longer fall back to the default weight.
- `--tuning.irace` fails with status 2 for a cost that is not one number (no
  `scalar_cost`, such as a Pareto cost), instead of writing a scenario whose
  every run fails.
- `--tuning.irace` reads numbers whatever the locale: under a decimal-comma
  locale (`it_IT`) `0.95` read as 0 in the suggested ranges and in
  configurations.txt.
- An `unlimited` limit given a finite tuning range starts at the upper bound
  of the range in configurations.txt, not at the lower one.
- The irace scenario keeps the precision of real bounds with more than 4
  decimals (`digits` in the `[global]` section of parameters.txt, at most 15),
  which irace rejected, and
  open integer bounds that are fractional are no longer off by one.
- Each parameter of an irace scenario gets an irace identifier (other
  characters than letters, digits, `.` and `_` turned into `_`, a numeric
  suffix on a collision, `runner` reserved), used in parameters.txt, its
  conditions and `[forbidden]` lines and configurations.txt, while its switch
  stays the path: a runner such as `slow-fi` gave a parameters.txt that irace
  rejected.

- REST: a request body whose arrays and objects nest deeper than 64 levels is
  rejected with `400 invalid_json` before it is parsed, instead of exhausting
  the stack of the parser's thread.
- REST: cancelling a queued run makes it `cancelled` at once: it leaves its
  place in the queue and can be deleted, instead of staying `queued` until a
  worker took it.
- REST: `GET /parameters` no longer reports every parameter as `read_only`
  (a run may change any of them), and lists the `kind`, `domain`, `active` and
  `condition` of each parameter.
- REST: when the execution pool cannot create one of its worker threads, its
  constructor throws instead of waiting forever for the workers already
  started.
- REST: a submission whose response cannot be built (a codec that fails to
  encode the target) no longer leaves a run that stays `queued` forever.
- REST: an arithmetic target outside the range of the cost type (`300` for a
  `signed char` cost) is rejected with `422` instead of wrapping, and so is an empty
  key among the `parameters`.
- REST: an `initial_solution` that is not valid for the Input is rejected
  with `422` when the run is submitted, instead of reaching the runner's
  deltas, which could read out of bounds.
- REST: `blueprint_options` with zero `workers` or a zero `queue_capacity` is
  rejected with `std::invalid_argument`, where it was raised to 1, and every
  option is checked before a worker thread starts.
- REST: a submission binds the app once on the HTTP thread, where it bound it
  twice (the run's session is configured before it gets the Input), and the
  target and the cost of a run are encoded once: status polls no longer wait
  for the codec.
- **Breaking:** TextUI: `tui::options::input_path` and `solution_path` are
  `std::filesystem::path`, as `path_base` is, and `exit_label` is gone: q
  quits a tester, and leads back to the list from a launcher's. The fields
  say their units and what 0 means (`max_render_chars` in bytes, 0 for no
  limit), and the tutorial's chapter 12 lists them.
- `Session::check_move_independence` and `check_random_move_distribution`
  (the Move page's `D` and `U`) compare a state or a drawn move only with
  those of the same cost when the cost is totally ordered: on a 400-city
  2-opt neighborhood they take 0.1 s and 0.6 s, where the independence check
  took 254 s. Their briefs give the complexity.
- TextUI: values are shown as the Session's cost report writes them, a cost
  in the syntax the target field reads even when it also has a `describe`
  hook; only composites of printable parts, such as tuples, are the
  tester's own.
- TextUI: each page's actions are defined once, and its buttons, its
  shortcut line, the help and the keys come from them: the labels agree
  (`I First improving` was "I Improve" in the shortcut line, `U Distribution`
  "U Dist", `L Load input` "L Load selected") and the help lists only the
  actions the problem offers. Esc no longer stops a run from the progress
  window, where it closes every other window: `X` stops it.
- TextUI: the tester computes what it shows of the session (validity, cost,
  the selected move's costs and delta check) once after each change, not at
  every frame: on the Move page each redraw ran about five full evaluations
  of the user's hooks, and the progress of a run redrew the screen with them.
- TextUI: a run posts its progress to the event loop at most every 50 ms,
  and never while the last event is pending, where it posted one every 64
  evaluations (about 900,000 a second with a fast delta), each redrawing the
  screen. The seed field, like the limits, accepts spaces around the number.
- TextUI: the Run page no longer shows an `X Stop running` button, which no
  one could press: a run's progress window covers the page, and its `X Stop`
  stops the run. The launcher's header comment no longer says that its apps
  load no files.
- TextUI: however its event loop ends, an exception included, the tester
  stops and joins a running search before the loop's screen is destroyed,
  which the search posts its progress to.
- TextUI: the result of a run says why the run ended, by the runner's own
  termination, and its evaluations ("(target reached, 1 evaluation)"),
  instead of comparing the cost with the target by `<`, which is wrong for a
  Pareto cost or a custom `better()`.
- TextUI: after a run that fails, finds no runner or leaves an invalid
  solution, the Last run box says so instead of showing the previous run.
- TextUI: the neighbor list counts the invalid moves apart, and its "... N
  more" counts only the valid moves not listed;
  `Session::neighborhood_preview` reports the invalid moves (`invalid`).
- TextUI: a move whose `make_move` leaves an invalid solution is reported
  ("INVALID solution"; Move and Run disabled), and `Session::apply_move` no
  longer asserts that the solution stays valid: it documents that a faulty
  `make_move` may break it.
- TextUI: the header and the Input viewer name the Input file loaded, not the
  file selected on the Input/Output page, and the launcher's testers name the
  file of the shared Input, which one of them may have loaded.
- TextUI: the launcher keeps one session per app, so leaving an app no
  longer discards its runner and problem parameters and its seed.
- `easylocal::generator`, the fallback of `std::generator` for libc++
  (AppleClang), no longer copies an `std::exception_ptr` at every resume nor
  moves each yielded value into an `std::optional`: the iterator refers to a
  yielded rvalue, as `std::generator` does. Best Improvement over a 2-opt
  `moves()` generator with an O(1) delta runs about 2.5 times faster
  (15 to 6 ns per evaluation).
- A run without a tracer builds no `move_evaluated`, `move_accepted` or
  `incumbent_updated` event, no route of a union's move (a `std::visit` per
  evaluation) and no copy of the cost before each move: Best Improvement over
  a union of two 2-opt explorers runs about 20% faster.

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
