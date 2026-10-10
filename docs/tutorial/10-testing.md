# 10. Testing your components

Before trusting search results, check that moves preserve valid solutions and
deltas agree with full evaluation. `<easylocal/testing.hpp>` supplies these
checks so you can run them in your test suite. They share a fixture containing
a small Input, a valid Solution and its SolutionManager:

<!-- snippet: tutorial/checks.cpp:fixture -->
```cpp
namespace elt = easylocal::testing;
// Not the identity tour: on 0, 1, 2, 3, 4 position k holds city k, and a
// delta that mixes up positions and cities would still be right.
const elt::fixture<tutorial::TourManager> tsp{
    tutorial::five_cities(),
    tutorial::Tour{{0, 2, 4, 1, 3}},
};
```

Each check names the type under test and builds it from the fixture, as an app
would: a component or a delta cost component from the Input, a NeighborhoodExplorer
from the SolutionManager.

<!-- snippet: tutorial/checks.cpp:run-checks -->
```cpp
using namespace tutorial;
return elt::run_checks(
    elt::check_solution_manager(tsp),
    elt::check_cost_component<TourLength>(tsp),
    elt::check_neighborhood<SwapExplorer>(tsp),
    elt::check_neighborhood<TwoOptExplorer>(tsp),
    elt::check_delta_cost_component<TwoOptExplorer, TourLength, TwoOptLengthDelta>(
        tsp),
    // No delta cost component: the check uses the component's own delta_evaluate.
    elt::check_delta_cost_component<TwoOptExplorer, TourLengthWithDelta>(tsp));
```

| Check | Verifies |
| --- | --- |
| `check_solution_manager(f)` | the fixture, initial and random solutions are valid |
| `check_cost_component<Component>(f)` | evaluating the same solution twice gives equivalent values |
| `check_neighborhood<NHE>(f)` | enumerated and sampled moves are valid, keep the solution valid and change it; random moves are enumerated ones, drawn from the generator given |
| `check_delta_cost_component<NHE, Component, Delta>(f)` | `value + delta` equals the full re-evaluation after each valid move |

Move checks use both the fixture's solution and random solutions followed by
random walks. This helps catch deltas that work on one tour but fail on another.

The fixture deliberately uses a shuffled tour. On `0, 1, 2, 3, 4`, city
identifiers equal their positions, so a delta could incorrectly use
`distance[i][j]` instead of `distance[order[i]][order[j]]` and still pass.

Failures identify the move, starting solution and disagreeing values. The
report prints up to three failures per check. A hook that throws fails its
check without stopping the remaining checks.

For a co-located delta (chapter 4) omit the third type, as for
`TourLengthWithDelta` above: the check then uses the component's own
`delta_evaluate`.

`fixture<SM>{input}` uses the initial solution when none is supplied. A third
argument controls sampling, random walks and seed, for example
`{.random_samples = 16, .max_enumerated_moves = 256, .random_solutions = 4,
.walk_length = 8, .seed = 7}`.

Comparisons use `testing::approximately`, with relative and absolute
floating-point tolerances of 1e-9 by default. This allows for rounding drift
from incremental updates. Other values are compared exactly. Set
`{.tolerance = {.relative = 0, .absolute = 0}}` for exact comparison, or provide
a second fixture template argument to replace the comparison policy.

If a component needs more than the Input to construct it, pass an object:
`check_cost_component(f, TourLength{...})`. Explorers and deltas support the
same form. Include `testing.hpp` in your test executable; the Core umbrella
provides the options and reports for `check(app)`, but not these individual
checks.

For bugs that appear only during a longer search, enable
[`EASYLOCAL_VERIFY_DELTAS`](04-delta-evaluation.md#checking-the-deltas-during-a-run)
in a debug build. It compares incremental and full evaluation after every
committed move.

For a whole composed problem, `easylocal::check(app, input)` runs the same
checks against an app ([chapter 14](14-checking.md)).

## See also

- [Testing](../reference/testing.md).

## Next steps

[Chapter 11](11-apps-and-tools.md) packages the problem as an application.
