# NeighborhoodExplorer

`<easylocal/helpers/neighborhood_explorer.hpp>`,
`<easylocal/helpers/neighborhood_union.hpp>`

The NeighborhoodExplorer defines *move semantics*: which moves exist, whether a
move is valid and how it changes a solution.

## Contract

| Member | Required | Used by |
| --- | --- | --- |
| `input_type`, `solution_type`, `move_type` | yes (`input_type` and `solution_type` those of the SolutionManager) | everything |
| `input() const -> const Input&` | yes | the check that the explorer was built for the runner's Input, unions |
| `is_valid(const Solution&, const Move&) const -> bool` | yes | debug assertions, checks |
| `make_move(Solution&, const Move&) const` | yes; the Solution by reference, which it changes | every runner |
| `input() const -> const Input&` | no (`neighborhood_explorer_base` gives it) | the checks that the services share the Input |
| `moves(const Solution&) const` → input range of moves | one of the two, for deterministic algorithms | First and Best Improvement, Tabu Search, `check(app)`, Session and TextUI |
| `first_move(const Solution&, Move&)`, `next_move(const Solution&, Move&)` → `bool` | | |
| `random_move(const Solution&, RNG&) const -> std::optional<Move>` | for stochastic algorithms | Simulated Annealing, Hill Climbing, Late Acceptance Hill Climbing, Great Deluge, Pareto Late Acceptance Hill Climbing, `check(app)`, Session and TextUI |
| `name() -> std::string_view` | no | TextUI display |
| `inverse(const Solution&, const Move& move, const Move& tabu_move) const -> bool` | for tabu search | Tabu Search: whether `move` is forbidden by `tabu_move`, applied earlier |
| `tabu_attribute(const Move&) const` → value with `std::hash` and `==` | no (default: the move, if hashable) | frequency-based tabu memory |

`neighborhood_explorer_for<NHE, SM>` checks the first four rows, so an explorer
without them is rejected where it is composed, not deep in a debug assertion
or a union. `neighborhood_explorer_base<SM, Move>` provides the types and `input()`, from
the SolutionManager it is constructed with.

Concepts: `neighborhood_explorer_for<NHE, SM>`, `cursor_neighborhood_for`,
`native_moves_neighborhood_for`, `deterministic_neighborhood_for`,
`random_neighborhood_for`, `inverse_neighborhood_for`, `has_tabu_attribute`.
Customization points: `easylocal::moves(nhe, solution)`,
`easylocal::random_move(nhe, solution, rng)`, `easylocal::inverse(nhe,
solution, move, tabu_move)` and `easylocal::tabu_attribute(nhe, move)`.

- `make_move` takes the Solution by non-const reference: a runner on an
  explorer whose `make_move` takes it by value or by const reference, which
  would change a copy and leave the search where it is, is rejected with a
  message when the neighborhood is added (`runner | neighborhood<NHE>()`), and
  `check_neighborhood` reports its null moves.
- `moves` may return any input range whose elements convert to `move_type`.
- When both `moves` and the cursor exist, the cursor is used.
- `random_move` may return `std::optional<T>` for any `T` convertible to
  `move_type`; no distribution is required.
- `inverse` has no default: which moves a tabu move forbids (the same pair of
  jobs, or any move of either job) is a modelling choice of the neighborhood,
  and can be one of its parameters.
- The recipe checks the contract member by member when it is written (when the
  explorer declares its `solution_type`, else when it is bound), so a missing
  `move_type` or `is_valid`, or a `make_move` that is not `const`, is named in
  the error.
- The explorer is constructed from the SolutionManager:
  `NHE(const SM&, args...)`, `args` from `neighborhood<NHE>(args...)`. The
  construction uses parentheses, so the arguments convert as the constructor
  takes them (`neighborhood<NHE>(2)` for a `double` parameter).

## Parameters

An explorer may have parameters, such as the inverse definition of a tabu
search, or a neighborhood size. It declares them as a parameter block
(`using parameters_type = P;`, see [Configuration](configuration.md)) and is
constructed from them after the SolutionManager:

```cpp
struct SwapParameters { std::string inverse{"both_jobs"}; /* schema, validate */ };

class SwapExplorer
{
public:
    using parameters_type = SwapParameters;
    SwapExplorer(const SM&, const SwapParameters&, args...);
    // ...
};

neighborhood<SwapExplorer>()                              // default parameters
neighborhood<SwapExplorer>(SwapParameters{...}, args...)  // given ones
```

The recipe holds the parameters (`parameters()`, `configuration()`), keeps
them when deltas are attached, and gives them to the explorer each time a
runner or an app is bound. A runner and an app expose them under
`neighborhood.*` (a runner's own neighborhood in an app under
`runners.<name>.neighborhood.*`); in a union, each child's under its position
(`neighborhood.0.*`), next to `neighborhood.random_biases`. This is the rule of
every configurable class: its `parameters_type` is a parameter block, and it
is constructed from it (see [Configuration](configuration.md#configurable-objects)).

## Convenience base

```cpp
template<class SolutionManager, class Move>
class neighborhood_explorer_base;  // aliases, input(), protected solution_manager_
```

It keeps the SolutionManager by reference, so the constructor from a temporary
SolutionManager is deleted: the SolutionManager outlives the explorer.

## Recipes

```cpp
neighborhood<NHE>(args...)
  | delta<C, D>(args...)     // separate delta cost component for component C
  | delta<C>()               // co-located: C::delta_evaluate
// equivalently: neighborhood<NHE>(args...).with_delta<C, D>(args...).with_delta<C>()
```

## Neighborhood unions

```cpp
neighborhood_union(child_recipe_1, child_recipe_2, ...)
  | random_biases(b1, b2, ...)
```

- The move type is a variant of the children's moves. A component is
  evaluated by deltas only when every child has a delta for it, each child
  then using its own; when one child has none, the moves of every child are
  evaluated in full for that component (a delta for every child is the
  remedy).
- Enumeration visits the children in order.
- Sampling draws a child with probability proportional to its bias, excluding
  zero-bias children and children that produced no move, until a move is found
  or no child is left. Biases are configurable (`NeighborhoodUnionParameters`).
- Unions nest; traces record each move's route through the nesting and the
  selection statistics.
- A neighborhood type may appear more than once in a union, with different
  parameters: `child<I>()` names a child by its position, `child<NHE>()` by
  its type when it occurs once.
- `inverse`: moves of different children never forbid each other; between
  moves of the same child, the child decides. The union has it when every
  child has it.
- `tabu_attribute`: the child's attribute, tagged with the child (a
  `std::variant` by index), so equal attributes of two children stay distinct.

## Design choices

- **Two enumeration protocols.** The range protocol is idiomatic C++; the cursor
  keeps EasyLocal 3 explorers and lazy enumeration natural. Algorithms see a
  single customization point.
- **Validity is relative to a valid solution.** `is_valid(solution, move)` never
  re-checks the solution; the framework asserts both before and after every
  application in debug builds.
- **Sampling is not uniform by contract.** Explorers choose their own
  distribution; unions only control the choice of the child.
