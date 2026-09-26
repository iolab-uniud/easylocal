# Exam timetabling MWE

This MWE is the reference model for the standard Simulated Annealing cost path.
It deliberately uses three independent cost components and aggregates them into
one arithmetic cost with `easylocal::aggregation::weighted_sum`:

- `StudentConflictComponent`: students with two exams in the same timeslot;
- `ConsecutiveExamComponent`: students with exams in consecutive timeslots;
- `TimeslotLoadComponent`: squared timeslot load, used as a simple balance term.

All three components have matching delta evaluators for `MoveExam`. The MWE
shows both supported spellings: `StudentConflictComponent` co-locates its
`delta_evaluate(...)` and is attached with `delta<StudentConflictComponent>()`,
while the other two components use the primary separate-evaluator form
`delta<Component, DeltaEvaluator>()`. The final `penalty_type` is therefore
already the SA energy: no Cost-to-Energy adapter or projection is required by
the standard `MetropolisAcceptance` path.

## Runnable configured SA example

`main.cpp` defines application-owned parameters for the external instance path
and RNG seed, while `FixedLengthParameters` remains beside the framework
temperature policy that it configures. The example builds and traverses a
`config::root(...)` with `application.*` and the runner-provided
`solver.search.temperature.*` paths, loads `instances/small.exam`, then runs a
fully delta-enabled Simulated Annealing search with an explicit deterministic
RNG.

With the default top-level build:

```sh
./build/<preset>/examples/exam_timetabling/easylocal_exam_timetabling_mwe
```

The same tree drives the generated CLI help and pre-bind overrides:

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
