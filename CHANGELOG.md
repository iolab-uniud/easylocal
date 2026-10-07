# Changelog

All notable changes to EasyLocal are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project adheres to [Semantic Versioning](https://semver.org/). Entries
are drafted from the Conventional Commits since the previous release and
reviewed by hand before tagging.

## [Unreleased]

## [4.0.0-alpha.3] — not yet released

### Parameters

- **Breaking:** a parameter set is applied and validated by its members
  only: the free `config::apply_overrides(set, overrides)` and
  `config::validate(set)` are gone, use `set.apply(overrides)` and
  `set.validate()`. `apply` is `[[nodiscard]]`: its result says whether the
  overrides were applied.
- **Breaking:** the result of `parameter_set::validate()` is named after the
  parameters, as the rest of the model: `parameter_validation_result` and
  `parameter_diagnostic` replace `configuration_validation_result` and
  `configuration_validation_diagnostic`. The messages say "unknown parameter"
  and "duplicate parameter path" (formerly "unknown configuration parameter"
  and "duplicate configuration path"), and a configuration file's empty key
  "parameter path must not be empty".

### Apps and tools

- **Breaking:** the explorer of an app has the name it has in a `Runner` and
  in a search run: `Session::neighborhood_type` is
  `Session::neighborhood_explorer_type`, and `BoundApp::neighborhood()` is
  `BoundApp::neighborhood_explorer()`.
- **Breaking:** a Session reads and writes streams with the verbs of the free
  functions of `<easylocal/app/io.hpp>`: `read_input(in)`, `read_solution(in)`
  and `write_solution(out)` replace the stream overloads of `load_input`,
  `load_solution` and `save_solution`, which take only a file path.
- **Breaking:** the block of `cli::run`'s switches, `cli::parameters`, is
  `cli::CommandLineParameters`, named as the other parameter blocks, and the
  program's own parameter set in `cli::options` is `program_parameters`
  (formerly `parameters`): `cli::run(app, argc, argv,
  {.program_parameters = own})`. The stability page says
  that `cli::options` is Stable but for its Experimental `tuning`.
- **Breaking:** a Session draws with its own RNG: `use_random_solution()`,
  `use_random_move()` and `check_random_move_distribution([rounds_per_move,
  stop])` no longer take a generator (they took any `std::mt19937_64`, while
  the session's was the one that made a session reproducible); `set_seed`
  reseeds it. Every constructor takes the seed last, 0 by default:
  `Session{app, input}` is `Session{app, input, 0}`.
- **Breaking:** the capabilities and results of a Session are named after its
  functions: `supports_check_neighborhood_costs`,
  `supports_check_move_independence` and
  `supports_check_random_move_distribution` replace
  `supports_cost_consistency_check`, `supports_move_independence_check` and
  `supports_random_distribution_check`; `supports_read_input`,
  `supports_read_solution` and `supports_write_solution` replace
  `supports_input_loading`, `supports_solution_loading` and
  `supports_solution_saving`; `supports_improving_moves`, the group of
  `use_first_improving_move`, `use_best_move` and `neighborhood_statistics`,
  replaces `supports_improvement_selection`. `check_neighborhood_costs_result`,
  `check_move_independence_result` and `check_random_move_distribution_result`
  replace `neighborhood_cost_check_result`, `move_independence_result` and
  `random_distribution_result`.

### Checking tools

- **Breaking:** `testing::check_delta_evaluator` is
  `testing::check_delta_cost_component`, in
  `<easylocal/testing/delta_cost_component.hpp>` (formerly
  `<easylocal/testing/delta_evaluator.hpp>`), and its template parameter, like
  that of `delta<Component, Delta>()`, is `Delta`: it checks a delta cost
  component, as the documentation calls it.

## [4.0.0-alpha.2] — 2026-10-07

### Problem model

- **Breaking:** `neighborhood_explorer_for<NHE, SM>` requires what the library
  used: `input_type` and `solution_type`, those of the SolutionManager, and
  `input()`, which debug builds and unions called. An explorer without them is
  rejected where it is composed; `neighborhood_explorer_base` provides them.
- `EASYLOCAL_VERIFY_DELTAS`, defined when compiling, makes every search
  compare the component values its deltas gave with a full evaluation after
  each move it keeps, and stop at the first disagreement naming the
  component (tutorial chapter 4).
- A neighborhood recipe checks the explorer's contract member by member when
  it is written (when the explorer declares its `solution_type`, else when it
  is bound): a missing `move_type` or `is_valid`, and a `make_move` that is
  not `const`, are each reported with the signature to write. A `final` explorer
  given a delta cost component, and recipe arguments that construct no
  SolutionManager, get their own message; the hint about inherited
  constructors appears only where it applies. `delta<...>()` checks the
  `delta_evaluate(const Solution&, const Move&) const` of the delta cost
  component on the explorer's Solution and Move, and that it returns a change
  of the component's value, where only binding the recipe did.
- An app checks a runner registration on its own neighborhood when it is
  added, as it already did for a runner with a neighborhood of its own: an
  algorithm that cannot run on it (First Improvement on an explorer without
  `moves()`) no longer compiles there, instead of deep inside `cli::run` or
  `Session::run`; `with_neighborhood` checks that the neighborhood explores
  the app's Solution.
- A neighborhood union may hold the same explorer type twice (two
  parameterizations of one move): `child<I>()` names a child by its position,
  and `child<NHE>()` asks for a type that occurs once. Its random moves draw a
  child from the raw biases, without normalizing them on every call.
- `describe` (and so the Session, the TextUI and `cli::run`) describes a
  `std::variant` by the alternative it holds, and the move of a neighborhood
  union by the move of its child.
- The tutorial (chapter 6) and the reference state the rule of a union's
  deltas: a component is evaluated by deltas only when every child has one.
- An explorer's `input()` is optional, as the contract says: the debug checks
  of an app and of a neighborhood union, and `check(app, ...)`, use it only
  when the explorer has it.

### Cost

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
- **Breaking:** unsigned integers are no longer costs. A cost component or a
  `cost::apply` function that returns one, and a `cost::lexicographic`,
  `cost::pareto` or `cost::hierarchical` level of an unsigned type, fail to
  compile with a message: the difference of two unsigned costs wraps around,
  so Simulated Annealing never accepted an improving move.
  `cost::arithmetic` excludes them, weights included.
- **Breaking:** the concepts of the structured costs are named by what the
  type is: `cost::hierarchical_cost`, `cost::lexicographic_cost` and
  `cost::pareto_cost` replace `cost::hierarchical_type`,
  `cost::lexicographic_type` and `cost::pareto_type`. The traits
  `cost::is_hierarchical`, `cost::is_lexicographic` and `cost::is_pareto`
  (and their `_v`) are no longer public: test a type with the concepts.
- `cost::from_text` rejects NaN, which compares with no cost (infinity is
  still read).
- The tutorial (chapter 2) and the reference say that costs are minimized,
  and how to maximize; chapter 2 lists `cost::objectives` and the Pareto
  cost among the structured costs. The reference says how to use one
  parametric component twice (a named type for each).

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
- **Breaking:** `easylocal::unlimited` is a tag of its own type,
  `unlimited_t`, which converts to the unlimited `limit`, rather than a
  `limit`. `config::field` and `config::range` take the tag: a count where a
  domain goes (`field<...>("...", 5)`) and a range of two types
  (`range(0.0, 1)`, `range(1, std::size_t{1000})`), which compiled and threw
  `std::invalid_argument`, or picked the unlimited overload, no longer
  compile. `limit x = unlimited;` and `limit{unlimited}` are unchanged.
- **Breaking:** `config::validation_result` owns its message, a `std::string`
  where it held a `std::string_view` that had to outlive it:
  `validation_result::failure()` takes a reason built at run time too.
  `config::check_schema` gives the same reason as the validation of a
  parameter set, with the field first, `cooling_rate: expected a value in (0,
  1), got 2`, where it read `cooling_rate is out of its range`.
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
- **Breaking:** a TOML file reaches `load_and_apply` and `cli::run`: the
  `--config` file is read by a `config::config_file_reader`, the new last
  argument of `load_and_apply` and `cli::options::read_config`, by default
  `config::load_config_file`; `config::load_toml_file` of the TOML adapter is
  one. The TOML adapter returns a `config_file_parse_result`, with
  `config_file_diagnostic`s, whose `config_file_error` gains `parse_error`
  and `unsupported_value` and whose diagnostic gains a `column`:
  `toml_config_parse_result`, `toml_config_diagnostic` and
  `toml_config_error` are removed, the path of an unsupported value is its
  `text`, and the place of a parse error is in its `line`, `column` and
  `text` (the file) rather than in its message. `print_diagnostics` also
  takes a `config_file_parse_result`, and prints the column after the line.
- A SolutionManager follows the rule too: one whose `parameters_type` is a
  parameter block is constructed from the Input and it,
  `solution_manager<SM>(parameters, args...)`, and its parameters are at the
  root of a runner and of an app, `solution_manager.*`, beside `cost.*`;
  the TextUI's problem parameters (`P`) show them.
- A schema is computed once, at compile time: a mistake in it, such as an
  inverted range, is reported where the schema is first read, naming
  `parameter_schema()`, rather than in a `validate()` or at run time. The
  name of a field or group must be a segment of a path (a letter or `_`, then
  letters, digits and `_`): `"cooling rate"` or `"a.b"` no longer compiles.
- `cli_help`, and so the `--help` of `load_and_apply` and `cli::run`, shows
  each parameter's values (`values: (0, 1)`, `<true|false>` for a boolean)
  and its condition (`only if ...`), and leaves out the read-only ones, which
  the command line cannot set.
- A value that does not parse says what was expected: `expected a
  non-negative integer in [0, 4294967295]` (where `-1` read "expected
  integer"), `expected a number`, `expected 2 elements, got 3`, and the
  element of a list by its position, `element 2: ...`. The configuration
  reference gains a "Values as text" section, with the limit that a text
  element of a list cannot hold a comma, and the frontend functions it left
  out.
- `parameter_set::apply` and `apply_overrides` take overrides that own
  their text, as a configuration file and the TOML adapter give them: the
  extra `override_views` step, whose views dangled with a temporary, is no
  longer needed.
- `parameter_set::apply` validates the blocks the batch does not touch too,
  and `load_and_apply` relies on it: its heuristic over the paths of the
  batch rejected an override of a nested field that repaired a requirement
  of the enclosing block, which `apply` accepted.
- A range with no upper bound honours `open_high()`: `range(0.0,
  unlimited).open_high()` leaves out infinity, and `range(1,
  unlimited).open_high()` leaves out `unlimited` for a limit, where both were
  accepted. Its text shows the end, `[1, unlimited]`, or `[1, unlimited)`
  without it, where both read `[1, unlimited)`.
- `config::override_result` is `[[nodiscard]]`, and so are
  `parameter_set::apply` and `Session::configure`, whose ignored result hid a
  misspelt path.
- `check(app, ...)` reports, as `runner parameters`, a registered runner whose
  `parameters_type` is not a parameter block (and not empty): the app runs it,
  but no frontend can change its parameters. The tutorial's `RandomDescent`
  declares a schema for its `max_evaluations`, configurable as
  `search.max_evaluations` in a runner and `runners.<name>.max_evaluations`
  in an app.

### Runners and solvers

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
- **Breaking:** the four tabu searches share one parameter block,
  `TabuSearchParameters<ListParameters, CandidateParameters>`: the parameters
  of the candidate strategy are the group `candidates`
  (`runners::candidates::FirstImprovementParameters`,
  `AspirationPlusParameters`, `EliteListParameters`; none for `TabuSearch`),
  `{.tabu_list = {...}, .candidates = {.min_moves = 10}}` in the code and
  `search.candidates.min_moves` in a configuration.
  `FirstImprovementTabuSearchParameters`, `AspirationPlusTabuSearchParameters`
  and `EliteCandidateTabuSearchParameters` are gone.
- **Breaking:** `search_run::with_context(ctx)` is replaced by
  `run.with_evaluation(wrap)`, which keeps the run's context and swaps only its
  evaluation facility for `wrap(run.evaluation())`. A decorated context
  without `better_or_equivalent()` or a SolutionManager silently dropped the
  target, the solution events and the archive identity: the assignment
  example's `slow-fi` ignored `--target`. `search_run` takes the facility as a
  third template parameter, and `run.evaluation()` returns the one it
  evaluates with.
- **Breaking:** `BoundRunner::run()` is const, and each run starts from a new
  algorithm (built from its parameters, or copied): state a custom algorithm
  keeps no longer passes from one run to the next, as for the runs of an app.
  An algorithm without a parameter block must be copyable to run.
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
- **Breaking:** a pipeline checks the names of its stages when it is built,
  once, instead of at every `solve()` and `configuration()`, which no longer
  throw. A stage rejects run options with a control (`stage & with(control)`
  dropped it: the control goes to `solve()`), with `std::invalid_argument`,
  and a floating-point target for an integer cost (`target(0.5)` was
  truncated), at compile time. `pipeline_stage::limits()`, which returned a
  detail type, is private.
- **Breaking:** a pipeline stage of a deterministic algorithm (First and Best
  Improvement declare `static constexpr bool deterministic = true`) with more
  than one attempt, all from the same solution and without `restart(...)`,
  throws `std::invalid_argument`, which suggests
  `restart(initialization::random)`: its attempts repeated the same run. A
  later stage is checked when the pipeline is built, the first one when it
  runs from a fixed start.
- **Breaking:** the Pareto archive keeps one point per non-dominated cost by
  default, the first reached: it kept every distinct solution of equal cost,
  an unbounded front that made each offer scan a whole plateau (Hill Climbing
  on a plateau ran thousands of times slower). `run_options::keep_front(
  {.keep_equivalent = true, .max_front_size = n})`, the new
  `pareto_archive_parameters`, keeps them all, up to `n` points; the solvers
  merge the fronts of their runs with the same parameters.
  `pareto_archive::offer` takes the relations to compare with, and `first()`
  returns the first point of `sorted()` without sorting.
- **Breaking:** `pareto_search_result` derives from `search_result`, whose
  fields it repeated, and adds the `front`: code that takes a
  `search_result&` takes a Pareto result, and one built with designated
  initializers builds its `search_result` base first.
- **Breaking:** `tabu_candidate<Run, WithCost>` has `cost()` and
  `equivalent_cost()` only `WithCost`, the candidate a list whose state
  declares `needs_cost` receives: a custom list that read the cost without
  declaring it dereferenced a null pointer in a Release build. When every
  move is tabu, Tabu Search applies the least tabu move with the evaluation
  its scan made, instead of evaluating (and tracing) it again.
- The real parameters of Great Deluge, Simulated Annealing and the tabu
  searches with no upper bound (the levels, the temperatures, the running
  time of `TimeBased`, `reheat_ratio`, `increase`, the fluctuations,
  `aspiration_level`, `quality`) exclude infinity in their domains
  (`(0, unlimited)`, `[1, unlimited)`), instead of a check of their own in
  `validate()`: an infinite value is reported as out of its domain, by the
  block and by a configuration (`expected a value in (0, unlimited), got
  inf`), and `--help` lists the domain as it is.
- The best cost of a run in progress:
  `run_control::observe_best_cost<Cost>(observer)` calls the observer with the
  run's first cost and then with each better one, on the commits that improve;
  the runs of MultiStart and of a pipeline report only costs better than those
  of the runs before. `shared_best_cost<Cost>` keeps the last one for another
  thread, and `run_control::observing_progress` copies a control with another
  progress observer. REST's run status has `progress.best`, encoded by the codec
  as the cost is, and the TextUI's progress line shows `best=` after the counts.
- The progress that MultiStart and a pipeline report to the caller's observer
  is the solve's: each run adds to the evaluations and iterations of the runs
  before it, and the evaluation limit is the solve's, so progress bars no
  longer jump back at every start, attempt or stage. `run_control` gives its
  `stop_token()`.
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
- A runner rejects, with a message, a neighborhood whose `make_move` takes the
  Solution by value or by const reference (`runner | neighborhood<NHE>()`): it
  changed a copy, and Hill Climbing ran forever.
- The policy contracts of Simulated Annealing and Tabu Search are public
  concepts: `acceptance_policy_for<Acceptance, Cost, RNG>` (formerly in
  `detail`) and the new `aspiration_for<A, Run>`, which constrains the four
  tabu searches.
- Hill Climbing, First Improvement and Best Improvement emit
  `incumbent_updated` at each improving move, as the other runners do, through
  the new `search_run::commit_improvement()`: every built-in runner reports
  its new best costs.
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
- A solver with an RNG whose result is narrower than 64 bits
  (`std::minstd_rand`) compiles: its seed was narrowed in a braced
  initializer.

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
- **Breaking:** a `BoundApp` holds the Input, its services and a copy of the
  registrations, and builds the algorithm of each run from the registration's
  parameters: it no longer builds an instance of every registered algorithm
  at bind (which the Session built and never ran), and runs on the same
  bound app no longer share an algorithm's state.
- **Breaking:** `App::runner_count`, which counted the pipelines too, is
  `App::registration_count`.
- **Breaking:** `RunParameters` is the block of a run's limits: `target`,
  `timeout` (seconds, as text) and `max_evaluations`, with
  `options<Cost>(input[, base])`, which gives their run options. `cli::run`
  reads its `--target`, `--timeout` and `--max_evaluations` through it
  (`cli::parameters::run_parameters()`), and
  `cli::parameters::timeout_seconds()` moves to `RunParameters`.
- **Breaking:** `Session::run` gives each run a generator of its own, seeded
  with one draw of the session's RNG, as the TextUI already did: the same seed
  and the same commands now give the same runs in `Session`, `cli::run`, the
  TextUI and REST, where the TextUI's runs differed from the others'. Runs of
  stochastic runners differ from those of alpha.1 with the same seed.
  `docs/stability.md` lists the conditions of reproducibility (no time limit,
  the same commands in order, a solver's stream across `solve()` calls, REST
  seeds, the same standard library).
- **Breaking:** `named_run_result`, what `run("name", ...)` returns, carries
  the effort as `search_result` does, in `evaluations`, `iterations` and
  `termination`, instead of `std::optional<run_effort> effort`; a custom
  result without them gives 0, 0 and `completed`. `Session::last_run_effort()`
  is set by every run that completes, `cli::run` always writes the
  iterations, evaluations and termination, REST always gives `termination`,
  and the TextUI's Last run box always shows how the run ended.
- `Session::run` runs on the session's bound app instead of binding the app
  again for every run, as `App::run` does; when the parameters of the app
  have changed through `app()` since it was bound, it binds it again first,
  so a run still uses the current parameters.
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
- `pipeline(name, stages...)` checks the names of the stages when the
  registration is made, and throws `std::invalid_argument` there, instead of
  at every `configuration()` of the app; `cli::run` no longer catches it.
- The help of `--target` and the reference say that a pipeline gives the
  target to its last stage, unless that stage has a target of its own.
- `cli::run` checks what the problem cannot do (`--solution` or `--output`
  without the I/O hooks, a `--start` it has no solutions for) before it reads
  the Input and runs, where `--output` failed after the run; saves the
  solution with `--tuning.print` too, which ignored `--output`; and prints
  every error after `error: ` (`unknown runner`, `tuning.irace: ...`, `trace:
  ...`). Its "time" is documented as including the binding of the app.
- The scalar number of a cost for tuning (`cost::scalar`, `--tuning.print`)
  documents where it stops ordering costs: past 2^53, as a lexicographic cost
  of three levels or a large hard cost with the default weight reach, its
  lower levels round away; a `scalar_cost` hook is the remedy.
- The logging API (`<easylocal/utils/logging.hpp>`) is Experimental, since the
  library emits no records yet; the documentation no longer presents it as
  framework diagnostics. `stderr_sink` writes a record in one write (up to 1
  KiB), and `set_sink` is no longer `[[nodiscard]]`.

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
- **Breaking:** `check(app)`'s `app_check_coverage` and `coverage()` are
  `app_check_composition` and `composition()`, its `neighborhood_graphs`
  `neighborhoods`, and `print_report` writes `composition:`: they count what
  the app composes, which both forms of `check` now fill. The form without a
  solution binds the app once, and checks the runners once. `check(app)`
  compares the repeated evaluation with the cost's equivalence, and reports
  each invalid parameter block of the app with its path.
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
- The neighborhood scans of `Session` (`neighborhood_preview`,
  `neighborhood_statistics`, `check_neighborhood_costs`,
  `check_move_independence`, `check_random_move_distribution`) take a
  `std::stop_token` as their last argument (none by default) and end at the
  next move once it is requested; their results say so in `stopped`. The
  TextUI runs them off its event loop, on a Session of its own with the
  current solution: the interface answers during a long scan, and leaving the
  Move page, applying a move or starting another scan stops it.

### Tracing

- **Breaking:** a recorder observes the core events of its cost type, its new
  `cost_type`, and those without a cost, instead of declaring every event
  observed: the memory and JSONL recorders silently dropped an event of
  another cost type, and the binary recorders converted its cost to theirs.
  The memory and JSONL recorders no longer claim the application events,
  which are ELTR records only, and `memory_recorder::stored_as_is` is
  private. `trace::emit` rejects at compile time a tracer whose
  `observes<Event>` is true but that has no `emit()` taking the event, such
  as an `emit` declared for another cost type.
- **Breaking:** a run rejects at compile time a recorder whose `cost_type` is
  not the runner's cost, which recorded nothing of the run; a pipeline gives a
  stage on another cost (`until_feasible()`) the caller's recorder through a
  tracer that forwards only the events without a cost.
- **Breaking:** `trace::jsonl_recorder` starts with a header line,
  `{"event":"trace","version":1,"metadata":{...}}`, the first line of
  `eltr.py`'s JSONL output without the cost layout; the new
  `trace::jsonl_options` gives its metadata, to the recorder and to
  `write_jsonl`. A reader of the events skips the line with event `trace`.
- **Breaking:** the JSONL recorder writes the library's structured costs:
  its default cost writer, `trace::default_json_cost_writer` (was
  `ostream_json_cost_writer`), writes a `cost::lexicographic` or a
  `cost::pareto` as the array of its levels and a `cost::hierarchical` as
  `{"hard": ..., "soft": ...}`, nested as the types are, the shape `eltr.py`
  decodes ELTR costs to; a JSONL trace of such a cost, and `cli::run`'s
  `--trace file.jsonl`, no longer fail to compile or stop with an error. It
  no longer falls back to `operator<<`, whose text need not be JSON: another
  cost type takes a cost writer of its own.
- **Breaking:** the solution hashes of `solution_visited` are strings of 16
  hexadecimal digits in JSONL (`jsonl_recorder`) and in the output of
  `eltr.py`, the STN's nodes and edges included: as JSON numbers, JavaScript
  and jq rounded them to doubles. ELTR keeps them as `u64`.
- **Breaking:** `solution_visited` has a `previous_hash`, the hash of the
  solution the move was applied to, 0 at the start of a run and for a
  solution no move reached (`evaluate_solution()`, such as a sample of
  Pareto Late Acceptance's history); it ends the event in ELTR and in JSONL.
  `eltr.py --format stn` draws an edge per move, from `previous_hash` to
  `hash`, instead of one per pair of consecutive visits, which was wrong for
  an algorithm that keeps several solutions. A run that traces visits hashes
  the solution before and after each move.
- **Breaking:** the synchronous ELTR recorder has one name,
  `trace::binary_recorder`, now the class itself: `buffered_binary_recorder`,
  of which it was an alias, is gone. The options of both binary recorders are
  `trace::binary_recorder_options` (was `binary_buffer_options`), since they
  carry the header's metadata and the timestamps as well as the buffering.
  The ELTR format is unchanged.
- `trace::event::run_context`, a core event without a cost (ELTR tag 12): the
  solvers emit it before each run, with the pipeline stage's name and index
  and the attempt (or MultiStart's start), so the runs of a solve can be told
  apart in a trace, the stages on the hard cost included. The memory and JSONL
  recorders keep it, and `eltr.py --format summary` gives each run its stage
  and attempt.
- Simulated Annealing traces its temperature:
  `trace::event::temperature_changed`, a core event without a cost (ELTR tag 13,
  JSONL, memory record `temperature_changed_record`), at the start of a run and
  at each change of the schedule's temperature, only for a tracer that observes
  it. The tracing guide's example of an application event is now
  `weight_changed`.
- Timestamps in traces, as a recorder option: with `timestamps` in
  `binary_recorder_options` or `jsonl_options`, each core event ends with
  `elapsed_ns`, the nanoseconds since the recorder was constructed (a `u64`
  field of the ELTR schemas, a field of the JSONL line), for anytime and
  time-to-target analyses. Without it, no clock is read.
- `cli::run` records the trace of the run with `--trace <file>`: JSON Lines
  for a `.jsonl` name, ELTR otherwise, with timestamps and with the program and
  its parameters as metadata. A pipeline stage on another cost than the app's
  (`until_feasible()`) records only its `run_context`.
- `trace::without<Types...>(tracer)`, with the new `trace::without_event_types`,
  hides events by their type: the events without a cost
  (`neighborhood_selection`, `tabu_escape`, `tabu_tenure_changed`,
  `run_context`) and application events, which the template form
  (`without<event::solution_visited>`) could not name.
- `eltr.py --format summary` marks `"unfinished": true` a run without
  `run_finished`, which an exception ended or a truncated trace cut. The
  solvers reference says how an exception thrown mid-run propagates.
- `eltr.py --format stn` attributes the network to the runs of the trace:
  each node and edge lists the runs that visit it (their indices, as the
  summary lists them), and `starts` and `ends` give the first and the last
  solution each run visits.

### Optional components

- **Breaking:** a REST run starts as `cli::run` does: the new `start` field
  chooses `"random"` (drawn from the run's seed) or `"initial"`, and without
  it or an `initial_solution` a problem with random solutions starts from a
  random one, where every run started from `initial_solution()`. Run
  resources report the `start`.
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
- `rest::blueprint_options::max_timeout` bounds the time of the runs of a REST
  service: a request with a longer `timeout` is rejected with `422`, and a run
  without one gets that limit.
- REST: the status of a run that ended gives its `termination`
  (`target_reached`, `cancelled`, ...), its `cost` and its final counts. A run
  is `cancelled` when its runner says so, no longer when a cancellation came
  after it had ended on its own terms.
- The REST examples listen on `127.0.0.1` only, not on every interface, and
  exit with status 1 when they cannot listen on their port, where they exited
  with 0.
- TOML: a parse error says where it is: its diagnostic has the `line` and the
  `column` of the error, and the name of the file. An array of arrays,
  `[[1, 2], [3]]`, sets a list of lists, as on the command line, and an
  unsupported value says what it is (a date or time, or an array holding
  strings) instead of naming the "textual override mapper".
- `--tuning.irace`: `write_irace_stub` leaves out the cost's and the read-only
  parameters itself, as its contract says, where `cli::run` did it for it; it
  writes no `configurations.txt` while nothing is tuned, and `cli::run` then
  says so, and asks for the instances when it has none; a boolean and an
  unlimited count say how to tune them; a default runner in the program's
  options no longer restricts the tuning to it; and the program's path stays
  as given when it cannot be made canonical. `write_irace_stub` is
  `[[nodiscard]]`.

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
- The debug assert that the solution of every evaluated move is valid, a
  whole-solution check that made Simulated Annealing 43 times slower in the
  dev preset, is opt-in: `EASYLOCAL_EXPENSIVE_CHECKS` (a macro, and the CMake
  option of the same name for EasyLocal's own targets; the `asan` preset sets
  it). The check after each move made stays.
- `EASYLOCAL_SANITIZERS` (such as `address;undefined`) builds EasyLocal's own
  tests and examples with those sanitizers, a report stopping the program;
  the `asan` preset uses it, and the full CI runs it with GCC 16.
- Through `add_subdirectory`, EasyLocal no longer installs its headers and
  package into the consumer's prefix: `EASYLOCAL_INSTALL`, on by default only
  when EasyLocal is the top-level project, enables its install rules. The
  installed package version file is architecture-independent unless it
  bundles the compiled FTXUI of the TUI.
- On Windows, a path parameter is UTF-8 text, as TOML, REST and the TextUI
  give it: an override reads it, and `format_value` writes it, in UTF-8 where
  they used the ANSI code page, and so do the TOML file names, the name of a
  configuration file in its diagnostics, and the paths the TextUI shows and
  edits. The command line counts as UTF-8 too: the `--config` file, the
  program's own name, and the paths `cli::run` writes in its messages and in
  the irace stub (a program that runs on Windows with another code page
  declares UTF-8 as its code page in its manifest).

### Documentation and examples

- The stability levels no longer contradict each other (`docs/stability.md`):
  the Stable level excludes what the Experimental one lists, which now names
  the adapters' headers, tracing beyond the tracer protocol, `tuning_range`
  and `cli::options::tuning`, logging and the `with_*` spellings of the
  composition.
- Tuning with irace is a tutorial chapter of its own, chapter 12, after the
  applications of chapter 11; the interactive tester, the checks, the REST
  service and the observation of a run are now chapters 13 to 16.
- Chapter 5 lists the algorithms with their header and needs, linking the
  runners reference for their parameters, with Pareto Late Acceptance and
  `time_limit_reached`; chapters 9 and 11 show `RunParameters`, the checks of
  a registration, pipelines and `BoundApp`; the app and Session reference
  lists every member (`check_configuration`, the `with_*` spellings,
  `has_move`, `rng`, `input_handle`...) and describes `configure` as it is;
  the reference index lists what `app/` and `cost/` hold.
- The briefs of the `LimDynamic`, `Foo` and `RandomFoo` tabu lists and the
  runners reference expand their names (limited dynamic tenure, Fluctuation Of
  the Objective) and give their sources.
- The README and the quick start say not to build with `-ffast-math` (or
  `-ffinite-math-only`, `/fp:fast`), which lets the compiler drop the
  library's checks for NaN and infinite values.
- The tutorial runs its annealing again after `load_and_apply`, so the
  parameters of the command line show (`configured annealing`); chapter 3
  lists every algorithm that needs `moves()`, `random_move()` or `inverse()`;
  chapter 11 gives Simulated Annealing's parameter block and the header of
  `cli::run`; the configuration reference warns that `-ffast-math` breaks
  the handling of NaN and infinity.
- The tutorial no longer takes the launcher of chapter 12 from `examples/tsp`:
  `examples/tutorial/launcher_main.cpp` opens a 2-opt and a swap app of the
  tutorial's own TSP. `AGENTS.md` says what each example directory is.
- The quick start says that costs are minimized and how to maximize.
- The tutorial's chapter 16, a table of EasyLocal 3 concepts and their
  counterparts, is part of [Coming from EasyLocal 3](docs/from-easylocal-3.md),
  which it duplicated; the tutorial has 15 chapters.
- The migration page says that `hard_soft` changes what Simulated Annealing
  accepts, compared with EasyLocal 3's `HARD_WEIGHT` (a weighted sum still
  crosses infeasible regions), and that Best Improvement keeps the first of
  equally good moves, where EasyLocal 3 chose one at random.
- The migration page and the benchmarks name the same EasyLocal 3, the
  `easylocal-legacy` v3.4.1 release that the benchmarks build, where the page
  cited a Bitbucket commit and the benchmarks v3.3.1; the migration page's
  tables say "EasyLocal 4", and the quick start links the README's list of
  compilers.
- The tutorial's custom runner ends with `local_optimum` when no move exists,
  not `completed`, and its comment lists every reason should_stop() stops
  for; the tutorial's 2-opt `is_valid` rejects the pair `(0, n - 1)`, which
  `moves()` skips because it removes the same edge twice.
- The cost components of the assignment, exam timetabling, TSP and PFSP
  examples have a `name()`, so `--report` and the TextUI's solution window
  name them (`component Capacity ...`) instead of numbering them, and the
  assignment's `CapacityValue` describes itself ("0 units over capacity on 0
  machines") instead of "(not printable)".
- The `configs/small.cfg` of the assignment, exam timetabling and TSP
  examples are commented sample configurations of a real run, where they were
  test fixtures that cut the run to one iteration; the fixtures, and the
  invalid TSP configuration, are in `tests/configs`, and a test runs each
  sample.
- The exam timetabling README's target example, `--target=8`, can be reached:
  no timetable of `small.exam` has the penalty 0 it gave.
- The TSP readers of the tutorial and of `examples/tsp`, and the tutorial's
  REST codec, reject asymmetric distances, which the 2-opt delta does not
  handle: it reverses a segment as if it cost the same.
- The TSP example's launcher has a third app, `tsp-union`, on the union of
  2-opt and swap, whose biases the problem parameters window edits; an
  end-to-end test drives that window.
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

### Fixed

- **Breaking:** the expressions of conditions and requirements compute
  numbers in `double`, as their irace export does: `7 / 2` is 3.5 (it was 3),
  `unlimited` is +infinity (`unlimited + 1` overflowed), and a minus applies
  after the conversion (`-count` wrapped an unsigned count). Their R text
  writes infinity and NaN as `Inf` and `NaN` and escapes the quotes and
  backslashes of a text; `text_with`'s second parameter is now `r_syntax`.
- **Breaking:** TextUI: `tui::options::input_path` and `solution_path` are
  `std::filesystem::path`, as `path_base` is, and `exit_label` is gone: q
  quits a tester, and leads back to the list from a launcher's. The fields
  say their units and what 0 means (`max_render_chars` in bytes, 0 for no
  limit), and the tutorial's chapter 12 lists them.
- `neighborhood_union(...) | random_biases(...)` compiles for explorers that
  derive from no EasyLocal base, in a namespace that does not use `easylocal`:
  the operator is a hidden friend of the union's recipe, found by ADL.
- `solution_manager_base` and `neighborhood_explorer_base` no longer bind a
  temporary Input or SolutionManager, which dangled: their constructors from
  an rvalue are deleted.
- The recipes construct their components with parentheses, as their
  `std::constructible_from` checks do: `neighborhood<X>(2)` for a `double`
  parameter failed with a narrowing error inside the library (also in
  `solution_manager`, `component`, `delta` and the `random_move` of a move
  built from another type).
- `with_hard_cost()`, `until_feasible()` and `two_stage()` compile with
  co-located deltas (`delta<C>()`): every SolutionManager layer, the hard-cost
  projection included, reaches the cost components.
- `easylocal::describe` and `easylocal::write_solution` are function objects,
  which the lookup of a problem's own hooks does not find: they work for types
  with `easylocal` among their associated namespaces, which recursed into their
  own constraints.
- The Metropolis criterion of Simulated Annealing computes in `double`, and
  `cost::delta` of a `cost::hierarchical` returns a `double` (a `long double`
  only when the soft delta is one): every worsening move did `long double`
  arithmetic, software on aarch64 Linux and x87 on x86-64.
- A floating-point cost updated by deltas reaches its target: deltas that
  leave it a rounding error away (`0.1 + 0.2 - 0.1 - 0.2` is 2.8e-17) kept a
  run, and `until_feasible()` with its zero hard cost, from ever stopping. A
  committed cost that misses the target only within `cost::tolerance{}` is
  evaluated in full before the run decides, without counting in the budget.
- The weights of a `cost::sum` must be finite: a configuration that sets one
  to NaN or infinity is rejected (a negative weight is still accepted).
- `cost::hierarchical` compares a branch that has only `<` and `==`, as its
  `delta` already accepted: its `operator<=>` was deleted for such a hard
  cost.
- `config::check_schema`, and so the `validate()` of a parameter block, checks
  the block's nested groups: an invalid temperature schedule of Simulated
  Annealing or tabu list of Tabu Search is rejected by the runner's
  parameters, so `check(app)` reports it as "runner configuration" instead of
  aborting. A parameter set reports such a group once, under its own path.
- `config::one_of` holds numbers of different types as their common type:
  `one_of(1, 1.5, 2)` holds 1.5, where it narrowed it to 1.
- A configuration file (`--config`) that is a directory is an error, where it
  read as empty, and a UTF-8 byte order mark at its start is skipped instead
  of becoming part of the first path.
- `timeout(d)` with a floating-point `std::chrono` duration that is NaN throws
  `std::invalid_argument`, as `timeout(seconds)` does, instead of converting
  NaN to the clock's integer ticks (undefined behaviour).
- `with(control)` and `with(control, tracer)` with a temporary `run_control`,
  and a `search_run` built over a temporary context, no longer compile: each
  kept a reference that dangled once the expression ended.
- The aspiration level of `AspirationPlusTabuSearch` and the quality level of
  `EliteCandidateTabuSearch` are `b + (factor - 1) * |b|` for the best cost
  `b`, unchanged for a positive one: `factor * b` put them below a negative or
  zero best cost, so the elite list evaluated more moves than plain Tabu
  Search.
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
- A run with a `cost::pareto` cost that reached its target returned the first
  point of its front, which need not meet the target, and reported
  `target_reached`: it returns a point of the front that meets it, and
  `best_so_far` keeps the first cost that meets the target even when it does
  not dominate the best one.
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
- A run without a tracer builds no `move_evaluated`, `move_accepted` or
  `incumbent_updated` event, no route of a union's move (a `std::visit` per
  evaluation) and no copy of the cost before each move: Best Improvement over
  a union of two 2-opt explorers runs about 20% faster.
- `configuration()` does not compile on a temporary (a runner, an app, a
  pipeline, a recipe, a solver or a Session), whose parameter set would refer
  to an object already gone.
- `Session::run` throws `std::invalid_argument`, and changes nothing, when the
  current solution is not valid for the Input, and `cli::run` rejects such a
  `--solution` with `error: solution: ...` and status 2, instead of running
  deltas that may index out of bounds.
- `Session::last_run_effort()` is empty after a new Input and after a run that
  does not complete, instead of keeping an older run's effort.
- `cli::run` writes the time, the iterations, the evaluations and the front's
  indices as they read back, whatever the locale of its stream (`time 0,42`
  and `evaluations 2.000` under a decimal-comma locale), and the TextUI's
  progress line writes its seconds with a point under any global locale.
- `cli::run` exits with status 1 and `error: unknown exception` when a run
  throws something other than a `std::exception`, instead of terminating.
- `save_solution` (and `Session::save_solution` to a file) report the errors
  of the final flush and close, and every write error names the file.
- The names of an app's runners and pipelines are validated: non-empty,
  distinct, of letters, digits, `_` and `-`; `configuration()` and `bind()`
  throw `std::invalid_argument` otherwise, and `check(app, ...)` reports it
  (and the real cause of an invalid app configuration) instead of blaming the
  pipelines.
- The randomized contract checks (`easylocal::testing` and `check(app, ...)`)
  draw from a `std::mt19937_64` seeded with the new `check_options::seed`,
  instead of `deterministic_rng`'s four fixed values: a `random_move()` that
  rejects draws until one fits, such as the tutorial's on 40 cities, no longer
  loops forever, and the random samples differ from each other.
  `deterministic_rng` remains for unit tests that script the draws.
- `Session::check_move_independence` and `check_random_move_distribution`
  (the Move page's `D` and `U`) compare a state or a drawn move only with
  those of the same cost when the cost is totally ordered: on a 400-city
  2-opt neighborhood they take 0.1 s and 0.6 s, where the independence check
  took 254 s. Their briefs give the complexity.
- `trace::jsonl_recorder` writes its numbers in the classic locale: a stream
  imbued with a locale that groups digits wrote `"evaluations":1,234`, which
  is not JSON. The line keeps the stream's flags and precision only.
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
- The TOML adapter reads each value by its TOML type: `true` read as the
  integer 1, so no boolean parameter could be set from TOML (nor an array of
  booleans), and a float such as `3.0` set an integer parameter.
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
  which irace rejected, and open integer bounds that are fractional are no
  longer off by one.
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
  `signed char` cost) is rejected with `422` instead of wrapping, and so is an
  empty key among the `parameters`.
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
  design choices). Their marked snippets are taken from programs that are
  compiled and run as tests.
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

[Unreleased]: https://github.com/iolab-uniud/easylocal/compare/v4.0.0-alpha.2...HEAD
[4.0.0-alpha.1]: https://github.com/iolab-uniud/easylocal/releases/tag/v4.0.0-alpha.1
[4.0.0-alpha.2]: https://github.com/iolab-uniud/easylocal/releases/tag/v4.0.0-alpha.2
