# SolutionManager

`<easylocal/helpers/solution_manager.hpp>`

The SolutionManager defines *solution semantics*: which solutions are valid and
how to build them. It does not compute the cost; see [Cost](cost.md).

## Contract

| Member | Required | Used by |
| --- | --- | --- |
| `input_type`, `solution_type` | yes | everything |
| `input() const -> const input_type&` | yes | everything |
| `is_valid(const solution_type&) const -> bool` | yes | debug assertions, checks, Tester |
| `initial_solution() const -> solution_type` | no | `initialization::initial`, `bound.initial_solution()`, Tester |
| `random_solution(RNG&) const -> solution_type` | no | `initialization::random`, MultiStart, Tester |

Concepts: `base_solution_manager<SM>` (the required members),
`has_initial_solution<SM>`, `has_random_solution<SM, RNG>`.

The SolutionManager is constructed from the Input when a runner is bound:
`SM{const Input&, args...}`, where `args` come from the recipe
`solution_manager<SM>(args...)`.

## Convenience base

```cpp
template<class Input, class Solution>
class solution_manager_base;   // input_type, solution_type, input(), protected input_
```

Inherit its constructor with `using solution_manager_base::solution_manager_base;`.
It is non-virtual and optional.

## Recipes

```cpp
solution_manager<SM>(args...)           // the SolutionManager
  | component<C>(args...)               // one or more cost components
  | aggregator(a)                       // optional, see Cost
// equivalently: solution_manager<SM>(args...).with_component<C>(args...).with_aggregator(a)
```

A recipe without components is rejected: the cost always comes from cost
components.

## Design choices

- **Validity is structural.** `is_valid` checks that the representation is
  well formed. A solution violating problem constraints is valid and costly.
- **Construction is optional and explicit.** A runner starts from a solution
  you provide; only solvers and tools need `initial_solution` or
  `random_solution`. There is no automatic fallback between the two.
- **Randomness is passed in.** `random_solution(rng)` receives the generator,
  so the caller (usually a solver) controls seeding and replay.
- **No cost in the SolutionManager.** Earlier designs let a SolutionManager
  evaluate the cost directly. Every problem now has the same cost layer, so
  delta evaluation, weighting and per-component diagnostics are always
  available; a monolithic objective is a single cost component.
