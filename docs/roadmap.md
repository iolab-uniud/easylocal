# Roadmap

Planned evolutions of EasyLocal: directions the design has already been
prepared for, not yet scheduled. Nothing here is a promise of the
[API stability](stability.md) policy; an item becomes part of the API only
when it is released and listed in the changelog.

## One spelling for each name

**Why.** Two documented ways to compose the same thing, the pipe (`|`, `&`)
and the `with_*` members, contradict the rule of one documented way per part;
and some names still say what the library called a concept before its
vocabulary settled: `check_delta_evaluator` checks a delta cost component, and
a few members and options keep older words.

**What.** The `with_*` members leave the public API (`detail`), so the
documentation and the examples show only `|` and `&`; the remaining
terminology moves in one breaking batch, such as `check_delta_evaluator` to
`check_delta_cost_component`, and the effort of a named run in
the fields of `search_result`. Alongside: messages naming what each algorithm
needs when a runner is registered on a neighborhood it cannot use, Pareto Late
Acceptance committing a move in place if a benchmark shows it pays, and a
restructured chapter 2 of the tutorial.

**When.** 4.0.0-alpha.3. Until then the `with_*` spellings are Experimental
(see [API stability](stability.md)).

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

## The Pareto front in the frontends

**Why.** A run with a `cost::pareto` cost returns the non-dominated solutions
it reached: `app.run("name", ...)` carries them in `named_run_result.front`,
the Session keeps them (`last_run_front()`), `cli::run` prints them and REST
returns them with the solution. The TextUI still shows one solution, the
current one, so a multi-objective app shows a single trade-off there.

**What.** A TextUI page that lists the front of the last run, each point with
its cost, and makes one of its solutions the current one.

**When.** Next, as a change of its own: the other frontends already show the
front.

## A failed run in the trace and in the result

**Why.** An exception thrown in the middle of a run, by a hook of the problem
or by the tracer, ends the run without a trace record and leaves `solve()`
with nothing: MultiStart loses its best start so far, a pipeline its earlier
stages and their reports. A trace shows only that the run did not finish
(`eltr.py` marks it unfinished), not why, and a long solve that fails at its
last start returns nothing of the work done.

**What.**

- a `run_aborted` core event, emitted by `search_run` when an exception
  leaves the run, with the counters and the cost reached, before the
  exception goes on;
- a `solve_failed` exception thrown by the solvers, which carries the best
  result so far and the stage reports, with the original exception nested.

**When.** Not scheduled: when a long-running use, such as a REST service or a
tuning campaign, needs the partial results.

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

Each frontend is a template on the app's type today: the TextUI and REST are
compiled with the problem. A binding needs a session whose type does not
depend on the app's, a runtime-erased Session (runner names, parameter paths,
solutions and costs as text or JSON, runs by name), which the TextUI and REST
could then share instead of each instantiating its own; after 4.0.

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

## A launcher across SolutionManagers

**Why.** The TextUI's launcher opens several apps on the same problem and
passes the Input and the current solution from one to the next, so the apps
must have the same SolutionManager. Some problems are explored with different
ones: two representations of a solution (a permutation and an assignment, for
example), or a relaxed model next to the full one. Moving a solution between
them today means saving it from one app and loading it in the other, when the
two formats agree.

**What.** A launcher whose apps may have different SolutionManagers, with
conversions between their solutions declared by the problem, for example a
hook `convert(const Input&, const SolutionA&) -> SolutionB` found by ADL. The
launcher converts the current solution when another app opens, and checks at
compile time that every app it may switch to can be reached by a conversion;
apps with the same SolutionManager keep passing it unchanged.

**When.** Not scheduled.

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
every runner that draws random moves could use it. A simpler fixed policy
fits the same place: an optional size of each child's neighborhood for a
solution would let the union draw a child in proportion to it, and so draw
uniformly among all the moves when the children do.

**When.** Not scheduled.

## Candidate strategies of Tabu Search

