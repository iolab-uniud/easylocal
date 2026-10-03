# Exam timetabling MWE

This MWE is the reference model for the standard Simulated Annealing cost path.
It deliberately uses three independent cost components, each with a plain
`penalty_type` value, and combines them into one arithmetic cost with the cost
expression
`cost::sum(component<StudentConflictComponent>() * 1000, component<ConsecutiveExamComponent>() * 10, component<TimeslotLoadComponent>())`:

- `StudentConflictComponent`: students with two exams in the same timeslot;
- `ConsecutiveExamComponent`: students with exams in consecutive timeslots;
- `TimeslotLoadComponent`: squared timeslot load, used as a simple balance term.

Two components have delta evaluators for `MoveExam`, in both supported
spellings: `StudentConflictComponent` co-locates its `delta_evaluate(...)` and
is attached with `delta<StudentConflictComponent>()`, while
`ConsecutiveExamComponent` uses the separate-evaluator form
`delta<Component, DeltaEvaluator>()`. Both look only at the conflicts of the
moved exam. `TimeslotLoadComponent` has none: its change needs the load of two
timeslots, and counting loads visits every exam, which is what a full
evaluation does; the framework evaluates it on a candidate solution instead. The final `penalty_type` is therefore
already the SA energy: no Cost-to-Energy adapter or projection is required by
the standard `MetropolisAcceptance` path.

## Runnable configured SA example

The executable intentionally uses the explicit `with_*` spelling of the
composition API: `solution_manager<SM>().with_cost(...)`,
`neighborhood<NHE>().with_delta<C, D>()...` and
`make_runner<SimulatedAnnealing<...>>(...).with_solution_manager(...).with_neighborhood(...)`.
Assignment demonstrates the equivalent pipe spelling.

`main.cpp` defines application-owned parameters for the external instance path
and RNG seed, while `FixedLengthParameters` remains beside the framework
temperature policy that it configures. The example builds a
`config::parameter_set` with `application.*` and the runner's parameters under
`solver` (`solver.search.temperature.*`), loads `instances/small.exam`, then runs
Simulated Annealing from the SolutionManager's `initial_solution()` with an
explicitly seeded RNG.

With the default top-level build:

```sh
./build/<preset>/examples/exam_timetabling/easylocal_exam_timetabling_mwe
```

The same set drives the generated CLI help and pre-bind overrides:

```sh
./build/<preset>/examples/exam_timetabling/easylocal_exam_timetabling_mwe \
  --solver.search.temperature.max_iterations=10 \
  --application.seed=42
```

A compact configuration file can provide the same dotted paths:

```sh
./build/<preset>/examples/exam_timetabling/easylocal_exam_timetabling_mwe \
  --config examples/exam_timetabling/configs/small.cfg
```

CLI values have higher precedence than values from the file.
