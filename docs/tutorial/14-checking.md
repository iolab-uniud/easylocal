# 14. Checking a composed problem

Chapter 10 checked each component on its own. Once the components are composed
into an app, two further levels of checking are available.

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

`easylocal::check(app, input)` builds the app for the Input, takes the initial
solution (or the one passed as third argument) and verifies the following,
from that solution and from random ones, as the checks of chapter 10 do; a
last argument, `testing::check_options`, sets the samples, the random
solutions, the seed of its random draws and the tolerance of its cost
comparisons (chapter 10), which forgives the rounding errors of a
floating-point cost updated by deltas:

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

The report converts to `bool`, lists the failures (`failures()`), which name
the move and the solution it starts from, and its `composition()` says how many
components, neighborhoods, delta bindings and registrations the app composes.
`print_report` prints both, and `testing::run_checks` takes the report with
those of chapter 10. A hook that throws fails its check.

## Session checks: the neighborhood of a solution

`check` looks at one solution and a sample of moves. A Session (chapter 11)
checks the whole neighborhood of its current solution:

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

The last two compare values: `check_move_independence` compares the solution
after each move with the others, as the search does (the SolutionManager's
`equal`, or the solution's `==`), and `check_random_move_distribution` compares
sampled moves with enumerated ones. The first compares only with those of the
same hash, or of the same cost when the costs are ordered, as the tour lengths
are; the second only with those of the same cost.
So far the tutorial's types had no
equality: to use these two checks, add it. Without it, everything else works:
calling one of the two does not compile, and the interactive tester does not
offer them (`D` and `U`), while its other neighborhood diagnostics stay.

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

A member `bool operator==(const Tour&) const = default;` inside each struct is
equivalent and shorter, when you can change the type; free functions, as here,
also work for types you cannot change. C++20 derives `!=` from `==`.

## See also

- [Testing](../reference/testing.md) for component checks.
- [Apps and tools](../reference/app-and-tools.md).

## Next steps

[Chapter 15](15-rest.md) offers the same searches over HTTP.
