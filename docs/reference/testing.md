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

`fixture<SM, Equivalent = approximately>` owns an Input, a Solution and the
SolutionManager `SM{input}`; it is neither copyable nor movable.

| Constructor | Meaning |
| --- | --- |
| `fixture{input, solution[, options]}` | the given Solution (checked for validity) |
| `fixture{input[, options]}` | the SolutionManager's `initial_solution()` |

`check_options` holds `random_samples` (default 32), `max_enumerated_moves`
(default 1024), `seed` and `tolerance`: the randomized checks draw from a
`std::mt19937_64` seeded with `seed`, so the same seed repeats the same draws.
`deterministic_rng`, which cycles through a few fixed values, is for unit tests
that script the draws, not for sampling.

`Equivalent` compares values (component values, `value + delta` against the
full evaluation). The default, `testing::approximately` (the type
`cost::tolerance`), uses the options' `tolerance`: two floating-point values are
equal when they differ by at most `max(absolute, relative * max(|a|, |b|))`,
both 1e-9 by default, integers compare exactly, lexicographic, hierarchical
and pareto costs level by level, other values with `==`. A cost updated by
deltas accumulates rounding errors that a full evaluation does not have (on a
TSP with decimal distances, `value + delta` differs from the new length in the
last bits): with an exact comparison the delta check would report them.
`{.tolerance = {.relative = 0, .absolute = 0}}` compares exactly, as does
`fixture<SM, std::equal_to<>>`; another `Equivalent` replaces the comparison.

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

For a composed problem, `easylocal::check(app, input[, solution][, options])`
(`<easylocal/app/check.hpp>`) runs the same kinds of checks on an app, with the
`seed` and the `tolerance` of the options.

## Design choices

- **Checks are contracts, not unit tests.** They verify the laws the framework
  relies on (validity preservation, the delta law), so a component that passes
  them can be used by every algorithm.
- **Deterministic.** Randomized checks use a fixed internal generator.
- **A tolerance in the checks, exact relations in the search.** Rounding
  errors are not bugs, so the checks forgive them; the search compares costs
  exactly unless the cost expression says otherwise (`cost::approximately`,
  see [Cost](cost.md#cost-semantics)).
- **One fixture, many checks.** The data is written once; each check names
  only the types under test.
