# Assignment

A small example with a hard/soft cost: First Improvement on a load-balancing
problem, run from the command line, from the TextUI or as a REST service.

## The problem

Each job has a demand and must be assigned to one machine; each machine has a
capacity. The load of a machine is the total demand of its jobs.

- **Hard cost**: the total overload, the sum over the machines of
  `max(0, load - capacity)`, then the number of overloaded machines.
- **Soft cost**: the load imbalance, `max(load) - min(load)`.

A hard improvement always beats a soft one. For example, with demands
`[4, 3, 2]`, capacities `[5, 5]` and the assignment `[0, 0, 1]`, the loads are
`[7, 2]`: the hard cost is `(2, 1)` and the soft cost `5`. Moving job 1 to
machine 1 gives loads `[4, 5]`, hard cost `(0, 0)` and soft cost `1`.

An instance file holds the number of jobs and of machines, then the demands,
then the capacities (`instances/small.assignment`):

```text
3 2
4 3 2
5 5
```

## The files

| File | Holds |
| --- | --- |
| `instance.hpp` | the input, `AssignmentInstance`: the demands and the capacities |
| `instance_io.hpp` | `read_input`, which reads an instance file |
| `solution.hpp` | the solution, `AssignmentSolution`: the machine of each job, with `read`, `write` and `describe` |
| `solution_manager.hpp` | `AssignmentSolutionManager`: the initial solution and the validity check |
| `cost_components.hpp` | the cost components, `CapacityCostComponent` (hard) and `LoadImbalanceCostComponent` (soft) |
| `cost.hpp` | the cost type and `assignment_cost()`, the cost expression that combines the components |
| `move.hpp` | the move, `ReassignJobMove`: a job and its new machine |
| `neighborhood_explorer.hpp` | `ReassignJobNeighborhoodExplorer`: the moves (a generator), a random move, `make_move` |
| `application.hpp` | the app: the solution manager, the neighborhood and the runners `fi` and `slow-fi` |
| `demo_runner.hpp` | `SlowFirstImprovement` (`slow-fi`): First Improvement that waits before each evaluation, to watch a run |
| `main.cpp` | the command-line program |
| `tui_main.cpp` | the TextUI program |
| `rest_main.cpp` | the REST service |

There are no delta cost components: the change of a machine's load needs the
loads, and computing them scans every job, as a full evaluation does. EasyLocal
then evaluates each move on a candidate solution, with no extra code.

Read `instance.hpp`, `solution.hpp` and `cost_components.hpp` first, then
`neighborhood_explorer.hpp`, `application.hpp` and `main.cpp`.

## Build and run

```sh
cmake --preset dev && cmake --build build/dev
./build/dev/examples/assignment/easylocal_assignment
```

The program runs `fi` from the initial solution of `instances/small.assignment`
and prints the cost, the running time, the effort of the run and the solution.
`--help` lists the switches; for example, a run with at most 25 evaluations:

```sh
./build/dev/examples/assignment/easylocal_assignment --runners.fi.max_evaluations=25
```

`--target` stops the run at the first solution that reaches a cost, written as
the cost nests, `[[overload, overloaded_machines], load_imbalance]`: for
example `--target='[[0, 0], 1]'`.
`--config examples/assignment/configs/small.cfg` reads the parameters from a
file; the switches on the command line win over it.

### TextUI

With the TextUI component (`-DEASYLOCAL_ENABLE_TUI=ON`),
`easylocal_assignment_tui` opens the interactive tester on
`instances/large.assignment` (250 jobs, 16 machines). Select `slow-fi`, start
a run and stop it with `X Stop`: the progress of the run shows while it
runs, and the stopped run keeps its best solution.

### REST service

With the REST component (`-DEASYLOCAL_ENABLE_REST=ON`),
`easylocal_assignment_rest [port [completed-run-capacity]]` serves the app at
`/assignment`, on port 18080 by default, and keeps the last 64 completed runs.
The input is a JSON object or the text of an instance file:

```sh
curl -X POST http://localhost:18080/assignment/runners/fi/runs \
  -H 'Content-Type: application/json' \
  -d '{"input":{"demand":[4,4,2],"capacity":[5,5]}}'

curl -X POST http://localhost:18080/assignment/runners/fi/runs \
  -H 'Content-Type: application/json' \
  -d '{"input":"3 2 4 3 2 5 5"}'
```

The answer gives the `id` of the run: `GET /assignment/runs/<id>` reports its
progress, `GET /assignment/runs/<id>/solution` its solution, and
`POST /assignment/runs/<id>/cancel` stops it. The REST guide of the
documentation describes every route.
