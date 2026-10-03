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
            {.samples_per_temperature = 50});

auto piped_application = el::app("tsp") | sm | nhe
    | el::runner<runners::FirstImprovement>("fi")
    | el::runner<runners::SimulatedAnnealing<Classic>>(
        "sa",
        {.samples_per_temperature = 50});
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
const Tour& session_tour = session.solution();
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

The selections and `run` are `[[nodiscard]]`: the compiler warns when their
result is ignored, since a misspelt runner name would otherwise run nothing,
silently.

Each `run` builds fresh services from the app, with the runner's current
parameters, so runs share nothing but the immutable Input.

## See also

- [Apps and tools](../reference/app-and-tools.md): the complete app and Session
  interfaces.

## Next steps

[Chapter 12](12-tester.md) explores the app in the interactive tester.
