# Testing

`<easylocal/testing.hpp>` (`easylocal::testing`), not part of the Core umbrella

Contract checks for user components. Each check takes a fixture type `T` and
returns a `check_report`; `run_checks(reports...)` prints them and returns a
process exit code.

## Fixtures

| Member | Required | Meaning |
| --- | --- | --- |
| `static instance() -> Input` | yes | a small Input |
| `static solution(const Input&) -> Solution` | yes | a valid Solution |
| `solution_manager` (type) | for SolutionManager and component checks | the SolutionManager under test |
| `component` (type) | for component and delta checks | the cost component |
| `neighborhood` (type) | for neighborhood and delta checks | the NeighborhoodExplorer |
| `delta_evaluator` (type) | for delta checks | the separate delta evaluator (omit for a co-located one) |
| `static move(const Input&, const Solution&)` | no | the move used by the delta check |
| `random_samples`, `max_enumerated_moves` | no | sampling limits |
| `make_solution_manager`, `make_component`, `make_neighborhood`, `make_delta_evaluator` | no | factories for non-default construction |
| `equivalent(a, b)` | no | comparison for values (tolerances) |

## Checks

| Check | Verifies |
| --- | --- |
| `check_solution_manager<T>` | the fixture, initial and random solutions are valid |
| `check_cost_component<T>` | evaluating the same solution twice gives equivalent values |
| `check_neighborhood<T>` | enumerated and sampled moves are valid; applying them keeps the solution valid |
| `check_delta_evaluator<T>` | for a sample move, `value + delta` equals the component value after the move |

For a composed problem, `easylocal::check(app, input[, solution])`
(`<easylocal/app/check.hpp>`) runs the same kinds of checks on an app.

## Design choices

- **Checks are contracts, not unit tests.** They verify the laws the framework
  relies on (validity preservation, the delta law), so a component that passes
  them can be used by every algorithm.
- **Deterministic.** Randomized checks use a fixed internal generator.