**Why.** The four tabu searches (`TabuSearch`, `FirstImprovementTabuSearch`,
`AspirationPlusTabuSearch`, `EliteCandidateTabuSearch`) are one algorithm with
four ways of scanning the neighborhood; they already share one parameter
block, `TabuSearchParameters<List, Candidates>`, but each strategy is still a
runner of its own.

**What.** One `TabuSearch<List, Aspiration, Candidates>` with a candidate
strategy policy, `candidates::{Full, FirstImprovement, AspirationPlus,
EliteList}`, whose parameters are the group `candidates` they already have,
and aliases for the names of EasyLocal 3.

**When.** Not scheduled.

## Cooperative runners

**Why.** A pipeline runs its stages one after the other, each from the solution
of the previous one. Some searches combine runners differently: several
runners at the same time, on different neighborhoods or with different
parameters, that exchange their best solutions (a portfolio, an island model),
or that alternate on the same solution under a common control, without a
fixed order.

**What.** A solver of cooperative runners: the runners, a policy that says when
they run (in parallel threads, or interleaved) and what they share (the best
solution found so far, at given intervals or when one improves it), a common
stopping criterion and budget, and the effort and best result of each runner
in the result. Its determinism with a seed when the runners run in parallel is
part of the design.

**When.** Not scheduled.

## Kicks, Iterated Local Search and Variable Neighborhood Descent

**Why.** EasyLocal 3 had kickers (a perturbation of several moves applied at
once to leave a local optimum), the `TokenRingSearch` and
`VariableNeighborhoodDescent` solvers. A pipeline runs its stages once, in a
fixed order: it cannot go back to an earlier stage when a later one improves,
nor perturb its solution between rounds, which Iterated Local Search and VND
need.

**What.** A kick, a sequence of random (or enumerated) moves of a
neighborhood applied together, as a stage of its own; pipeline rounds (run the
stages again, from the last solution, while a round improves the best cost or
until a budget), and a restart rule (back to the first stage on an
improvement), which together give Iterated Local Search (descent, kick,
acceptance) and VND (descents on neighborhoods in turn, back to the first
after an improvement). The rounds share the solve's budget and progress, as
the stages do.

**When.** Not scheduled.

## Parameter tuning

**Why.** The parameters of a search are usually tuned with an automatic
configurator: [irace](https://mlopez-ibanez.github.io/irace/) (iterated
racing), [SMAC](https://github.com/automl/SMAC3) (Bayesian optimization with
random forests) or [Optuna](https://optuna.org/) (Bayesian optimization in
Python, define-by-run). irace is supported: every parameter declares its
domain (`check(app)` fails on one without), conditions and requirements
between parameters (`only_if`, `config::require`) are exported as irace
conditions and `[forbidden]` expressions, and `cli::run --tuning.irace` writes
the scenario. What is left:

**What.** **SMAC and Optuna** exporters from the same domains and conditions:
the configuration space (ConfigSpace) and a target function for SMAC; the
search space as a Python function for Optuna, which with the Python bindings
(above) could also run the app in-process.

**When.** Not scheduled.

## Experiment configurations

**Why.** Tuning and benchmarking produce configurations (the tuned values, the
fixed ones, the instances and seeds) that must be kept with the version of the
code that produced them, and found again for a paper or a later comparison.
The tool that versions them is outside EasyLocal, but EasyLocal programs must
make it easy, with little boilerplate.

**What.** What a program could give to such a workflow:

- its effective configuration, every parameter with its value, in the
  `path = value` format that `--config` reads back;
- its identity in its output: the EasyLocal version and the commit of the
  program's code, recorded at build time;
- a machine-readable result of a run: instance, seed, configuration (or its
  hash), cost, effort and time;
- irace's elite configurations as configuration files.

**When.** Not scheduled: to be designed with the workflow it serves.

## Trace micro-benchmarks

**Why.** The tracing overhead benchmark of
[easylocal-benchmarks](https://github.com/iolab-uniud/easylocal-benchmarks)
measures a First Improvement workload, whose events are the core ones; it also
records the visited solutions, with a hash per committed move, and leaves them
out at compile time (`trace::without`). Tabu search's aspirations, escapes and
tenure changes, and the cost of each event type on its own, are not measured.

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
