# Apps and tools

`<easylocal/app/app.hpp>`, `app/check.hpp`, `app/session.hpp`,
`app/run_parameters.hpp`; adapters
`<easylocal/adapters/tui.hpp>`, `<easylocal/adapters/rest.hpp>`

An **app** names a problem graph (SolutionManager and neighborhood recipes) and
the runners available on it. Tools consume apps.

## Building an app

```cpp
auto application = app("name")
    .with_solution_manager(sm)
    .with_neighborhood(nhe)
    .with_runner<Algorithm>("runner-name", parameters);   // parameters optional

// equivalently
auto application = app("name") | sm | nhe | runner<Algorithm>("runner-name", parameters);
```

A registered algorithm must expose a default-constructible `parameters_type`
and be constructible from it. The same algorithm may be registered under
several names.

### Pipelines

An app also registers [pipelines](solvers.md#pipeline), beside its runners and
under names of the same list:

```cpp
using namespace easylocal::solvers;
auto application = app("tsp") | sm | nhe
    | runner<runners::FirstImprovement>("fi")
    | pipeline("cascade",
          stage("feasible", descent) & until_feasible() & attempts(5),
          stage("climb", climbing));
// or pipeline("cascade", (stage(...) & ...) | stage(...)), or .with_pipeline(...)
```

Each stage is a runner with its own recipes; the stages must have the app's
Input and Solution, and the last one the app's cost (checked at compile time).
For every tool a pipeline is one more runner: it is listed among the runners,
run by name from the current solution (`pipeline.run(input, solution, rng)`,
so its first stage's attempts all start from that solution), and configured
under `runners.<name>.<stage>.*` (`--runners.cascade.climb.search.*`,
`runners.cascade.feasible.attempts`).

| Member | Purpose |
| --- | --- |
| `name()` | the app name |
| `runner_config<A>()`, `runner_config<A>("name")` | the stored parameters |
| `runner_name<A>()` | the registered name |
| `configuration()` | the app's parameters as a `config::parameter_set`: `cost.*`, `neighborhood.*` and `runners.<name>.*` |
| `bind(input)` | the *bound app*: services built once for `input`, which it borrows (a temporary Input is rejected) |
| `run("name", input, solution, rng, options...)` | run the runner or pipeline registered under a name; `std::optional<named_run_result>`, empty for an unknown name |
| `run<A>(input, solution, args...)`, `run_at<I>(...)` | run by algorithm or by registration index |
| `run_at_with_rng<I>(input, solution, rng, options...)` | run by index; `rng` goes to stochastic algorithms only |
| `make_runner<A>([name])`, `make_solver<Solver, A>([name,] config)` | standalone runner or solver |
| `for_each_registration_name(visitor)` | the name of every registration, runner or pipeline, in order |
| `for_each_runner_registration[_indexed](visitor)` | iterate the runner registrations (adapters) |

The `run` members bind the app to the Input for that run only, with the
current runner parameters. A bound app (`bind`) offers
`solution_manager()`, `neighborhood()`, `input()`, `runner<A>()`,
`run<A>(solution, args...)` and `run_at<I>(...)` on services built once.

`named_run_result{solution, cost, effort}` keeps what every runner result
provides (`search_result_for`): a runner chosen by name may be any algorithm,
built-in or your own, each with its own result type. `effort` is a
`run_effort{evaluations, iterations, termination}` when the result has those
members, as `search_result` does, and empty otherwise.

## Tools

| Tool | Purpose |
| --- | --- |
| `check(app, input[, solution]) -> app_check_report` | contract checks of the composed problem; `print_report` |
| `cli::run(app, argc, argv[, options]) -> int` | the app as a command-line program (`<easylocal/app/cli.hpp>`): see below |
| `tui::run(app, options)`, `tui::run_launcher(options, apps...)` | interactive terminal tester (TUI component, FTXUI); `tui::options` |
| `rest::blueprint(prefix, app, codec, options)` | Crow blueprint: asynchronous runs, status, cancellation, solutions (REST component); see [REST](../rest.md) |

`tui::run_launcher(options, apps...)` opens a list of apps over the same
problem. The launcher owns the Input, read from `options.tester.input_path`
when it is set, and the current solution: its first entry, *Input and
solution*, is a tester with the Input/Output page only, which loads and saves
them; each app opens on them in the complete tester, and what a tester leaves,
loaded or computed, becomes the shared state. The apps must have the same SolutionManager
recipe, cost included, which a `static_assert` checks; they differ in the
neighborhood and the runners.

Tools give stochastic runners an RNG they own, from a configurable seed: the
TextUI `seed` option (also editable on its Run page) and REST's per-run `seed`
(default `blueprint_options::seed + run id`). `run("name", ...)` passes the RNG
to the algorithm only if it takes one, so deterministic runners ignore it.

The TextUI edits the app's parameters in modal windows: `G` on the Run page
opens the selected runner's (`runners.<name>.*`) before running it, `P` the
problem's (`cost.*`, `neighborhood.*`). Values are checked before anything
changes and stay for the rest of the session. Its *Target cost* field, when
filled, stops each run at the first solution that reaches it; below it, the
current cost is shown in the same syntax. Its *Time limit (s)* field, when
filled, stops each run after that many seconds; the progress shows the time
elapsed, and the result says "(time limit reached)". Its *Evaluations* field
does the same with a number of evaluations ("(evaluation budget
exhausted)").

`cli::run` parses the command line and a `--config` file with
`config::load_and_apply`: its own block `cli::parameters` at the root
(`instance`, `seed`, `runner`, `start`, `solution`, `output`, `target`,
`timeout`, `max_evaluations`, `report`), the
app's `configuration()`, and `options.parameters`, the program's own set;
`options.defaults`, a `cli::parameters`, gives the values of its switches
before the command line. It
then builds a `Session` with the seed, loads the Input, takes the starting
solution (`--solution`, else `--start`: `random` by default when the problem
has `random_solution`, `initial` otherwise), runs the runner by name, with
`stop_at` when `--target` is set, `timeout` when `--timeout` is (seconds) and
`max_evaluations` when `--max_evaluations` is, and writes `cost`, `time`, the session's
`last_run_effort()` when the runner reports it (`iterations`, `evaluations`,
`termination`), with `--report`
the session's `cost_report()` (a line `component <name> <value>` for each
component, followed by its description, indented), and the solution
(or saves it to `--output`) to `options.out`; errors go to `options.err`. It
returns 0, 2 for an invalid command line or an unknown runner, 1 when the run
throws. It requires the `read_input` hook, and the solution hooks only when the
corresponding switches are used.

### Tuning with irace

`<easylocal/app/tuning.hpp>`; tutorial: [chapter 11](../tutorial/11-apps-and-tools.md#tuning-the-parameters-with-irace).

`cli::run` also reads the block `easylocal::TuningParameters` under `tuning`:

| Switch | |
| --- | --- |
| `--tuning.irace=DIR` | write an irace scenario to `DIR`, which it creates, and exit without loading the Input |
| `--tuning.print=cost`, `cost_time` | print only the cost as one number (`scalar_cost`), and the running time in seconds after it with `cost_time` |
| `--tuning.hard_weight=W` | the weight of a hard cost over a soft one in that number; 10^9 by default |

`options.tuning`, a vector of `tuning_range{path, domain}`, gives the values
to try for parameters instead of the domains of their schemas; a range must
lie within the declared domain and name a parameter of the app or of
`options.parameters`, or the export fails with status 2.

`write_irace_stub(irace_stub) -> irace_stub_result` writes the scenario:

- `parameters.txt`: the parameters with a finite domain, as `path "--path=" type
  (values)`, with `r`, `i` (`,log` on a logarithmic range), `o` for a set of
  numbers and `c` for text and the runner; an open bound moves inward by one
  step of irace's 4 digits. With several runners a categorical `runner` is
  added and each `runners.<name>.*` gets the condition `| runner == "<name>"`;
  a runner chosen with `--runner` is the only one written. A field's
  `only_if` condition becomes an irace condition when it names tuned
  parameters, those that are not tuned replaced by their values; when it names
  none, it is decided at once, and a parameter whose condition is false is
  commented out as inactive. Each `require` that names a tuned parameter
  becomes a `[forbidden]` expression, `!(...)`, after its message; with values
  in place of some names, the expression with every name follows as a comment,
  and so does the condition of a commented-out line. The other
  parameters, whose domain has no upper bound or is any value, are commented
  out, with a range of a factor of ten around a positive value, within the
  lower bound, to start from, or `(LOW, HIGH)` with the lower bound when it has
  one; lists, paths, text and
  `unlimited` limits are noted, not written. `cost.*` is never written: it
  defines the cost that irace compares.
- `fixed.conf`: the parameters the command line changed when the stub was
  written (but `instance`, `seed`, `output`, `report` and `tuning.*`), read by
  each run with `--config`; the tuned values override them.
- `target-runner`: a shell script that runs the program, by its absolute path,
  with `--config fixed.conf --tuning.print=cost --instance=... --seed=...` and
  the candidate's switches; a bound passed by irace is ignored. It does not run
  on Windows.
- `instances.txt`, `scenario.txt`: stubs, with the program's instance and a
  budget of 1000 runs.
- `configurations.txt`: the current values as a first configuration, `NA` for
  the parameters of the runners that are not the first and for those whose
  condition is false.

The files that exist are kept, so that the user edits them; only
`configurations.txt` is rewritten, each time from `parameters.txt` as it is on
disk (up to a `[global]` or `[forbidden]` section), with each value moved into
its range when it lies outside (`moved` lists them). A parameter of
`parameters.txt` that the program does not have is an error.

`scalar_cost(input, cost, hard_weight) -> double` is the number irace compares:
the problem's `scalar_cost(const Input&, const Cost&)` when one is found by
argument-dependent lookup, else `cost::scalar(cost, hard_weight)`
(`<easylocal/cost/scalar.hpp>`): a number as it is, a `hierarchical` cost as
`hard * weight + soft`, a `lexicographic` one as the sum of each value times
`weight` to the number of values after it. `scalar_cost_available<Input, Cost>`
tells whether either applies; without it, `--tuning.print` fails with status 2.

A program that reads its configuration from the command line itself adds
`easylocal::RunParameters` for the target, under a prefix of its choice:

```cpp
easylocal::RunParameters run;
configuration.add("run", run);                       // --run.target=0
// ... once the Input is loaded:
if (!run.target.empty())
    session.run("sa", easylocal::stop_at(session.read_cost(run.target)));
```

`run.target_cost<Cost>(input)` gives the target as a `std::optional<Cost>`,
empty when none is set, for programs that run a runner or a solver directly;
its errors name the field.

The target stays text until the Input is known, because a problem may read
its costs with its own `read_cost`. A program that runs a runner without a
Session reads it with `easylocal::read_cost<Cost>(input, text)`, which the
Session also uses: the problem's `read_cost` when it has one, else
`cost::from_text`; `easylocal::readable_cost<Input, Cost>` tells whether
either applies.

## Sessions

A `Session` (`<easylocal/app/session.hpp>`) is an app at work on one Input: an
owned Input, the app bound to it, a current solution, a selected move and an
RNG, with the commands that change them. It is how a program runs an app
headless, and the model of an interactive frontend: the TextUI is a view on
it, and a GUI or a web frontend would be another.

```mermaid
flowchart TB
    subgraph app["app: a description"]
        direction LR
        recipes["recipes:<br/>SolutionManager, neighborhood"]
        regs["runner registrations:<br/>algorithm, name, parameters"]
    end
    subgraph session["Session"]
        copy["app (its own copy)"]
        subgraph bound["bound app"]
            direction LR
            sm["SolutionManager"]
            nhe["NeighborhoodExplorer"]
        end
        input[("Input")]
        subgraph state[" "]
            direction LR
            sol["current solution"]
            move["selected move (optional)"]
            rng["RNG (seed)"]
        end
    end
    subgraph fresh["each run(&quot;name&quot;)"]
        direction LR
        fbound["fresh bound app"]
        runner["runner"]
    end
    app -- copied --> copy
    copy -- "bind, again on configure" --> bound
    copy -- "app.run" --> fresh
    bound -. borrows .-> input
    fresh -. borrows .-> input
    fresh -- "result replaces" --> sol
```

| Constructor | |
| --- | --- |
| `Session{app, input, seed}` | a session on `input`, which it owns; another Input is another session |
| `Session{app, std::shared_ptr<const Input>, seed}` | a session on an Input it shares with other owners, without copying it |
| `Session{app[, seed]}` | a session without an Input yet, for frontends that load one (`set_input`, `load_input`) |

| Commands | |
| --- | --- |
| Input | `set_input`, `load_input` (the I/O hooks of [Problem model](problem-model.md#optional-hooks)), `input`, `has_input` |
| Solution | `use_initial_solution`, `use_random_solution(rng)`, `set_solution`, `load_solution`, `save_solution`, `solution`, `is_valid`, `evaluate`, `check()` |
| Move | select with `use_first_move`, `use_next_move`, `use_first_improving_move`, `use_best_move`, `use_random_move(rng)` or `set_move`; then `move_is_valid`, `evaluate_move`, `evaluate_move_fully`, `move_evaluation_matches_full`, `apply_move` |
| Neighborhood | `neighborhood_preview`, `neighborhood_statistics`, `check_neighborhood_costs`, `check_move_independence` (needs `Solution::operator==`), `check_random_move_distribution(rng)` (needs `Move::operator==`) |
| Runners | `runner_names` (the runners and pipelines), `run("name", options...)` (replaces the current solution; options are `with(control, tracer)`), `last_run_effort()`: the evaluations, iterations and termination of the last run, when its algorithm reports them |
| Costs | `read_cost(text)`: a cost written as text, such as a target, by the problem's `read_cost` or `cost::from_text`; `cost_report()`: each cost component on the current solution, in the order of the recipe, as `component_report{name, value, description}`: its `name()` or `#<position>`, its own value without weights, and its `describe(solution)` text, empty without it |
| Parameters | `configuration()`, the app's; `configure(text_overrides)` applies them all or none and, when the cost or the neighborhood changes, rebuilds the bound services |

The commands move the session through these states; a new Input drops the
solution and the move, a new solution or a run drops the move:

```mermaid
stateDiagram-v2
    state "no Input" as empty
    state "Input, no solution" as input
    state "current solution" as solution
    state "solution and selected move" as move
    [*] --> empty: Session{app, seed}
    [*] --> input: Session{app, input, seed}
    empty --> input: set_input, load_input
    input --> solution: use_initial_solution, use_random_solution,<br/>set_solution, load_solution
    solution --> move: use_..._move finds one, set_move
    move --> solution: apply_move, run, configure,<br/>use_..._move finds none
    solution --> solution: run, configure
    solution --> input: set_input
    move --> input: set_input
```

The selections and `run` return `false` when there is nothing to select or no
runner with that name, and are `[[nodiscard]]`. `run` uses fresh services and
the current runner parameters, like `app.run`, and executes on the calling
thread. The REST service gives every run a Session of its own and calls `run`
on a worker thread, with `with(control)` for progress and cancellation. The
TextUI, which keeps reading its Session while a runner works, runs it with
`app.run("name", ...)` on copies of the app, Input and solution instead.

## Design choices

- **Fresh services per run.** `app.run` binds new services, so concurrent runs
  share only the immutable Input and no locking is needed.
- **Registration by class and name.** The class is the key, the name tells
  registrations of the same class apart and is what tools show.
- **One way to run by name.** `app.run("name", ...)` is the single place where
  a name selects a runner; the TextUI, the REST service and `Session` use it.
- **Adapters are optional components** outside Core, meant to become separate
  projects once the API is stable.
