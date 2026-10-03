# 10. Testing your components

`<easylocal/testing.hpp>` checks that your components honour their contracts.
All checks share one fixture: a small Input, a valid Solution and the
SolutionManager built on them.

<!-- snippet: tutorial/checks.cpp:fixture -->
```cpp
namespace elt = easylocal::testing;
const elt::fixture<tutorial::TourManager> tsp{
    tutorial::five_cities(),
    tutorial::Tour{{0, 1, 2, 3, 4}},
};
```

Each check names the type under test and builds it from the fixture, as an app
would: a component or a delta evaluator from the Input, a NeighborhoodExplorer
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
    // No delta evaluator: the check uses the component's own delta_evaluate.
    elt::check_delta_evaluator<TwoOptExplorer, TourLengthWithDelta>(tsp));
```

| Check | Verifies |
| --- | --- |
| `check_solution_manager(f)` | the fixture, initial and random solutions are valid |
| `check_cost_component<Component>(f)` | evaluating the same solution twice gives equivalent values |
| `check_neighborhood<NHE>(f)` | enumerated and sampled moves are valid and keep the solution valid |
| `check_delta_evaluator<NHE, Component, Delta>(f)` | `value + delta` equals the full re-evaluation after each valid move |

For a co-located delta (chapter 4) omit the third type, as for
`TourLengthWithDelta` above: the check then uses the component's own
`delta_evaluate`.

Without a Solution, `fixture<SM>{input}` uses the SolutionManager's initial
solution. A third argument sets the sampling limits
(`{.random_samples = 16, .max_enumerated_moves = 256}`), and a second template
argument replaces `==` for comparing values, for instance with a tolerance. A
component that cannot be built from the Input alone is passed as an object:
`check_cost_component(f, TourLength{...})`; the same holds for
neighborhoods and delta evaluators. `testing.hpp` is not part of the Core
umbrella: include it from your test executables.

For a whole composed problem, `easylocal::check(app, input)` runs the same
checks against an app ([chapter 12](12-checking.md)).

## See also

- [Testing](../reference/testing.md).

## Next steps

[Chapter 11](11-apps-and-tools.md) packages the problem as an application.
