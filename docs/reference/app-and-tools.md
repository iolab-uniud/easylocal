# Apps and tools

`<easylocal/app/app.hpp>`, `app/check.hpp`, `app/tester.hpp`; adapters
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
| `run<A>(input, solution, args...)`, `run_at<I>(...)` | run on fresh services |
| `for_input(input)` | a reusable *runtime*: services built once |
| `make_runner<A>([name])`, `make_solver<Solver, A>([name,] config)` | standalone runner or solver |
| `for_each_runner_registration[_indexed](visitor)` | iterate registrations (adapters) |

## Tools

| Tool | Purpose |
| --- | --- |
| `check(app, input[, solution]) -> app_check_report` | contract checks of the composed problem; `print_report` |
| `Tester{app}` | headless driver: `set_input` / `load_input`, `use_initial_solution` / `use_random_solution`, `set_solution` / `load_solution` / `save_solution`, `evaluate`, `check`, moves (`use_first_move`, `use_next_move`, `use_first_improving_move`, `use_best_move`, `use_random_move`), `run_runner(name)` |
| `tui::run(tester, options)`, `tui::run_launcher(options, apps...)` | interactive terminal tester (TUI component, FTXUI) |
| `rest::blueprint(prefix, app, codec, options)` | Crow blueprint: asynchronous runs, status, cancellation, solutions (REST component); see [REST](../rest.md) |

The tools run registered runners without extra arguments, hence without an
RNG: only deterministic runners can be used through them for now.

## Design choices

- **Fresh services per run.** `app.run` materializes new services, so
  concurrent runs share only the immutable Input and no locking is needed.
- **Registration by class and name.** The class is the key, the name tells
  registrations of the same class apart and is what tools show.
- **Adapters are optional components** outside Core, meant to become separate
  projects once the API is stable.
