# SolutionManager

`<easylocal/helpers/solution_manager.hpp>`

The SolutionManager defines *solution semantics*: which solutions are valid and
how to build them. It does not compute the cost; see [Cost](cost.md).

## Contract

| Member | Required | Used by |
| --- | --- | --- |
| `input_type`, `solution_type` | yes | everything |
| `input() const -> const input_type&` | yes | everything |
| `is_valid(const solution_type&) const -> bool` | yes | debug assertions, checks, Session and TextUI |
| `initial_solution() const -> solution_type` | no | `initialization::initial`, bound runner's `initial_solution()`, Session and TextUI |
| `random_solution(RNG&) const -> solution_type` | no | `initialization::random`, MultiStart, Session and TextUI |
| `hash(const solution_type&) const -> std::uint64_t` | no | solution identity, see below |
| `equal(const solution_type&, const solution_type&) const -> bool` | no | solution identity, see below |

Concepts: `base_solution_manager<SM>` (the required members),
`has_initial_solution<SM>`, `has_random_solution<SM, RNG>`,
`has_solution_hash<SM>`, `has_solution_equality<SM>`.

## Solution identity

Algorithms and tools that recognize a solution met before, such as a reactive
tabu list or a trace of the search trajectory, use
`easylocal::solution_hash(sm, solution)` and
`easylocal::solutions_equal(sm, lhs, rhs)`. They come from the SolutionManager's
`hash` and `equal` members when it has them, otherwise from the solution
type's `std::hash` and `operator==`. The members take precedence, so a problem
can leave out redundant data (caches, derived matrices) or identify symmetric
representations, such as a tour and its reverse. Equal solutions must have
equal hashes. Nothing requires them: only the features that recognize
solutions do.

`<easylocal/utils/hash.hpp>` helps to write a hash:

```cpp
std::uint64_t hash(const Tour& tour) const
{
    return easylocal::hash_range(tour.order);        // folds each element
}
// easylocal::hash_combine(seed, value) folds one value into a running hash
```

Both depend on the order of the values and on `std::hash`, so a hash may differ
between standard libraries.

The SolutionManager is constructed from the Input when a runner is bound:
`SM(const Input&, args...)`, where `args` come from the recipe
`solution_manager<SM>(args...)` and convert as the constructor takes them.

## Convenience base

```cpp
template<class Input, class Solution>
class solution_manager_base;   // input_type, solution_type, input(), protected input_
```

Inherit its constructor with `using solution_manager_base::solution_manager_base;`.
It is non-virtual and optional. It keeps the Input by reference, so the
constructor from a temporary Input is deleted: the Input outlives the
SolutionManager.

## Recipes

```cpp
solution_manager<SM>(args...)           // the SolutionManager
  | component<C>(args...)               // its cost: one component, or a cost
                                        // expression over several, see Cost
// equivalently: solution_manager<SM>(args...).with_cost(component<C>(args...))
```

A recipe without a cost is rejected: the cost always comes from cost
components. A recipe has exactly one cost expression.

## Design choices

- **Validity is structural.** `is_valid` checks that the representation is
  well formed. A solution violating problem constraints is valid and costly.
- **Construction is optional and explicit.** A runner starts from a solution
  you provide; only solvers and tools need `initial_solution` or
  `random_solution`. There is no automatic fallback between the two.
- **Randomness is passed in.** `random_solution(rng)` receives the generator,
  so the caller (usually a solver) controls seeding and replay.
- **No cost in the SolutionManager.** A SolutionManager does not evaluate the
  cost: every problem has the same cost layer, so
  delta evaluation, weighting and per-component diagnostics are always
  available; a monolithic objective is a single cost component.
