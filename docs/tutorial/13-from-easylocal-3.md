# 13. Coming from EasyLocal 3

| EasyLocal 3 | EasyLocal |
| --- | --- |
| Input / State / Move | Input / Solution / Move, plain values |
| `StateManager` | SolutionManager: validity and construction only |
| `CostComponent` | cost component (`evaluate`), attached with `component<C>()` |
| hard/soft components and weights | aggregators: `cost::weighted_sum`, `cost::hierarchical`, custom |
| `DeltaCostComponent` | delta evaluator (`delta_evaluate`), attached with `delta<C, D>()` |
| `NeighborhoodExplorer` (`FirstMove`, `NextMove`, `RandomMove`, `MakeMove`) | NeighborhoodExplorer: cursor or `moves`, `random_move`, `make_move` |
| `MultimodalNeighborhoodExplorer` | `neighborhood_union` |
| `Runner` subclasses (hill climbing, SA, ...) | algorithm classes in `easylocal::runners` with one `run` member |
| `Solver` (`SimpleLocalSearch`, token ring, ...) | `easylocal::solvers` |
| `Tester` | `easylocal::Tester` and the TextUI adapter |
| observers | `easylocal::trace` |
| `ParameterBox` | parameter schemas and the configuration tree |

The main differences:

- **No virtual dispatch and no required base classes.** Capabilities are
  checked by concepts at compile time; the bases are optional conveniences.
- **Services are bound to an immutable Input** instead of being reconfigured,
  so concurrent runs only share the Input.
- **The cost always comes from cost components**, and their composition (the
  cost layer and the delta cost layer) is described by recipes.
- **Randomness is explicit**: RNGs are passed in, never hidden in services.
- **The search loop's machinery belongs to the framework** (`search_run`):
  counters, budget, cancellation, progress and events are not reimplemented by
  each runner.

## Next steps

The [reference](../reference/README.md) describes every component in full.
