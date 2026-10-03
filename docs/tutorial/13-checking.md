# 13. Checking a composed problem

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
EasyLocal tsp check: 56 checks passed
coverage: solution_managers=1, cost_components=1, neighborhood_graphs=1, delta_bindings=1, runner_registrations=2
```

`easylocal::check(app, input)` builds the app for the Input, takes the initial
solution (or the one passed as third argument) and verifies:

| Check | What fails it |
| --- | --- |
| input binding | a service bound to a different Input |
| check solution | the solution is not valid for the SolutionManager |
| repeat evaluation | evaluating the same solution twice gives non-equivalent values |
| neighborhood move validity, move application | an enumerated move that is not valid, or that breaks the solution |
| incremental evaluation | a delta disagreeing with the full re-evaluation |
| random proposal, random proposal application | a sampled move that is not valid, or that breaks the solution |
| runner configuration, runner construction | invalid registered parameters, or a runner that cannot be built |

The report converts to `bool`, lists the failures (`failures()`), and its
`coverage()` says how many components, delta bindings and registrations were
exercised. `print_report` prints both.

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
const auto sampling = session.check_random_move_distribution(session.rng());

if (costs.mismatches != 0 || costs.invalid != 0 || sampling.out_of_neighborhood != 0)
    return 1;
```

| Check | Result | What fails it |
| --- | --- | --- |
| `check_neighborhood_costs()` | `moves`, `mismatches`, `invalid` | a move whose delta evaluation disagrees with the full evaluation, or an invalid enumerated move |
| `check_move_independence()` | `moves`, `null_moves`, `repeated_states`, `invalid` | moves that change nothing (*null moves*), and different moves that lead to the same solution |
| `check_random_move_distribution(rng)` | `neighborhood_size`, `samples`, `out_of_neighborhood`, `unseen`, `min_frequency`, `max_frequency` | a sampled move that is not in the enumerated neighborhood; moves never sampled (`unseen`) hint at a biased `random_move` |

All three need the moves to be enumerable; the last one also needs
`random_move`. The interactive tester of chapter 12 runs the same checks from
its Move page: `C`, `D` and `U`.

The last two compare values: `check_move_independence` compares the solution
after each move with the others, `check_random_move_distribution` compares
sampled moves with enumerated ones. So far the tutorial's types had no
equality: to use these two checks, add it. Without it, everything else works:
calling one of the two does not compile, and the interactive tester does not
offer its neighborhood checks.

<!-- snippet: tutorial/tsp.hpp:equality -->
```cpp
// Equality of solutions and moves, for the neighborhood checks of a Session
// (chapter 13).
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

[Chapter 14](14-rest.md) offers the same searches over HTTP.
