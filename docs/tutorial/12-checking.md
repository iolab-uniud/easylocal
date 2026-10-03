# 12. Checking a composed problem

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

## Tester checks: the neighborhood on a solution

The Tester adds checks of the neighborhood around its current solution:

<!-- snippet: tutorial/main.cpp:tester-checks -->
```cpp
const auto costs = tester.check_neighborhood_costs(); // delta vs full evaluation
const auto independence = tester.check_move_independence(); // null and repeated moves
const auto sampling = tester.check_random_move_distribution(tester.rng());
if (costs.mismatches != 0 || costs.invalid != 0 || sampling.out_of_neighborhood != 0)
    return 1;
```

| Check | Result | Needs |
| --- | --- | --- |
| `check_neighborhood_costs()` | `moves`, `mismatches` (delta vs full evaluation), `invalid` | enumerable moves, `equivalent` costs |
| `check_move_independence()` | `moves`, `null_moves` (moves that change nothing), `repeated_states`, `invalid` | enumerable moves, `Solution::operator==` |
| `check_random_move_distribution(rng)` | `samples`, `out_of_neighborhood`, `unseen`, `min_frequency`, `max_frequency` | enumerable and random moves, `Move::operator==` |

The last two compare values: `check_move_independence` compares the solution
after each move with the solution before it and with the other neighbors,
`check_random_move_distribution` compares sampled moves with enumerated ones.
So far the tutorial's types had no equality: to use these two checks, add it.
Without it, everything else works: calling one of the two checks does not
compile, and the interactive tester of the next chapter hides them.

<!-- snippet: tutorial/tsp.hpp:equality -->
```cpp
// Equality of solutions and moves, for the Tester checks of chapter 12.
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

The last check compares sampling with enumeration: a sampled move outside the
enumerated neighborhood is an error, never-sampled moves (`unseen`) hint at a
biased `random_move`. The interactive tester (next chapter) runs the same checks
from its pages: `C` on the Input/Output page, and `C`, `D` and `U` on the Move
page.

## See also

- [Testing](../reference/testing.md) for component checks.
- [Apps and tools](../reference/app-and-tools.md).

## Next steps

[Chapter 13](13-textui.md) explores the problem interactively.
