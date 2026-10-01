# NeighborhoodExplorer

`<easylocal/helpers/neighborhood_explorer.hpp>`,
`<easylocal/helpers/neighborhood_union.hpp>`

The NeighborhoodExplorer defines *move semantics*: which moves exist, whether a
move is valid and how it changes a solution.

## Contract

| Member | Required | Used by |
| --- | --- | --- |
| `input_type`, `solution_type`, `move_type` | yes | everything |
| `is_valid(const Solution&, const Move&) const -> bool` | yes | debug assertions, checks |
| `make_move(Solution&, const Move&) const` | yes | every runner |
| `moves(const Solution&) const` → input range of moves | one of the two, for deterministic algorithms | First/Best Improvement, Tester |
| `first_move(const Solution&, Move&)`, `next_move(const Solution&, Move&)` → `bool` | | |
| `random_move(const Solution&, RNG&) const -> std::optional<Move>` | for stochastic algorithms | Simulated Annealing, Tester |
| `name() -> std::string_view` | no | TextUI display |

Concepts: `neighborhood_explorer_for<NHE, SM>`, `cursor_neighborhood_for`,
`native_moves_neighborhood_for`, `deterministic_neighborhood_for`,
`random_neighborhood_for`. Customization points: `easylocal::moves(nhe,
solution)` and `easylocal::random_move(nhe, solution, rng)`.

- `moves` may return any input range whose elements convert to `move_type`.
- When both `moves` and the cursor exist, the cursor is used.
- `random_move` may return `std::optional<T>` for any `T` convertible to
  `move_type`; no distribution is required.
- The explorer is constructed from the SolutionManager:
  `NHE{const SM&, args...}`, `args` from `neighborhood<NHE>(args...)`.

## Convenience base

```cpp
template<class SolutionManager, class Move>
class neighborhood_explorer_base;  // aliases, input(), protected solution_manager_
```

## Recipes

```cpp
neighborhood<NHE>(args...)
  | delta<C, D>(args...)     // separate delta evaluator for component C
  | delta<C>()               // co-located: C::delta_evaluate
// equivalently: neighborhood<NHE>(args...).with_delta<C, D>(args...).with_delta<C>()
```

## Neighborhood unions

```cpp
neighborhood_union(child_recipe_1, child_recipe_2, ...)
  | random_biases(b1, b2, ...)
```

- The move type is a variant of the children's moves; each child keeps its
  delta bindings.
- Enumeration visits the children in order.
- Sampling draws a child with probability proportional to its bias, excluding
  zero-bias children and children that produced no move, until a move is found
  or no child is left. Biases are configurable (`NeighborhoodUnionParameters`).
- Unions nest; traces record each move's route through the nesting and the
  selection statistics.
- A neighborhood type may appear once per union.

## Design choices

- **Two enumeration protocols.** The range protocol is idiomatic C++; the cursor
  keeps EasyLocal 3 explorers and lazy enumeration natural. Algorithms see a
  single customization point.
- **Validity is relative to a valid solution.** `is_valid(solution, move)` never
  re-checks the solution; the framework asserts both before and after every
  application in debug builds.
- **Sampling is not uniform by contract.** Explorers choose their own
  distribution; unions only control the choice of the child.
