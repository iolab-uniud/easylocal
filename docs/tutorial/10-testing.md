# 10. Testing your components

`<easylocal/testing.hpp>` checks that your components honour their contracts.
Each check takes a small fixture: an Input, a Solution and the types under test.

<!-- snippet: tutorial/checks.cpp:fixtures -->
```cpp
struct TspCheckData
{
    static tutorial::Tsp instance()
    {
        return tutorial::five_cities();
    }
    static tutorial::Tour solution(const tutorial::Tsp&)
    {
        return tutorial::Tour{{0, 1, 2, 3, 4}};
    }
};

struct TourManagerCheck : TspCheckData
{
    using solution_manager = tutorial::TourManager;
};

struct TourLengthCheck : TspCheckData
{
    using solution_manager = tutorial::TourManager;
    using component = tutorial::TourLength;
};

struct TwoOptCheck : TspCheckData
{
    using neighborhood = tutorial::TwoOptExplorer;
};

struct TwoOptDeltaCheck : TspCheckData
{
    using neighborhood = tutorial::TwoOptExplorer;
    using component = tutorial::TourLength;
    using delta_evaluator = tutorial::TwoOptLengthDelta;
};
```

<!-- snippet: tutorial/checks.cpp:run-checks -->
```cpp
return easylocal::testing::run_checks(
    easylocal::testing::check_solution_manager<TourManagerCheck>(),
    easylocal::testing::check_cost_component<TourLengthCheck>(),
    easylocal::testing::check_neighborhood<TwoOptCheck>(),
    easylocal::testing::check_delta_evaluator<TwoOptDeltaCheck>());
```

| Check | Verifies |
| --- | --- |
| `check_solution_manager<T>` | the fixture, initial and random solutions are valid |
| `check_cost_component<T>` | evaluating the same solution twice gives equivalent values |
| `check_neighborhood<T>` | enumerated and sampled moves are valid and keep the solution valid |
| `check_delta_evaluator<T>` | `value + delta` equals the full re-evaluation after the move, for a sample move (`T::move` or one taken from the neighborhood) |

Fixtures may also set `random_samples` and `max_enumerated_moves`, provide
`make_*` factories for non-default construction, and an `equivalent` function
for tolerance-based comparisons. `testing.hpp` is not part of the Core umbrella:
include it from your test executables.

For a whole composed problem, `easylocal::check(app, input)` runs the same
checks against an app ([chapter 12](12-checking.md)).

## See also

- [Testing](../reference/testing.md).

## Next steps

[Chapter 11](11-apps-and-tools.md) packages the problem as an application.
