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
parameters through their parameter set, create a Session on an Input, run
runners by name, and read solutions, costs and traces. The C++ problem is
written once; the scripting side never reimplements it. It would build on the
same public surface the other adapters use (`app`, `Session`, `app.run`, the
parameter sets), with a binding library for Python, such as nanobind, and
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

## Trace micro-benchmarks

**Why.** The tracing overhead benchmark of
[easylocal-benchmarks](https://github.com/iolab-uniud/easylocal-benchmarks)
measures a First Improvement workload, whose events are the core ones. The
events added since (the visited solutions, with a hash computed per move, and
tabu search's aspirations, escapes and tenure changes) and the self-describing
ELTR header are not measured.

**What.** Micro-benchmarks of the encoding of each event type, the solution
hash included, and a Tabu Search workload with each tabu list, under the same
recorders as the existing benchmark (null, in-memory, buffered and
asynchronous ELTR), reporting the cost per event and per iteration.

**When.** Not scheduled.

## Delta ablation of a neighborhood explorer

**Why.** A problem's delta cost components are where most of its speed comes
from, but a delta is not always worth it: the EasyLocal 3 versus EasyLocal 4
benchmarks measure each problem with every delta, with some and with none,
and on the assignment problem the run without deltas (which applies each move
to a copy and recomputes the cost) beats the one with all of them, whose
deltas cost O(jobs) each. Which deltas pay off depends on the problem, the
instance and the neighborhood, and today it is found by hand.

**What.** A benchmarking tool for a problem's own neighborhood explorers: it
evaluates the moves of sampled solutions with every subset of the delta cost
components, from all of them to none, one component at a time (so each delta
is measured against the full evaluation it replaces), checks that every
subset gives the same costs, and reports the time per move of each
configuration and of each delta against `none`. It would run on the
problem's instances, next to the contract checks of `<easylocal/testing.hpp>`,
and tell which deltas to keep.

**When.** Not scheduled.

## Parameters described by reflection (C++26)

**Why.** A parameter block describes itself by hand: `parameter_schema()`
lists each field with its name, a pointer to the member and a description,
next to the field itself. Everything else is generic (the command line,
configuration files and TOML, the interactive tester's parameter windows), but
the list repeats the struct, and a field added without its schema line is
silently not configurable.

**What.** With C++26 static reflection, the schema would be derived from the
struct: one parameter per data member, named after it, with the description
given as an annotation on the member. A hand-written `parameter_schema()`
would remain possible, and would take precedence, for names or groupings
that differ from the members. The schema is then the only part of a runner's
configuration still written by hand: a runner already holds its algorithm's
parameters and builds the algorithm from them.

**When.** When the compilers EasyLocal supports provide reflection. In
October 2026, GCC 16 implements it behind `-freflection`; GCC 15, Clang 22
and 23 (neither the `^^` operator nor libc++'s `<meta>`) and Apple Clang do
not. Until all supported compilers have it, reflection could only be an
optional path next to the hand-written schema.
