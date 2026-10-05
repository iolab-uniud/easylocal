# Testing

`<easylocal/testing.hpp>` (`easylocal::testing`), not part of the Core umbrella

Contract checks for user components. Each check takes a fixture and returns a
`check_report`; `run_checks(reports...)` prints them and returns a process exit
code.

```cpp
const easylocal::testing::fixture<TourManager> f{input, solution};
return easylocal::testing::run_checks(
    easylocal::testing::check_solution_manager(f),
    easylocal::testing::check_neighborhood<TwoOptExplorer>(f),
    easylocal::testing::check_delta_evaluator<TwoOptExplorer, TourLength, TwoOptDelta>(f));
```

## Fixture

`fixture<SM, Equivalent = std::equal_to<>>` owns an Input, a Solution and the
SolutionManager `SM{input}`; it is neither copyable nor movable.

| Constructor | Meaning |
| --- | --- |
| `fixture{input, solution[, options]}` | the given Solution (checked for validity) |
| `fixture{input[, options]}` | the SolutionManager's `initial_solution()` |

`check_options` holds `random_samples` (default 32), `max_enumerated_moves`
(default 1024) and `seed`: the randomized checks draw from a `std::mt19937_64`
seeded with it, so the same seed repeats the same draws (`check(app, ...)` uses
the default seed). `deterministic_rng`, which cycles through a few fixed values,
is for unit tests that script the draws, not for sampling. `Equivalent` compares values (component values, `value + delta`
against the full evaluation); replace it for tolerance-based comparisons.

## Checks

| Check | Verifies |
| --- | --- |
| `check_solution_manager(f)` | input binding; the fixture, initial and random solutions are valid |
| `check_cost_component<C>(f)` | evaluating the same solution twice gives equivalent values |
| `check_neighborhood<N>(f)` | enumerated and sampled moves are valid; applying them keeps the solution valid |
| `check_delta_evaluator<N, C, D>(f)` | for each valid enumerated (or, failing that, sampled) move, `value + delta` equals the component value after the move |
| `check_delta_evaluator<N, C>(f)` | the same, with the co-located `C::delta_evaluate` |

The type form builds the object under test as an app does: components and delta
evaluators from the Input (or default-constructed), NeighborhoodExplorers from
the fixture's SolutionManager. For other constructors pass the objects instead:
`check_cost_component(f, c)`, `check_neighborhood(f, n)`,
`check_delta_evaluator(f, n, c[, d])`.

For a composed problem, `easylocal::check(app, input[, solution])`
(`<easylocal/app/check.hpp>`) runs the same kinds of checks on an app.

## Design choices

- **Checks are contracts, not unit tests.** They verify the laws the framework
  relies on (validity preservation, the delta law), so a component that passes
  them can be used by every algorithm.
- **Deterministic.** Randomized checks use a fixed internal generator.
- **One fixture, many checks.** The data is written once; each check names
  only the types under test.
