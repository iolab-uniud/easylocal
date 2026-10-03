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

## Runnable SA example

The executable intentionally uses the explicit `with_*` spelling of the
composition API: `solution_manager<SM>().with_cost(...)`,
`neighborhood<NHE>().with_delta<C, D>()...` and
`app(...).with_solution_manager(...).with_neighborhood(...).with_runner<...>(...)`.
Assignment demonstrates the equivalent pipe spelling.

`main.cpp` registers Simulated Annealing as the runner `sa` of an app and runs
it with `easylocal::cli::run`, on `instances/small.exam` from the
SolutionManager's `initial_solution()`, with the seed 2026 by default:

```sh
./build/<preset>/examples/exam_timetabling/easylocal_exam_timetabling_mwe
```

`--help` lists the switches; the runner's parameters are
`--runners.sa.temperature.*` and the cost's weights `--cost.weights`:

```sh
./build/<preset>/examples/exam_timetabling/easylocal_exam_timetabling_mwe \
  --runners.sa.temperature.max_iterations=10 \
  --seed=42
```

`--target` stops the search at the first timetable whose penalty reaches a
value, `--target=0` at the first one with no penalty.

A compact configuration file can provide the same dotted paths:

```sh
./build/<preset>/examples/exam_timetabling/easylocal_exam_timetabling_mwe \
  --config examples/exam_timetabling/configs/small.cfg
```

CLI values have higher precedence than values from the file.
