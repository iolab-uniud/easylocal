# Exam timetabling MWE

This MWE is the reference model for the standard Simulated Annealing cost path.
It deliberately uses three independent cost components and aggregates them into
one arithmetic cost with `easylocal::aggregation::weighted_sum`:

- `StudentConflictComponent`: students with two exams in the same timeslot;
- `ConsecutiveExamComponent`: students with exams in consecutive timeslots;
- `TimeslotLoadComponent`: squared timeslot load, used as a simple balance term.

All three components have matching delta evaluators for `MoveExam`. The final
`penalty_type` is therefore already the SA energy: no Cost-to-Energy adapter or
projection is required by the standard `MetropolisAcceptance` path.
