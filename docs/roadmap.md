# Roadmap

Planned evolutions of EasyLocal: directions the design has already been
prepared for, not yet scheduled. Nothing here is a promise of the
[API stability](stability.md) policy; an item becomes part of the API only
when it is released and listed in the changelog.

## Asynchronous runs on a Session

**Why.** A `Session` runs a registered runner on the calling thread:
`session.run("sa", with(control))` returns when the runner has finished. That
suits a program that only waits for the result, and the REST service, where
every run has a Session of its own on a worker thread. It does not suit an
interactive frontend, which keeps reading the session (the current solution,
the selected move, the pages it shows) while a runner works, and must stay
responsive. The TextUI does this today on its own: it copies the app, the Input
and the current solution, runs the runner on a worker thread with
`app.run("name", ...)`, and puts the result back when it arrives.

**What.** The Session would offer the same in two steps, for every frontend:

- `session.start("sa")` prepares an independent run, with copies of what it
  needs, a `run_control` for cancellation and progress, and its own RNG drawn
  from the session's, to be executed on another thread;
- `session.accept(result)` makes the run's result the current solution, unless
  the session has moved on since the run started.

**When.** When a second interactive frontend needs it, for example a GUI, a web
frontend, or REST resources for interactive sessions (selecting and applying
moves over HTTP). The TextUI would then move to the same mechanism, and the
design would start from the needs of two real users.

## Python and Julia bindings

**Why.** The problem components are C++: the SolutionManager, the
neighborhood explorers, the cost components and their deltas, compiled for
speed. Experiments around them, such as choosing runners and parameters,
running many instances and seeds, and analysing the results, are often more
convenient in a notebook or a script.

**What.** An optional adapter, like the TextUI and the REST service, that
exposes a compiled app to Python or Julia: list its runners, set their
parameters through the configuration tree, create a Session on an Input, run
runners by name, and read solutions, costs and traces. The C++ problem is
written once; the scripting side never reimplements it. It would build on the
same public surface the other adapters use (`app`, `Session`, `app.run`, the
configuration tree), with a binding library for Python, such as nanobind, and
a C interface for Julia.

**When.** Not scheduled. Like the other adapters, it would live outside
`EasyLocal::Core` and could become a separate project once the API is stable.

## Shifting penalty

**Why.** When the hard constraints are folded into the cost with a fixed
weight, the right weight depends on the instance and on the phase of the
search: too low and the search stays infeasible, too high and it cannot cross
infeasible regions to reach better feasible ones. EasyLocal 3 had a
`ShiftingPenaltyRunner` that wrapped any move runner and adapted the weights
of the constraints while the search ran.

**What.** Weights that change during a run, raised after a number of
iterations in which a constraint stays violated and lowered after a number of
iterations in which it is satisfied, within a range and with a random
perturbation. In EasyLocal 4 the weights live in the cost expressions
(`weighted`, `hard_soft`), so the design starts there: dynamic weights in the
expression tree, which a runner, or a wrapper around any runner, adjusts,
with the deltas staying consistent with the current weights.

**When.** Not scheduled. It depends on how the cost expressions settle.

## Adaptive neighborhood selection

**Why.** In a multi-neighborhood search the share of moves drawn from each
neighborhood is fixed by the user, while the neighborhoods that pay off change
with the instance and along the run. EasyLocal 3 had a
`SimulatedAnnealingWithLearning` runner that learned these shares during the
search.

**What.** A selection policy for the union of neighborhoods that updates the
probability of each one from what its moves achieve (improving, sideways and
accepted moves, the improvement obtained, the time spent evaluating), with a
learning rate and a lower bound on each probability. It belongs to the
multi-neighborhood composition rather than to Simulated Annealing, so that
every runner that draws random moves could use it.

**When.** Not scheduled.
