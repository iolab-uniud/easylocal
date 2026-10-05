# 10. Testing your components

`<easylocal/testing.hpp>` checks that your components honour their contracts.
All checks share one fixture: a small Input, a valid Solution and the
SolutionManager built on them.

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
    elt::check_delta_evaluator<TwoOptExplorer, TourLength, TwoOptLengthDelta>(tsp),
    // No delta cost component: the check uses the component's own delta_evaluate.
    elt::check_delta_evaluator<TwoOptExplorer, TourLengthWithDelta>(tsp));
```

| Check | Verifies |
| --- | --- |
| `check_solution_manager(f)` | the fixture, initial and random solutions are valid |
| `check_cost_component<Component>(f)` | evaluating the same solution twice gives equivalent values |
| `check_neighborhood<NHE>(f)` | enumerated and sampled moves are valid, keep the solution valid and change it; random moves are enumerated ones, drawn from the generator given |
| `check_delta_evaluator<NHE, Component, Delta>(f)` | `value + delta` equals the full re-evaluation after each valid move |

The move checks start from the fixture's Solution and from a few random
solutions (`random_solution`, then some random moves): a delta can be right on
one solution and wrong on the others. The fixture above is not the identity
tour for the same reason: on `0, 1, 2, 3, 4` position `k` holds city `k`, so a
delta that reads `distance[i][j]` where it should read
`distance[order[i]][order[j]]` would pass. A failure names the move, the
solution it starts from and the two values that disagree; the failures of one
check are printed together, three of them, and a hook that throws fails the
check instead of stopping the program.

For a co-located delta (chapter 4) omit the third type, as for
`TourLengthWithDelta` above: the check then uses the component's own
`delta_evaluate`.

Without a Solution, `fixture<SM>{input}` uses the SolutionManager's initial
solution. A third argument sets the sampling limits, the random solutions and
the seed of the random draws (`{.random_samples = 16, .max_enumerated_moves =
256, .random_solutions = 4, .walk_length = 8, .seed = 7}`) and the tolerance of
the comparisons. Values are compared with
`testing::approximately`: floating-point values within a relative and an
absolute tolerance (1e-9 by default), since a length updated by deltas differs
from the full one in the last bits, and the others exactly;
`{.tolerance = {.relative = 0, .absolute = 0}}` compares exactly, and a second
template argument replaces the comparison. A
component that cannot be built from the Input alone is passed as an object:
`check_cost_component(f, TourLength{...})`; the same holds for
neighborhoods and delta cost components. `testing.hpp` is not part of the Core
umbrella: include it from your test executables.

For a whole composed problem, `easylocal::check(app, input)` runs the same
checks against an app ([chapter 13](13-checking.md)).

## See also

- [Testing](../reference/testing.md).

## Next steps

[Chapter 11](11-apps-and-tools.md) packages the problem as an application.
