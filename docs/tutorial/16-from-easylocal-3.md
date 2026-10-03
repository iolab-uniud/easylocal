# 16. Coming from EasyLocal 3

| EasyLocal 3 | EasyLocal |
| --- | --- |
| Input / State / Move | Input / Solution / Move, plain values |
| `StateManager` | SolutionManager: validity and construction only |
| `CostComponent` | cost component (`evaluate`), attached with `component<C>()` |
| hard/soft components and weights | cost expressions: `cost::hard_soft`, `cost::sum`, `cost::weighted`, `cost::in_order`, `cost::apply` |
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
- **Only what is used is written.** A component implements the members the
  composed algorithms and tools call, not a whole interface: an explorer used
  only by Simulated Annealing has `random_move` and no enumeration.
- **Services are bound to an immutable Input** instead of being reconfigured,
  so concurrent runs only share the Input.
- **The cost always comes from cost components**, and their composition (the
  cost layer and the delta cost layer) is described by recipes.
- **Hard and soft are not flags on a component.** A cost expression in the
  recipe states them, with each weight beside its term:
  `cost::hard_soft(cost::sum(component<A>(), component<B>()), component<C>() * 10)`.
  The hard cost is a separate level, not a large multiplier, so it can never be
  traded for soft improvements.
- **Randomness is explicit**: RNGs are passed in, never hidden in services.
- **The search loop's machinery belongs to the framework** (`search_run`):
  counters, budget, cancellation, progress and events are not reimplemented by
  each runner.

## Next steps

The [reference](../reference/README.md) describes every component in full.
