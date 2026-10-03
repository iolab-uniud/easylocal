# 11. Applications

A runner (chapter 5) is one algorithm on one problem. An **app** packages the
whole problem: a name, the SolutionManager, the neighborhood, and the runners
available on it, each under a name of its own. A **Session** puts an app to
work on one Input. The interactive tester, the checks and the REST service of
the next chapters all start from an app.

## The app

<!-- snippet: tutorial/main.cpp:app -->
```cpp
auto application =
    el::app("tsp")
        .with_solution_manager(sm)
        .with_neighborhood(nhe)
        .with_runner<runners::FirstImprovement>("fi")
        .with_runner<runners::SimulatedAnnealing<Classic>>(
            "sa",
            {.temperature = {.samples_per_temperature = 50}});

auto piped_application = el::app("tsp") | sm | nhe
    | el::runner<runners::FirstImprovement>("fi")
    | el::runner<runners::SimulatedAnnealing<Classic>>(
        "sa",
        {.temperature = {.samples_per_temperature = 50}});
```

- An app is a description, like the recipes it holds: it builds nothing and
  runs nothing by itself.
- A runner is registered by its algorithm class and a name, optionally with its
  parameters (`parameters_type`). They are stored in the app and can be changed
  later with `runner_config<Algorithm>()` or `runner_config<Algorithm>("name")`.
- Simulated Annealing is registered with its temperature policy's parameters:
  `runners::SimulatedAnnealing<Classic>` takes `ClassicParameters`.
- The two spellings are equivalent: `with_*` calls, or pipes with
  `el::runner<Algorithm>(name, parameters)` for each registration.

## The Session

A Session is the app at work on one Input. It owns the Input, builds the
services for it, and keeps a *current solution*, which you change by hand, one
move at a time, or by running a registered runner:

<!-- snippet: tutorial/main.cpp:session -->
```cpp
// The app on one Input, which the session owns, with a seeded RNG.
el::Session session{application, tsp, /* seed */ 2026};
session.use_initial_solution();

// A step by hand: select the first improving move, if any, and apply it.
if (session.use_first_improving_move())
    session.apply_move();

// A registered runner, by name, from the current solution; false means no
// runner has that name.
if (!session.run("sa")) // receives the session's RNG
    return 1;
const double session_cost = session.evaluate();
const Tour session_tour = session.solution(); // a copy: the session goes on
```

- `el::Session{application, tsp, seed}` copies the app and the Input: the
  session owns both, so it can outlive the variables it was built from. The
  seed starts the RNG that the session gives to stochastic runners, so the same
  seed repeats the same session. For another Input, build another Session.
- `use_initial_solution()` makes the initial tour the current solution
  (`use_random_solution(rng)` makes a random one).
- A step by hand takes two calls: `use_first_improving_move()` *selects* the
  first move that improves the current solution, and `apply_move()` applies
  it. The selection returns `false` when there is no such move, on a local
  optimum for example. `use_first_move`, `use_next_move`, `use_best_move` and
  `use_random_move` select in other ways; `evaluate_move()` gives the cost the
  selected move would lead to.
- `run("sa")` runs the runner registered as `"sa"` from the current solution,
  and makes its result the current solution. It returns `false` when no runner
  has that name.
- `evaluate()` and `solution()` read the current cost and solution.
  `solution()` returns a reference to the session's solution, which the next
  command may replace: copy it to keep it, as here.

The selections and `run` are `[[nodiscard]]`: the compiler warns when their
result is ignored, since a misspelt runner name would otherwise run nothing,
silently.

Each `run` builds fresh services from the app, with the runner's current
parameters, so runs share nothing but the immutable Input.

### Parameters and targets

The parameters of an app are those of its parts, by path:
`runners.<name>.*` for each runner's algorithm, `cost.*` for the weights of
its cost expression and `neighborhood.*` for the biases of a neighborhood
union. The session changes them, and stops a run at a target cost:

<!-- snippet: tutorial/main.cpp:session-parameters -->
```cpp
// The app's parameters by path; they change this session's app only.
const std::array changes{
    el::config::text_override{"runners.sa.temperature.cooling_rate", "0.9"}};
if (!session.configure(changes)) // all or none, checked
    return 1;

// A run that stops at the first tour of length 26 or less.
session.use_initial_solution();
if (!session.run("sa", el::stop_at(session.read_cost("26"))))
    return 1;
```

- `configure(changes)` applies `path = value` changes as chapter 9 does: all
  of them, checked, or none. They change the session's own copy of the app;
  when they touch the cost or the neighborhood, the session rebuilds its
  services, so `evaluate()` and the moves follow. `configuration()` lists the
  parameters with their current values.
- `read_cost("26")` reads a cost written as text: a number, `[hard, soft]` for
  a hierarchical cost, or the problem's own notation when it provides
  `read_cost(const Tsp&, std::string_view)`. `el::stop_at(cost)` makes the run
  stop at the first solution that reaches it, a known optimum or a lower
  bound for example.

A program that takes these from the command line adds the app's parameters to
its set before building the Session, which copies the app, and the run's
target as `el::RunParameters`:

```cpp
el::RunParameters run;
el::config::parameter_set configuration;
configuration.add(application.configuration()); // --runners.sa.temperature.*
configuration.add("run", run);                   // --run.target
// load_and_apply(argc, argv, configuration), then:
el::Session session{application, tsp, /* seed */ 2026};
session.use_initial_solution();
const bool ran = run.target.empty()
    ? session.run("sa")
    : session.run("sa", el::stop_at(session.read_cost(run.target)));
```

The target stays text until the Input is loaded, since the problem may read
its costs in its own way. The TextUI (chapter 12) and the REST service
(chapter 14) take the same paths and targets.

## See also

- [Apps and tools](../reference/app-and-tools.md): the complete app and Session
  interfaces.

## Next steps

[Chapter 12](12-tester.md) explores the app in the interactive tester.
