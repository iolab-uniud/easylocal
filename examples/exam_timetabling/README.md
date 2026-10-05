# Exam timetabling

A small example with a weighted sum of costs and delta cost components:
Simulated Annealing on an exam timetabling problem, run from the command line.

## The problem

Each exam must be put in one timeslot. Some pairs of exams share students, and
the penalty of a timetable is a weighted sum of three cost components:

- `StudentConflictComponent`, weight 1000: the students with two exams in the
  same timeslot;
- `ConsecutiveExamComponent`, weight 10: the students with two exams in
  consecutive timeslots;
- `TimeslotLoadComponent`, weight 1: the sum of the squared number of exams of
  each timeslot, which spreads the exams.

An instance file holds the number of exams, of timeslots and of conflicts,
then one line `first second students` per pair of exams that share students
(`instances/small.exam`):

```text
4 3 4
0 1 4
0 2 2
...
```

## The files

| File | Holds |
| --- | --- |
| `instance.hpp` | the input, `ExamTimetablingInstance`, with `read`; `conflicts_by_exam`, the conflicts of each exam |
| `solution.hpp` | the solution, `ExamTimetable`: the timeslot of each exam, with `read`, `write` and `describe` |
| `solution_manager.hpp` | `ExamTimetablingSolutionManager`: the initial timetable and the validity check |
| `cost_components.hpp` | the three cost components |
| `cost_deltas.hpp` | `ConsecutiveExamDeltaEvaluator`, the delta cost component of `ConsecutiveExamComponent` |
| `move.hpp` | the move, `MoveExam`: an exam and its new timeslot |
| `neighborhood_explorer.hpp` | `MoveExamNeighborhoodExplorer`: the moves (a generator), a random move, `make_move` |
| `main.cpp` | the program: the cost, the neighborhood with its delta cost components, the app with the runner `sa` |

The delta cost components compute the change of a cost component from the
conflicts of the moved exam only. They are written in two ways:
`StudentConflictComponent` has a member `delta_evaluate`, attached with
`with_delta<StudentConflictComponent>()`, while `ConsecutiveExamComponent` has
a class of its own, attached with
`with_delta<ConsecutiveExamComponent, ConsecutiveExamDeltaEvaluator>()`.
`TimeslotLoadComponent` has none: counting the loads visits every exam, as a
full evaluation does, so EasyLocal evaluates each move on a candidate solution.

`main.cpp` builds the app with `with_*` calls; the assignment and TSP examples
write the same with pipes (`|`).

Read `instance.hpp`, `solution.hpp` and `cost_components.hpp` first, then
`cost_deltas.hpp`, `neighborhood_explorer.hpp` and `main.cpp`.

## Build and run

```sh
cmake --preset dev && cmake --build build/dev
./build/dev/examples/exam_timetabling/easylocal_exam_timetabling
```

The program runs Simulated Annealing from the initial timetable of
`instances/small.exam`, with the seed 2026, and prints the penalty, the
running time, the effort of the run and the timetable. `--help` lists the
switches, among them the runner's parameters (`--runners.sa.temperature.*`)
and the weights of the cost (`--cost.weights`):

```sh
./build/dev/examples/exam_timetabling/easylocal_exam_timetabling \
  --runners.sa.temperature.allowed_iterations=10 --seed=42
```

`--target=0` stops the run at the first timetable with no penalty.
`--config examples/exam_timetabling/configs/small.cfg` reads the parameters
from a file; the switches on the command line win over it.
