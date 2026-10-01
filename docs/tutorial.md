# Writing a local search solver, step by step

This tutorial builds a local search solver for the symmetric Travelling
Salesperson Problem (TSP) with EasyLocal. It starts with a complete, minimal
program and then adds one capability per step: incremental (delta) evaluation,
random moves, Simulated Annealing, composite neighborhoods, a custom search
algorithm, solvers, configuration, testing, applications and tooling.

Every step states what the framework *requires* and what is *optional*. EasyLocal
components are specified incrementally: you write only what the algorithms and
tools you use actually need, and the compiler tells you when a capability is
missing.

The code in this tutorial is complete and compiles against the current headers.
The examples under `examples/` (Assignment, TSP, Exam Timetabling) are larger,
runnable versions of the same ideas.

- [Mental model](#mental-model)
- [Quick start](#quick-start)
- [Step 1 — Model the problem](#step-1--model-the-problem)
- [Step 2 — The SolutionManager](#step-2--the-solutionmanager)
- [Step 3 — Cost components and aggregation](#step-3--cost-components-and-aggregation)
- [Step 4 — The NeighborhoodExplorer](#step-4--the-neighborhoodexplorer)
- [Step 5 — Delta evaluation](#step-5--delta-evaluation)
- [Step 6 — Choosing a search algorithm](#step-6--choosing-a-search-algorithm)
- [Step 7 — Combining neighborhoods](#step-7--combining-neighborhoods)
- [Step 8 — Writing your own runner](#step-8--writing-your-own-runner)
- [Step 9 — Solvers](#step-9--solvers)
- [Step 10 — Configuration](#step-10--configuration)
- [Step 11 — Testing your components](#step-11--testing-your-components)
- [Step 12 — Applications, Tester, TextUI and REST](#step-12--applications-tester-textui-and-rest)
- [Step 13 — Observing and controlling a run](#step-13--observing-and-controlling-a-run)
- [Coming from EasyLocal 3](#coming-from-easylocal-3)
- [Capability summary](#capability-summary)

## Mental model

You describe the problem with a few plain value types and a few small service
classes; the framework composes them and runs the search.

```text
problem values      Input (immutable), Solution, Move

problem services    SolutionManager      validity, construction of solutions
                    cost components      one term of the objective each
                    aggregator           cost from the component values
                    NeighborhoodExplorer moves: validity, application, enumeration/sampling
                    delta evaluators     change of one component under a move

framework           Runner<Algorithm>    a search algorithm + the composed services
                    search_run           one execution: counters, budget, cancellation,
                                         progress and trace events
                    Solver               from an Input to a final solution
                    app / Tester         named runners over one problem, tooling
```

The SolutionManager together with its cost components and aggregator forms the
*cost layer*; the NeighborhoodExplorer together with its delta evaluators forms
the *delta cost layer*. Recipes such as
`solution_manager<SM>() | component<C>()` describe these compositions; the
runner materializes them when it is bound to an Input.

All services are instance-bound: they borrow the Input by `const&` and never own
or mutate it. The ownership order is `Input > SolutionManager >
NeighborhoodExplorer`.

## Quick start

A 2-opt First Improvement for the TSP in one file:

```cpp
#include <easylocal/easylocal.hpp>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <numeric>
#include <vector>

// The problem: Input, Solution and Move are plain values.
struct Tsp
{
    std::size_t cities{};
    std::vector<double> distance; // cities x cities, row-major

    [[nodiscard]] auto d(std::size_t from, std::size_t to) const -> double
    {
        return distance[from * cities + to];
    }
};

struct Tour
{
    std::vector<std::size_t> order;
};

struct TwoOpt
{
    std::size_t i; // reverse the segment order[i + 1 .. j]
    std::size_t j;
};

// SolutionManager: what a valid solution is and how to build one.
class TourManager : public easylocal::solution_manager_base<Tsp, Tour>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]] auto initial_solution() const -> Tour
    {
        Tour tour{std::vector<std::size_t>(input_.cities)};
        std::iota(tour.order.begin(), tour.order.end(), std::size_t{0});
        return tour;
    }

    [[nodiscard]] auto is_valid(const Tour& tour) const -> bool
    {
        return tour.order.size() == input_.cities;
    }
};

// A cost component: one term of the objective.
class TourLength
{
public:
    explicit TourLength(const Tsp& tsp) : tsp_{tsp} {}

    [[nodiscard]] auto evaluate(const Tour& tour) const -> double
    {
        double length = 0.0;
        for (std::size_t k = 0; k < tour.order.size(); ++k)
        {
            length += tsp_.d(tour.order[k], tour.order[(k + 1) % tour.order.size()]);
        }
        return length;
    }

private:
    const Tsp& tsp_;
};

// NeighborhoodExplorer: which moves exist and how they change a solution.
class TwoOptExplorer
    : public easylocal::neighborhood_explorer_base<TourManager, TwoOpt>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]] auto moves(const Tour& tour) const -> std::vector<TwoOpt>
    {
        std::vector<TwoOpt> result;
        const auto n = tour.order.size();
        for (std::size_t i = 0; i + 2 < n; ++i)
        {
            for (std::size_t j = i + 2; j < n && !(i == 0 && j + 1 == n); ++j)
            {
                result.push_back({i, j});
            }
        }
        return result;
    }

    [[nodiscard]] auto is_valid(const Tour& tour, const TwoOpt& move) const -> bool
    {
        return move.i + 2 <= move.j && move.j < tour.order.size();
    }

    void make_move(Tour& tour, const TwoOpt& move) const
    {
        std::reverse(
            tour.order.begin() + static_cast<std::ptrdiff_t>(move.i + 1),
            tour.order.begin() + static_cast<std::ptrdiff_t>(move.j + 1));
    }
};

int main()
{
    const Tsp tsp{
        .cities = 5,
        .distance = {
            0, 2, 9, 10, 7,
            2, 0, 6, 4, 3,
            9, 6, 0, 8, 5,
            10, 4, 8, 0, 6,
            7, 3, 5, 6, 0,
        },
    };

    // Compose a runner: algorithm | SolutionManager recipe | neighborhood recipe.
    auto runner =
        easylocal::make_runner<easylocal::runners::FirstImprovement>(
            easylocal::runners::FirstImprovementParameters{.max_evaluations = 10'000})
        | (easylocal::solution_manager<TourManager>()
           | easylocal::component<TourLength>()
           | easylocal::aggregator(easylocal::cost::weighted_sum{1.0}))
        | easylocal::neighborhood<TwoOptExplorer>();

    // Bind it to an Input and run it from a solution.
    auto bound = runner.bind(tsp);
    const auto result = bound.run(bound.initial_solution());

    std::cout << "length " << result.cost << " after " << result.evaluations
              << " evaluations\n";
}
```

Link the program against the `EasyLocal::Core` CMake target:

```cmake
find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core)
add_executable(tsp main.cpp)
target_link_libraries(tsp PRIVATE EasyLocal::Core)
```

It prints `length 26 after 9 evaluations`. Everything else in this tutorial
refines one of the pieces above.

## Step 1 — Model the problem

**Input**, **Solution** and **Move** are plain value types. They do not derive
from framework classes and do not reference each other:

- the Input is built once (for instance by your parser) and treated as immutable
  while services are bound to it;
- a Solution does not own or reference the Input;
- a Move does not own or reference a Solution; it is applied to one by the
  NeighborhoodExplorer.

Optional hooks are picked up by tools when present:

| Hook | Used by | Purpose |
| --- | --- | --- |
| `static Input::read(std::istream&)` or `operator>>` | Tester, TextUI | load an Input from a file |
| `static Solution::read(const Input&, std::istream&)` | Tester, TextUI | load a Solution |
| `Solution::write(const Input&, std::ostream&) const` or `operator<<` | Tester, TextUI | save a Solution |
| `describe() const -> std::string` on Input, Solution or Move | TextUI | human-readable display |
| `operator==` on Move | tests, Tester | compare moves |

Free functions found by ADL are accepted as alternatives to the member hooks:
`read_input(std::type_identity<Input>, std::istream&) -> Input`,
`read_solution(const Input&, std::istream&) -> Solution` and
`write_solution(const Input&, const Solution&, std::ostream&)`.

## Step 2 — The SolutionManager

The SolutionManager owns *solution semantics*: which solutions are valid and how
to obtain one. It does not evaluate the cost; cost components do (Step 3).

| Member | Required | Used by |
| --- | --- | --- |
| `input_type`, `solution_type` | yes | everything |
| `input() const -> const input_type&` | yes | everything |
| `is_valid(const Solution&) const -> bool` | yes | debug assertions, checks, Tester |
| `initial_solution() const -> Solution` | no | `Initial` initialization, `bound.initial_solution()` |
| `random_solution(RNG&) const -> Solution` | no | `Random` initialization, multi-start |

`is_valid` checks *structural* validity (a well-formed representation). A
solution that violates problem constraints is still valid; constraint
violations are expressed as cost, typically through hard components.

`easylocal::solution_manager_base<Input, Solution>` is an optional non-virtual
convenience base that provides the type aliases, the constructor and the
protected `input_` reference. A fully hand-written class works as well.

Construction is caller-owned: `random_solution` receives the RNG explicitly, so
seeding and replay are controlled by whoever runs the search (usually a
Solver).

> **Choice — cost in the SolutionManager.** A SolutionManager may also define
> `cost_type` and `evaluate(solution) -> cost_type` directly. Recipes without
> cost components then use it as is. This is convenient for very small problems,
> but cost components are the recommended route: they enable delta evaluation,
> weighting, hierarchical costs and per-component diagnostics.

## Step 3 — Cost components and aggregation

A **cost component** computes one term of the objective:

```cpp
class TourLength
{
public:
    explicit TourLength(const Tsp& tsp);
    auto evaluate(const Tour& tour) const -> double;
};
```

- The only requirement is `evaluate(const Solution&) const -> Value`.
- `Value` can be an arithmetic type or a domain type (for example a struct
  holding both a total and a count). Wrappers are useful when they add meaning,
  not because the framework needs them.
- Components are constructed from the Input when the runner is bound:
  `Component{const Input&, args...}` is preferred, `Component{args...}` is
  accepted for stateless components. Extra constructor arguments come from the
  recipe: `component<C>(args...)`.
- No base class is required.

Attach components to the SolutionManager recipe, then say how their values
become the cost with an **aggregator**:

```cpp
auto sm = easylocal::solution_manager<TourManager>()
        | easylocal::component<TourLength>()
        | easylocal::aggregator(easylocal::cost::weighted_sum{1.0});
```

An aggregator is a function object called with the component values in
declaration order. EasyLocal provides:

| Aggregator | Result | Notes |
| --- | --- | --- |
| `cost::weighted_sum{w1, w2, ...}` | scalar | weights configurable as `cost.weights` |
| `cost::weighted_sum_with_hard_penalty` | scalar | `hard_multiplier * hard + sum(w_i * soft_i)` |
| your own function object | any cost type | e.g. a hierarchical cost (below) |

If you omit the aggregator and every component value can be multiplied by a
weight and summed, EasyLocal materializes a unit-weight `weighted_sum` and logs
a warning. An explicit aggregator is required whenever no safe default exists.

### Structured costs

`easylocal::cost` (in `<easylocal/cost.hpp>`) provides cost types beyond
scalars:

```cpp
using HardCost = easylocal::cost::lexicographic<std::int64_t, std::int64_t>;
using Cost     = easylocal::cost::hierarchical<HardCost, double>;

struct MyAggregator
{
    // optional: the hard branch from the hard component prefix (used by TwoStage)
    auto hard(const OverloadValue& overload) const -> HardCost
    {
        return HardCost{overload.total, overload.machines};
    }

    auto operator()(const OverloadValue& overload, double imbalance) const -> Cost
    {
        return Cost{hard(overload), imbalance};
    }
};
```

- `cost::lexicographic<Ts...>` compares its values in order; it has no numeric
  delta.
- `cost::hierarchical<Hard, Soft>` gives the hard branch strict priority and
  compares soft only when hard is equivalent. Its `cost::delta` is
  hard-preserving: a hard improvement is `-inf`, a hard worsening `+inf`,
  otherwise the soft delta. Delta-based acceptance such as Metropolis can
  therefore run on it without ever accepting a hard degradation.
- Both are constructed directly, with deduced types: `cost::hierarchical{hard,
  soft}`.
- The optional `hard(...)` member over the leading (hard) components makes the
  aggregator model `cost::hard_projection`, which TwoStage uses to evaluate the
  hard components only.

### Cost semantics

Algorithms never compare costs with operators directly. They ask the search
context `better(a, b)`, `equivalent(a, b)` and `better_or_equivalent(a, b)`.
By default these are `<`, `==` and `<=` of the cost type. An aggregator may
override any of them by providing members with the same names, for instance to
compare floating-point costs with a tolerance.

## Step 4 — The NeighborhoodExplorer

The NeighborhoodExplorer owns *move semantics*:

| Member | Required | Used by |
| --- | --- | --- |
| `move_type`, `input_type`, `solution_type` | yes | everything |
| `is_valid(const Solution&, const Move&) const -> bool` | yes | debug assertions, checks |
| `make_move(Solution&, const Move&) const` | yes | every runner |
| `moves(const Solution&) const` | one of these two for deterministic runners | First/Best Improvement, Tester |
| `first_move(const Solution&, Move&)` + `next_move(...)` | | |
| `random_move(const Solution&, RNG&) const -> std::optional<Move>` | for stochastic runners | Simulated Annealing, unions, Tester |
| `name() -> std::string_view` (may be static) | no | TextUI display |

- `moves(solution)` may return any input range whose elements convert to
  `move_type`: a container, a view, a coroutine generator.
- The EL3-style cursor (`first_move`/`next_move`) is supported as an
  alternative; when both exist the cursor wins. Use it when enumerating lazily
  is natural and you want to avoid materializing all moves.
- `random_move` returns `std::nullopt` when the neighborhood is empty. No
  uniform distribution is required.
- `is_valid` checks the move against an already valid solution; solution
  validity remains the SolutionManager's responsibility.

`easylocal::neighborhood_explorer_base<SolutionManager, Move>` is an optional
non-virtual base providing the aliases and the SolutionManager reference.

Adding sampling to the quick-start explorer:

```cpp
template<std::uniform_random_bit_generator RNG>
auto random_move(const Tour& tour, RNG& rng) const -> std::optional<TwoOpt>
{
    const auto all = moves(tour);
    if (all.empty()) return std::nullopt;
    std::uniform_int_distribution<std::size_t> pick{0, all.size() - 1};
    return all[pick(rng)];
}
```

(A real implementation decodes a random rank directly, as in
`examples/tsp/neighborhood_explorer.hpp`.)

In debug builds the framework asserts a valid solution and a valid move before
every application and a valid solution afterwards; the checks disappear with
`NDEBUG`.

## Step 5 — Delta evaluation

Without help, evaluating a move means applying it to a copy of the solution and
re-evaluating every component. A **delta evaluator** computes how one
component's value changes under a move instead:

```cpp
class TwoOptLengthDelta
{
public:
    explicit TwoOptLengthDelta(const Tsp& tsp);

    auto delta_evaluate(const Tour& tour, const TwoOpt& move) const -> double
    {
        const auto n = tour.order.size();
        const auto a = tour.order[move.i], b = tour.order[move.i + 1];
        const auto c = tour.order[move.j], d = tour.order[(move.j + 1) % n];
        return tsp_.d(a, c) + tsp_.d(b, d) - tsp_.d(a, b) - tsp_.d(c, d);
    }
};
```

Bind it to the component in the neighborhood recipe:

```cpp
auto nhe = easylocal::neighborhood<TwoOptExplorer>()
         | easylocal::delta<TourLength, TwoOptLengthDelta>();
```

- The contract is algebraic: `Value + Delta -> Value`. With arithmetic values
  the delta can simply be the same type; with domain values define
  `operator+(Value, Delta)`.
- Deltas are **per component and per neighborhood**: each binding says how one
  component changes under the moves of one explorer. The move cost is always
  recomputed by the aggregator from the updated component values, so deltas
  never deal with weights or hierarchical structure.
- Coverage can be partial. Components without a delta binding are evaluated on
  a materialized candidate solution; when every component has a delta, no
  candidate solution is built at all.

> **Choice — separate or co-located delta.** The delta evaluator can be a
> separate class, as above, or a `delta_evaluate(solution, move)` member of the
> component itself, attached with `delta<TourLength>()`. Separate evaluators
> keep one component reusable across neighborhoods; co-located deltas are
> shorter when a component serves a single neighborhood.

## Step 6 — Choosing a search algorithm

Search algorithms live in `easylocal::runners`, one header each:

| Algorithm | Header | Needs from the neighborhood | Parameters |
| --- | --- | --- | --- |
| `FirstImprovement` | `runners/first_improvement.hpp` | `moves` or cursor | `max_evaluations` (0: until a local optimum) |
| `BestImprovement` | `runners/best_improvement.hpp` | `moves` or cursor | `max_evaluations` (0: until a local optimum) |
| `SimulatedAnnealing<Temperature, Acceptance>` | `runners/simulated_annealing.hpp` | `random_move` | a temperature policy |

A `Runner` couples an algorithm with the recipes. Two equivalent spellings:

```cpp
auto a = easylocal::make_runner<easylocal::runners::FirstImprovement>(parameters) | sm | nhe;
auto b = easylocal::Runner{easylocal::runners::FirstImprovement{parameters}} | sm | nhe;
```

The fluent form `.with_solution_manager(sm).with_neighborhood(nhe)` is
equivalent to the pipes.

Simulated Annealing takes its temperature policy as a value; the policies live
in `runners::temperature` (`Classic`, `FixedLength`, `Cutoff`, `Hybrid`), and
acceptance defaults to `runners::MetropolisAcceptance`:

```cpp
namespace runners = easylocal::runners;

auto sa = easylocal::Runner{runners::SimulatedAnnealing{runners::temperature::Classic{
              runners::temperature::ClassicParameters{
                  .initial_temperature = 10.0, .final_temperature = 0.1,
                  .cooling_rate = 0.95, .samples_per_temperature = 50}}}}
          | sm | nhe;

std::mt19937_64 rng{42};
auto bound = sa.bind(tsp);
const auto result = bound.run(bound.initial_solution(), rng);
```

Randomness is always explicit: stochastic algorithms take the RNG as a `run`
argument, and nothing inside the framework owns a hidden engine.

Metropolis acceptance needs a numeric difference between costs, expressed by
`cost::delta(candidate, current)`: arithmetic costs and `cost::hierarchical`
provide it; `cost::lexicographic` deliberately does not.

### Results

Built-in algorithms return `easylocal::search_result`:

```cpp
result.solution;     // the final (for SA: the best) solution
result.cost;
result.evaluations;  // including the initial evaluation
result.iterations;   // committed moves (FI/BI), proposed moves (SA)
result.termination;  // termination_reason::local_optimum, evaluation_budget_exhausted,
                     // cancelled or completed
```

Solvers, the Tester and the adapters require only what the concept
`easylocal::search_result_for<Result, Solution, Cost>` describes: a `solution`
and its `cost`. Custom runners may return richer result types.

## Step 7 — Combining neighborhoods

`neighborhood_union` combines several explorers into one. Each child keeps its
own move type and delta bindings:

```cpp
auto union_nhe = easylocal::neighborhood_union(
    easylocal::neighborhood<TwoOptExplorer>() | easylocal::delta<TourLength, TwoOptLengthDelta>(),
    easylocal::neighborhood<SwapExplorer>()   | easylocal::delta<TourLength, SwapLengthDelta>())
    | easylocal::random_biases(3.0, 1.0);
```

- Deterministic algorithms enumerate the children in order.
- Random sampling picks a child with the configured biases (configurable
  through `NeighborhoodUnionParameters`; a zero bias disables a child) and asks
  it for a move; a child that cannot produce one is excluded and another child
  is drawn, until a move is found or no child is left.
- Unions nest, and traces report the route of every move through the nesting
  (Step 13).

Algorithms need no change to use a union: it is just another neighborhood.

## Step 8 — Writing your own runner

A search algorithm is a class with a single `run` member. The framework calls it
with an `easylocal::search_run`, which forwards the search context
(`better`, `neighborhood_explorer`, ...) and owns everything every search
shares: evaluation and iteration counters, the evaluation budget, cancellation,
progress reporting and the core trace events.

```cpp
struct RandomDescentParameters
{
    std::size_t max_evaluations{1000};
};

class RandomDescent
{
public:
    using parameters_type = RandomDescentParameters; // for app registration
    explicit RandomDescent(RandomDescentParameters parameters);

    template<class Run, std::uniform_random_bit_generator RNG>
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
        run.limit_evaluations(parameters_.max_evaluations);
        auto current = run.start(solution);                 // evaluates, run_started

        while (!run.should_stop())                          // cancellation or budget
        {
            auto move = run.random_move(solution, rng);
            if (!move) break;

            run.next_iteration();
            auto candidate = run.evaluate_move(solution, current, *move);
            if (run.better(candidate.cost(), current.cost()))
            {
                run.commit(solution, current, std::move(candidate), *move);
            }
        }
        return run.finish(std::move(solution), current.cost()); // run_finished
    }
};
```

| `search_run` member | Effect |
| --- | --- |
| `limit_evaluations(n)` | evaluation budget, including the initial evaluation |
| `start(solution)` | first evaluation; emits `run_started`, reports progress |
| `should_stop()` | true on cancellation or exhausted budget; the reason is recorded |
| `moves(solution)`, `random_move(solution, rng)` | neighborhood access (with selection events for unions) |
| `evaluate_move(solution, current, move)` | counts, emits `move_evaluated`, reports progress |
| `commit(solution, current, candidate, move)` | applies the move, emits `move_accepted` |
| `next_iteration()` | advances the iteration counter |
| `incumbent_updated(previous, cost)` | for algorithms that track a best-so-far solution |
| `finish(solution, cost[, reason])` | emits `run_finished` and returns a `search_result` |
| `evaluation()`, `emit(event)`, `with_context(ctx)` | escape hatches |

Every runner is cancellable by contract: checking `should_stop()` in the loop is
all it takes. Per-run state lives in local variables, so `run` is `const` and a
runner object can be reused.

Use it like any built-in algorithm:
`easylocal::make_runner<RandomDescent>(RandomDescentParameters{}) | sm | nhe`.

## Step 9 — Solvers

A runner starts from a solution you provide. A **Solver** owns the whole
pipeline from an Input: it builds the initial solution, owns the RNG (so a seed
reproduces the run) and orchestrates one or more runners. Built-in solvers live
in `easylocal::solvers`:

| Solver | Does |
| --- | --- |
| `LocalSearch` | initial solution, then one run |
| `MultiStart` | `starts` independent runs, keeps the best |
| `TwoStage` | first stage on the hard cost branch, second on the full hierarchical cost |

```cpp
auto solver = easylocal::make_solver<easylocal::solvers::MultiStart>(
    runner,
    easylocal::solvers::MultiStartConfig<easylocal::initialization::Random>{
        .parameters = {.starts = 5},
        .initialization = easylocal::initialization::random,
        .seed = 1,
    });
const auto best = solver.solve(tsp);
```

- Initialization is chosen statically (`initialization::initial` /
  `initialization::random`, checked at compile time against the
  SolutionManager) or at runtime with `initialization::Mode`, validated when
  set.
- `TwoStage` requires a `cost::hierarchical` cost and an aggregator modelling
  `cost::hard_projection` (Step 3); its first stage never evaluates soft
  components.
- Solvers are class-as-key like runners: `make_solver<solvers::X>(runner,
  config)` or direct construction `solvers::X{runner, config}`.

## Step 10 — Configuration

Parameter blocks describe themselves with a schema, which makes them
configurable from the command line, from configuration files and from the
TextUI:

```cpp
struct AppParameters
{
    std::filesystem::path instance_file;
    std::uint64_t seed{0};

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"instance_file", &AppParameters::instance_file>("TSP instance"),
            easylocal::config::field<"seed", &AppParameters::seed>("RNG seed"));
    }

    auto validate() const -> easylocal::config::validation_result;
};
```

Runners, temperature policies and aggregators expose their parameters as a
configuration tree; combine them with your own:

```cpp
AppParameters app_parameters{};
const auto configuration = easylocal::config::root(
    easylocal::config::named<"application">(app_parameters),
    runner.configuration<"solver">());

const auto configured = easylocal::config::load_and_apply(argc, argv, configuration);
if (configured.help_requested) { /* print config::cli_help(argv[0], configuration) */ }
if (!configured)               { /* config::print_diagnostics(std::cerr, configured) */ }
```

Values are applied in place to the objects referenced by the tree, so the
runner sees them when it is bound. TOML files are available through the
optional `<easylocal/adapters/toml.hpp>` adapter.

## Step 11 — Testing your components

`<easylocal/testing.hpp>` provides contract checks for your components. It is
not part of the Core umbrella: include it from your test executables. Each
check takes a small fixture that supplies an Input, a Solution and the types
under test:

```cpp
#include <easylocal/testing.hpp>

struct TspCheckData
{
    static auto instance() -> Tsp { /* a small instance */ }
    static auto solution(const Tsp&) -> Tour { /* a valid solution */ }
};

struct TourLengthCheck : TspCheckData
{
    using solution_manager = TourManager;
    using component = TourLength;
};

struct TwoOptDeltaCheck : TspCheckData
{
    using neighborhood = TwoOptExplorer;
    using component = TourLength;
    using delta_evaluator = TwoOptLengthDelta;
};

int main()
{
    return easylocal::testing::run_checks(
        easylocal::testing::check_cost_component<TourLengthCheck>(),
        easylocal::testing::check_delta_evaluator<TwoOptDeltaCheck>());
}
```

| Check | Verifies |
| --- | --- |
| `check_solution_manager<T>` | construction capabilities produce valid solutions |
| `check_cost_component<T>` | evaluation is deterministic and consistent |
| `check_neighborhood<T>` | enumerated and sampled moves are valid, applied moves keep the solution valid |
| `check_delta_evaluator<T>` | `value + delta` equals the full re-evaluation for every move |

Fixtures may also set `random_samples` and `max_enumerated_moves`, provide
`make_*` factories for non-default construction, and `equivalent` for
tolerance-based comparisons. For a whole composed problem,
`easylocal::check(app, input)` runs the same checks against an app (Step 12).

## Step 12 — Applications, Tester, TextUI and REST

An **app** names a problem graph and the runners available on it:

```cpp
auto application = easylocal::app("tsp")
    .solution_manager(sm)
    .neighborhood(nhe)
    .runner<easylocal::runners::FirstImprovement>("fi");

application.runner_config<easylocal::runners::FirstImprovement>().max_evaluations = 1000;

const auto result = application.run<easylocal::runners::FirstImprovement>(tsp, initial);
```

- A runner is registered by its algorithm class and a name; its parameters
  (`parameters_type`) are stored in the app and editable with
  `runner_config<Algorithm>()` or `runner_config<Algorithm>("name")`.
- Every `app.run(...)` materializes fresh services for that run, so concurrent
  runs only share the immutable Input. `app.for_input(input)` gives a reusable
  runtime when you want to keep the services.
- `app.make_runner<Algorithm>()` and `app.make_solver<Solver, Algorithm>(config)`
  build standalone runners and solvers from the registration.

The app is the unit consumed by the tools:

| Tool | Header | Purpose |
| --- | --- | --- |
| `easylocal::check(app, input)` | `app/check.hpp` | contract checks of the composed problem |
| `easylocal::Tester` | `app/tester.hpp` | headless driver: load input/solution, inspect moves, run runners |
| `easylocal::tui::run(tester, options)` | `adapters/tui/tester.hpp` | interactive terminal tester (FTXUI) |
| `easylocal::rest::blueprint(prefix, app, codec, options)` | `adapters/rest.hpp` | HTTP API with asynchronous, cancellable runs (Crow) |

```cpp
easylocal::Tester tester{application};
tester.set_input(tsp);
tester.use_initial_solution();
tester.use_first_improving_move();
(void)tester.run_runner("fi");
```

The adapters are optional CMake components (`ConfigTOML`, `TUI`, `REST`) and are
kept outside Core. See [rest.md](rest.md) for the REST model.

## Step 13 — Observing and controlling a run

Pass a control and/or a tracer as the trailing argument of `run`:

```cpp
std::stop_source stop;
auto observer = [](const easylocal::run_progress& progress) {
    // progress.evaluations, progress.iterations, progress.evaluation_limit
};
easylocal::run_control control{stop.get_token(), observer};
easylocal::trace::memory_recorder<double> trace;

const auto result = bound.run(initial, rng, easylocal::with(control, trace));
```

- `stop.request_stop()` from another thread ends the run cooperatively; the
  result reports `termination_reason::cancelled`.
- The tracer receives typed search events (`run_started`, `move_evaluated`,
  `move_accepted`, `incumbent_updated`, `local_optimum`,
  `neighborhood_selection`, `run_finished`). Without a tracer, event
  construction is removed at compile time.
- Recorders: `trace::memory_recorder`, `trace::jsonl_recorder`, and binary ELTR
  recorders (`buffered_binary_recorder`, `async_binary_recorder`). See
  [tracing.md](tracing.md).

Diagnostic logging (`<easylocal/utils/logging.hpp>`) is separate from tracing;
see [logging.md](logging.md).

## Coming from EasyLocal 3

| EasyLocal 3 | EasyLocal |
| --- | --- |
| Input / State / Move | Input / Solution / Move, plain values |
| `StateManager` | SolutionManager (validity, construction) |
| `CostComponent` | cost component (`evaluate`), attached with `component<C>()` |
| hard/soft components, weights | aggregator: `cost::weighted_sum`, `cost::hierarchical`, custom |
| `DeltaCostComponent` | delta evaluator (`delta_evaluate`), attached with `delta<C, D>()` |
| `NeighborhoodExplorer` (`FirstMove`/`NextMove`/`RandomMove`/`MakeMove`) | NeighborhoodExplorer: cursor or `moves()`, `random_move`, `make_move` |
| `MultimodalNeighborhoodExplorer` | `neighborhood_union` |
| `Runner` subclasses (hill climbing, SA, ...) | algorithm classes in `easylocal::runners`, one `run` member |
| `Solver` (`SimpleLocalSearch`, token ring, ...) | `easylocal::solvers` |
| `Tester` | `easylocal::Tester` + TextUI adapter |
| observers | `easylocal::trace` |
| `ParameterBox` | parameter schemas and the configuration tree |

The main differences: no virtual dispatch and no inheritance requirements
(capabilities are concepts), services are bound to an immutable Input instead of
being reconfigured, randomness is passed explicitly, and the search loop's
common machinery (counters, budget, cancellation, events) belongs to the
framework rather than to each runner.

## Capability summary

What each consumer needs beyond the required members (`is_valid`, `make_move`,
the type aliases and `input()`):

| Consumer | SolutionManager | NeighborhoodExplorer | Cost |
| --- | --- | --- | --- |
| First / Best Improvement | — | `moves` or cursor | `better` |
| Simulated Annealing | — | `random_move` | `better`, `cost::delta` |
| `bound.initial_solution()`, `initialization::initial` | `initial_solution` | — | — |
| `initialization::random`, MultiStart | `random_solution` | — | — |
| TwoStage | — | — | `cost::hierarchical` + aggregator `hard(...)` |
| Tester move inspection | — | `moves`/cursor, `random_move` | `better`, `equivalent` |
| Tester / TextUI file I/O | — | — | Input/Solution read/write hooks |
