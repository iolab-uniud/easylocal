# 14. Checking a composed problem

Chapter 10 tested components individually. Now check that they work together:
`check` validates the app, while Session checks inspect the neighborhood of
a particular solution. Both are also available in the interactive tester.

## `check`: the app's contracts

<!-- snippet: tutorial/main.cpp:check -->
```cpp
const auto report = el::check(application, tsp); // also: check(app, input, solution)
el::print_report(std::cout, report);
if (!report)
    return 1;
```

```text
EasyLocal tsp check: 503 checks passed
composition: solution_managers=1, cost_components=1, neighborhoods=1, delta_bindings=1, runner_registrations=2
```

`easylocal::check(app, input)` builds the services and tests them from the
initial solution, or one supplied as the third argument. It also tries random
solutions, as in chapter 10.

An optional final `testing::check_options` argument controls sample counts,
random walks, seed and comparison tolerance. The default tolerance allows for
floating-point rounding in incremental costs. The report covers:

| Check | What fails it |
| --- | --- |
| input binding | a service bound to a different Input |
| check solution | the solution is not valid for the SolutionManager |
| repeat evaluation | evaluating the same solution twice gives non-equivalent values |
| random solution | `random_solution` returns an invalid solution |
| neighborhood move validity, move application | an enumerated move that is not valid, or that breaks the solution |
| incremental evaluation | a delta disagreeing with the full re-evaluation |
| delta sign | with a `compare` at the root of the cost expression, a `cost::delta` whose sign disagrees with it (chapter 2) |
| random proposal, random proposal application | a sampled move that is not valid, or that breaks the solution |
| random move in the neighborhood, availability, reproducibility | with moves that compare, a sampled move that the enumeration does not contain, a `random_move` that finds nothing while the neighborhood has moves, or one that does not draw from the generator given |
| registration names | a runner or pipeline name that is empty, repeated, or made of other characters than letters, digits, `_` and `-` |
| runner parameters | a registered runner whose `parameters_type` is not a parameter block (chapter 7), so no frontend can change it; an empty one has nothing to configure |
| runner configuration, runner construction | invalid registered parameters, or a runner that cannot be built |
| app configuration | invalid parameters of the app, or pipeline stages without distinct names |
| parameter domain | a parameter of the app that declares no domain (chapter 9): give it a range, `one_of`, or `easylocal::unlimited` for any value |

Test the report as a `bool` to check success. `failures()` gives diagnostics,
including the starting solution and move for move-related failures.
`composition()` counts components, neighborhoods, delta bindings and
registrations.

Use `print_report` for readable output or pass the report to
`testing::run_checks` alongside the component checks from chapter 10.
Exceptions from hooks are reported as check failures.

## Session checks: the neighborhood of a solution

While `check` uses sampling limits, a Session can inspect the entire
neighborhood of its current solution. This is useful when investigating a
specific tour:

<!-- snippet: tutorial/main.cpp:session-checks -->
```cpp
// Each check enumerates the neighborhood of the current solution and
// returns a struct of counters (Session::..._result).

// The delta evaluation of each move against the full evaluation of the
// solution it leads to: moves (enumerated), mismatches (the two costs
// differ), invalid (moves that are not valid or lead to an invalid
// solution).
const auto costs = session.check_neighborhood_costs();

// What each move does to the solution: moves, null_moves (moves that leave
// it unchanged), repeated_states (moves that lead to a solution an earlier
// move reached), invalid.
const auto independence = session.check_move_independence();

// random_move against the enumeration: neighborhood_size (valid enumerated
// moves), samples (draws, 20 per move by default), out_of_neighborhood
// (draws that return no move or one the enumeration does not contain),
// unseen (enumerated moves never drawn), min_frequency and max_frequency
// (how often the least and the most drawn moves came up).
const auto sampling = session.check_random_move_distribution();

if (costs.mismatches != 0 || costs.invalid != 0 || sampling.out_of_neighborhood != 0)
    return 1;
```

| Check | Result | What fails it |
| --- | --- | --- |
| `check_neighborhood_costs()` | `moves`, `mismatches`, `invalid` | a move whose delta evaluation disagrees with the full evaluation, or an invalid enumerated move |
| `check_move_independence()` | `moves`, `null_moves`, `repeated_states`, `invalid` | moves that change nothing (*null moves*), and different moves that lead to the same solution |
| `check_random_move_distribution([rounds_per_move])` | `neighborhood_size`, `samples`, `out_of_neighborhood`, `unseen`, `min_frequency`, `max_frequency` | a sampled move that is not in the enumerated neighborhood; moves never sampled (`unseen`) hint at a biased `random_move` |

All three need the moves to be enumerable; the last one also needs
`random_move`. `check_neighborhood_costs(tolerance)` compares the costs within
a `cost::tolerance` (1e-9 relative and absolute by default; `{0, 0}` compares
exactly). The interactive tester of chapter 13 runs the same checks from
its Move page: `C`, `D` and `U`.

The last two checks need equality. `check_move_independence` compares resulting
solutions using the SolutionManager's `equal` or the Solution's `==`.
`check_random_move_distribution` compares sampled and enumerated moves.

To reduce comparisons, independence checks group solutions by hash, or by cost
when costs are ordered. Distribution checks compare moves with the same cost.
Add equality to the tutorial's types to enable them:

<!-- snippet: tutorial/tsp.hpp:equality -->
```cpp
// Equality of solutions and moves, for the neighborhood checks of a Session
// (chapter 14).
inline bool operator==(const Tour& a, const Tour& b)
{
    return a.order == b.order;
}

inline bool operator==(const SwapCities& a, const SwapCities& b)
{
    return a.i == b.i && a.j == b.j;
}

inline bool operator==(const TwoOpt& a, const TwoOpt& b)
{
    return a.i == b.i && a.j == b.j;
}
```

When you can edit a type, a defaulted member such as
`bool operator==(const Tour&) const = default;` is shorter. Free functions
also work for types you cannot change. C++20 derives `!=` from `==`.

Without equality, these two checks do not compile and the tester hides their
`D` and `U` commands. Other neighborhood diagnostics remain available.

## See also

- [Testing](../reference/testing.md) for component checks.
- [Apps and tools](../reference/app-and-tools.md).

## Next steps

[Chapter 15](15-rest.md) offers the same searches over HTTP.
