# Apps and tools

`<easylocal/app/app.hpp>`, `app/check.hpp`, `app/session.hpp`; adapters
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

| Member | Purpose |
| --- | --- |
| `name()` | the app name |
| `runner_config<A>()`, `runner_config<A>("name")` | the stored parameters |
| `runner_name<A>()` | the registered name |
| `bind(input)` | the *bound app*: services built once for `input`, which it borrows (a temporary Input is rejected) |
| `run("name", input, solution, rng, options...)` | run the runner registered under a name; `std::optional<named_run_result>`, empty for an unknown name |
| `run<A>(input, solution, args...)`, `run_at<I>(...)` | run by algorithm or by registration index |
| `run_at_with_rng<I>(input, solution, rng, options...)` | run by index; `rng` goes to stochastic algorithms only |
| `make_runner<A>([name])`, `make_solver<Solver, A>([name,] config)` | standalone runner or solver |
| `for_each_runner_registration[_indexed](visitor)` | iterate registrations (adapters) |

The `run` members bind the app to the Input for that run only, with the
current runner parameters. A bound app (`bind`) offers
`solution_manager()`, `neighborhood()`, `input()`, `runner<A>()`,
`run<A>(solution, args...)` and `run_at<I>(...)` on services built once.

`named_run_result{solution, cost}` keeps what every runner result provides
(`search_result_for`): a runner chosen by name may be any algorithm, built-in
or your own, each with its own result type.

## Tools

| Tool | Purpose |
| --- | --- |
| `check(app, input[, solution]) -> app_check_report` | contract checks of the composed problem; `print_report` |
| `tui::run(app, options)`, `tui::run_launcher(options, apps...)` | interactive terminal tester (TUI component, FTXUI); `tui::options` |
| `rest::blueprint(prefix, app, codec, options)` | Crow blueprint: asynchronous runs, status, cancellation, solutions (REST component); see [REST](../rest.md) |

Tools give stochastic runners an RNG they own, from a configurable seed: the
TextUI `seed` option (also editable on its Run page) and REST's per-run `seed`
(default `blueprint_options::seed + run id`). `run("name", ...)` passes the RNG
to the algorithm only if it takes one, so deterministic runners ignore it.

## Sessions

A `Session` (`<easylocal/app/session.hpp>`) is an app at work on one Input: an
owned Input, the app bound to it, a current solution, a selected move and an
RNG, with the commands that change them. It is how a program runs an app
headless, and the model of an interactive frontend: the TextUI is a view on
it, and a GUI or a web frontend would be another.

| Constructor | |
| --- | --- |
| `Session{app, input, seed}` | a session on `input`, which it owns; another Input is another session |
| `Session{app[, seed]}` | a session without an Input yet, for frontends that load one (`set_input`, `load_input`) |

| Commands | |
| --- | --- |
| Input | `set_input`, `load_input` (I/O hooks), `input`, `has_input` |
| Solution | `use_initial_solution`, `use_random_solution(rng)`, `set_solution`, `load_solution`, `save_solution`, `solution`, `is_valid`, `evaluate`, `check()` |
| Move | select with `use_first_move`, `use_next_move`, `use_first_improving_move`, `use_best_move`, `use_random_move(rng)` or `set_move`; then `move_is_valid`, `evaluate_move`, `evaluate_move_fully`, `move_evaluation_matches_full`, `apply_move` |
| Neighborhood | `neighborhood_preview`, `neighborhood_statistics`, `check_neighborhood_costs`, `check_move_independence` (needs `Solution::operator==`), `check_random_move_distribution(rng)` (needs `Move::operator==`) |
| Runners | `runner_names`, `run("name")` (replaces the current solution) |

The selections and `run` return `false` when there is nothing to select or no
runner with that name, and are `[[nodiscard]]`. `run` uses fresh services and
the current runner parameters, like `app.run`, and executes on the calling
thread; the TextUI runs runners on a worker thread instead, with
`app.run("name", ...)` on a copy of the app and `with(control)` for progress
and cancellation.

## Design choices

- **Fresh services per run.** `app.run` binds new services, so concurrent runs
  share only the immutable Input and no locking is needed.
- **Registration by class and name.** The class is the key, the name tells
  registrations of the same class apart and is what tools show.
- **One way to run by name.** `app.run("name", ...)` is the single place where
  a name selects a runner; the TextUI, the REST service and `Session` use it.
- **Adapters are optional components** outside Core, meant to become separate
  projects once the API is stable.
