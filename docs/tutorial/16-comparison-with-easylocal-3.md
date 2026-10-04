# 16. Comparison with EasyLocal 3

The concepts of EasyLocal 3 and their counterparts in this framework:

| EasyLocal 3 | EasyLocal 4 |
| --- | --- |
| Input / State / Move | Input / Solution / Move, plain values |
| `StateManager` | SolutionManager: validity and construction only |
| `CostComponent` | cost component (`evaluate`), attached with `component<C>()` |
| `PrintViolations` | an optional `describe(solution)` member of the cost component |
| hard/soft components and weights | cost expressions: `cost::hard_soft`, `cost::sum`, `cost::weighted`, `cost::in_order`, `cost::apply` |
| `DeltaCostComponent` | delta cost component (`delta_evaluate`), attached with `delta<C, D>()` |
| `NeighborhoodExplorer` (`FirstMove`, `NextMove`, `RandomMove`, `MakeMove`) | NeighborhoodExplorer: cursor or `moves`, `random_move`, `make_move` |
| `MultimodalNeighborhoodExplorer` | `neighborhood_union` |
| `Runner` subclasses (hill climbing, SA, ...) | algorithm classes in `easylocal::runners` with one `run` member |
| `Solver` (`SimpleLocalSearch`, token ring, ...) | `easylocal::solvers`, or a Session that runs a runner by name |
| `Tester`, `MoveTester` | the interactive tester: `tui::run(app, options)` (TextUI adapter); the checks of `check` and the Session |
| observers | `easylocal::trace` |
| `ParameterBox`, `Parameter<T>`, `CommandLineParameters` | parameter schemas and parameter sets |
| `Random::Uniform`, `Random::SetSeed` | an RNG passed to the members that need one |

## Next steps

[Coming from EasyLocal 3](../from-easylocal-3.md) ports an EasyLocal 3 program
step by step; the [reference](../reference/README.md) describes every
component in full.
