# Coming from EasyLocal 3

This page is for readers with an EasyLocal 3 program to port. It has three
parts:

- **[The map](#the-map)**: what each EasyLocal 3 concept, runner and solver
  became, and what is not there yet.
- **[The walkthrough](#porting-the-problem)**: a complete EasyLocal 3 program,
  the TSP with 2-opt moves, migrated piece by piece up to the TSP of the
  [tutorial](tutorial/README.md).
- **[Checking the port](#checking-the-port)**, with a
  [checklist](#a-porting-checklist) for when it compiles.

The EasyLocal 3 code quoted here is the TSP of the [benchmarks](benchmarks.md)
that compare the two versions, abridged, on the last EasyLocal 3 release,
[easylocal-legacy v3.4.1](https://github.com/iolab-uniud/easylocal-legacy/tree/v3.4.1);
the EasyLocal 4 code is the tutorial's, which is compiled and tested at every
build.

## The map

### The concepts

| EasyLocal 3 | EasyLocal 4 |
| --- | --- |
| Input, Solution and Move (the State of `RandomState`, `PrintState`) | the same three values, plain: no base class, no back pointer to the Input |
| `SolutionManager` (`RandomState`, `GreedyState`, `CheckConsistency`) | SolutionManager: construction and validity only |
| `CostComponent::ComputeCost` | cost component (`evaluate`), attached with `component<C>()` |
| `PrintViolations` | an optional `describe(solution)` member of the cost component |
| the hard flag and the weight of a component, `HARD_WEIGHT` | cost expressions: `cost::hard_soft`, `cost::sum`, `cost::weighted`, `cost::in_order`, `cost::apply`, `cost::objectives` |
| `CostStructure`, `DefaultCostStructure<CFtype>` | nothing to declare: the cost type follows from what the components return |
| `DeltaCostComponent::ComputeDeltaCost` | delta cost component (`delta_evaluate`), attached with `delta<C, D>()` |
| `NeighborhoodExplorer` (`FirstMove`, `NextMove`, `RandomMove`, `MakeMove`, `FeasibleMove`) | NeighborhoodExplorer: a cursor or a `moves()` generator, `random_move`, `make_move`, `is_valid` |
| `MultimodalNeighborhoodExplorer`, `ActiveMove` | `neighborhood_union`, whose move is a variant of the children's moves |
| `ParallelNeighborhoodExplorer` (TBB) | no counterpart |
| `Kicker` | no counterpart ([roadmap](roadmap.md#kicks-iterated-local-search-and-variable-neighborhood-descent)) |
| `Runner` subclasses, `MoveRunner` | algorithm classes in `easylocal::runners`, with one `run` member |
| `Solver` (`SimpleLocalSearch`, `MultiStartSearch`, ...) | `easylocal::solvers`, or an app whose runners a Session runs by name |
| `Tester`, `MoveTester` | the interactive tester: `tui::run(app, options)` (TextUI adapter); the checks of `check` and the Session |
| `ComponentTester`, `KickerTester` | no counterpart: `easylocal::testing` checks the components in unit tests |
| `Trace::Channel` and its sinks | `easylocal::trace`: typed events and tracers, written with `--trace` |
| `Interruptible`, a runner's timeout parameter | `run_control` (cancellation and progress) and the run options `timeout` and `stop_at` |
| `ParameterBox`, `Parameter<T>`, `CommandLineParameters` | a parameter block per component, with a schema and domains, gathered in a `parameter_set` |
| `Random::Uniform`, `Random::SetSeed` | an RNG passed to the members that need one |
| the modelling layer (`AutoState`, expressions) | no counterpart |
| `Boost.program_options`, TBB | no dependencies: the library parses the command line |

Beyond the names:

- **No virtual dispatch and no required base classes.** Capabilities are
  checked by concepts at compile time; the bases are optional conveniences.
- **Only what is used is written.** A component implements the members the
  composed algorithms and tools call, not a whole interface: an explorer used
  only by Simulated Annealing has `random_move` and no enumeration.
- **Services are bound to an immutable Input** instead of being reconfigured,
  so concurrent runs only share the Input. The SolutionManager, the explorers,
  the cost components and the deltas receive it in their constructor, the
  place for data precomputed from the instance; values (Input, Solution, Move)
  hold no Input, and hooks such as `read_solution` receive it as a parameter
  (see the [problem model](reference/problem-model.md#design-choices)).
- **The cost always comes from cost components**, and their composition (the
  cost layer and the delta cost layer) is described by recipes, not by objects
  registered into each other.
- **Hard and soft are not flags on a component.** A cost expression in the
  recipe states them, with each weight beside its term:
  `cost::hard_soft(cost::sum(component<A>(), component<B>()), component<C>() * 10)`.
  The hard cost is a separate level, not a large multiplier, so it can never be
  traded for soft improvements.
- **Randomness is explicit**: RNGs are passed in, never hidden in services.
- **The search loop's machinery belongs to the framework** (`search_run`):
  counters, budget, cancellation, progress and events are not reimplemented by
  each runner.
- **What is missing is a compile error**, where the component is composed, with
  the signature to write; EasyLocal 3 reported a missing piece as a
  pure-virtual to implement, or at run time.

### The runners and the solvers

| EasyLocal 3 | EasyLocal 4 |
| --- | --- |
| `FirstDescent` | `runners::FirstImprovement` (the scan restarts from the first move) |
| `SteepestDescent` | `runners::BestImprovement` (ties go to the first best move, not drawn at random) |
| `HillClimbing` | `runners::HillClimbing` |
| `LateAcceptanceHillClimbing` | `runners::LateAcceptanceHillClimbing` (the history records the current cost, see [Runners](reference/runners.md)) |
| `GreatDeluge` | `runners::GreatDeluge` |
| `SimulatedAnnealing` (`max_neighbors_sampled`, `max_neighbors_accepted`) | `runners::SimulatedAnnealing<temperature::Hybrid>`, the same schedule; `temperature::Classic` is the textbook one, which cools on samples alone and ends at the final temperature |
| `SimulatedAnnealingOnlyCutoff`, `SimulatedAnnealingFixedTemperature`, `SimulatedAnnealingTimeBased` | `SimulatedAnnealing` with `temperature::Cutoff`, `FixedTemperature`, `TimeBased` |
| `SimulatedAnnealingWithReheating`, `...TimeBased` | `SimulatedAnnealing<temperature::Reheating<Hybrid>>`, `Reheating<TimeBased>`; every schedule with an initial temperature can be reheated |
| `TabuSearch`, `FirstImprovementTabuSearch` | `runners::TabuSearch`, `runners::FirstImprovementTabuSearch`, and `AspirationPlusTabuSearch`, `EliteCandidateTabuSearch` |
| `min_tenure`, `max_tenure` | a tabu list policy: `runners::tabu::FixedLength` (one tenure), `RandomTenure` (drawn in a range), and also `Cyclic`, `Reactive`, `Frequency` |
| `max_idle_iterations` | `max_idle_iterations`, in the runner's parameter block, as every other limit |
| the `Inverse` function given to `TabuSearch` | `inverse(solution, move, tabu_move)` in the explorer |
| `SimpleLocalSearch`, `MultiStartSearch` | `solvers::LocalSearch`, `solvers::MultiStart` |
| a solve in stages, written by hand with `Resolve` | `solvers::two_stage`, or a `solvers` pipeline of stages |
| — | `runners::ParetoLateAcceptanceHillClimbing`, for a multi-objective cost |

Every algorithm is a class template with a parameter block of its own, listed
in [Runners](reference/runners.md); an algorithm of your own is written once on
`search_run` ([chapter 7](tutorial/07-custom-runner.md)).

### What is not there yet

EasyLocal 3 code that uses these has no counterpart to port to:

| EasyLocal 3 | Planned |
| --- | --- |
| `ShiftingPenaltyRunner` | [Shifting penalty](roadmap.md#shifting-penalty) |
| `SimulatedAnnealingWithLearning` | [Adaptive neighborhood selection](roadmap.md#adaptive-neighborhood-selection) |
| `SampleTabuSearch` | [Candidate strategies of Tabu Search](roadmap.md#candidate-strategies-of-tabu-search) |
| kickers, `VariableNeighborhoodDescent` | [Kicks, Iterated Local Search and Variable Neighborhood Descent](roadmap.md#kicks-iterated-local-search-and-variable-neighborhood-descent) |
| `TokenRingSearch` | [Cooperative runners](roadmap.md#cooperative-runners), for the runners that exchange solutions; a fixed order of runners is a pipeline today |
| `GRASP` (`GreedyState(alpha, k)`) | not planned: a randomized construction is a `random_solution` of its own, and a `solvers::MultiStart` over it |
| `ParallelNeighborhoodExplorer`, the modelling layer | not planned |

Nothing on the roadmap is a promise: see [API stability](stability.md).

## Before you start

- EasyLocal needs a C++23 compiler (see the [quick start](quick-start.md))
  and no longer depends on Boost: the command line is parsed by the library.
- Headers and names have changed: `#include <easylocal/easylocal.hpp>` and the
  namespace `easylocal` replace `easylocal.hh` and the namespace
  `EasyLocal::Core`. That name now belongs to the CMake target,
  `EasyLocal::Core`, from
  `find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core)`.
- In place of `using namespace EasyLocal::Core;`, give the namespace a short
  alias, as the code of this page does:

  ```cpp title="EasyLocal 4"
  namespace el = easylocal;               // el::app, el::component, ...
  namespace runners = easylocal::runners; // runners::FirstImprovement, ...
  ```

  `el::app` is then `easylocal::app`. The aliases go in a function or a source
  file, not in a header, where they would reach every file that includes it
  (see the [conventions of the tutorial](tutorial/README.md#conventions-of-the-code)).
- Migrate in the order of this page, and run the program after each group
  of steps. A first version needs only the SolutionManager, the cost
  components and one explorer: without delta cost components, moves are
  evaluated on a copy of the solution with the move applied. Add the deltas
  afterwards, one at a time, and check each against the full evaluation
  ([checking the port](#checking-the-port)), as EasyLocal 3's `MoveTester` did.

## Porting the problem

### 1. The Input

An EasyLocal 3 Input was a class that read its own file in the constructor:

```cpp title="EasyLocal 3"
class Input
{
public:
    explicit Input(const std::string& path)
    {
        std::ifstream is(path);
        is >> city_count;
        distances.resize(city_count * city_count);
        for (auto& d : distances)
            is >> d;
    }
    double Distance(std::size_t from, std::size_t to) const;

    std::size_t city_count = 0;
    std::vector<double> distances;
};
```

Now the Input is a plain value, read from a stream by a function of its own
(step 4), so that an Input can also be built in code (as in the tests) or
received over HTTP:

<!-- snippet: tutorial/tsp.hpp:model -->
```cpp title="EasyLocal 4"
// Input: the instance, immutable during the search.
struct Tsp
{
    // distance[a][b] is the distance between cities a and b (symmetric).
    std::vector<std::vector<double>> distance;

    std::size_t cities() const
    {
        return distance.size();
    }
};

// Solution: order[k] is the k-th city visited; after the last city the tour
// returns to order[0].
struct Tour
{
    std::vector<std::size_t> order;
};

// Move: exchange the cities visited at positions i and j, with i < j.
struct SwapCities
{
    std::size_t i;
    std::size_t j;
};
```

The accessors of the Input, such as `Distance(from, to)`, can stay as they
are: the library never calls members of the Input. Data precomputed from the
instance stays there too, or in the constructor of the service that uses it.

### 2. The Solution

An EasyLocal 3 State was constructed from the Input and often kept a
reference to it:

```cpp title="EasyLocal 3"
class Tour
{
public:
    explicit Tour(const Input& in) : tour(in.city_count) {}
    std::vector<std::size_t> tour;
};
```

`Tour` in the model of step 1 is the EasyLocal 4 Solution: a value that can
be copied, moved and assigned.

- The reference to the Input is no longer needed: the SolutionManager, the
  explorers, the cost components and the deltas are all constructed from the
  current Input, so every member that receives a solution can reach the Input
  too (`input()` in the bases, or the member it was stored in), and the hooks
  that read and write a solution take it as a parameter. Keeping it is
  possible but not recommended: the runners copy and assign solutions, which a
  reference member forbids and a pointer (`const Input*`) only allows with
  care, since every copy must point to the same Input that outlives it.
- A constructor that takes the Input only to size the containers can go as
  well: the SolutionManager builds solutions (step 5), with the Input at hand.
  Keep it as long as the Solution is read with `operator>>`, which reads into
  `Solution{input}` (step 4).
- Data derived from the solution for speed (redundant matrices, counters) may
  stay in the Solution. Keep them up to date in `make_move`, and leave them out
  of solution identity with the SolutionManager's `hash` and `equal` when a
  tabu list or a trace compares solutions (see the
  [SolutionManager reference](reference/solution-manager.md)).

### 3. The Move

An EasyLocal 3 Move was a class with a constructor that set its attributes,
with default arguments, since the runners declared a Move and then filled it,
and the comparison and stream operators the framework required:

```cpp title="EasyLocal 3"
class TwoOpt
{
public:
    TwoOpt(std::size_t first_edge = 0, std::size_t second_edge = 0)
        : first_edge(first_edge), second_edge(second_edge) {}
    std::size_t first_edge, second_edge;
};
bool operator==(const TwoOpt& a, const TwoOpt& b);
bool operator!=(const TwoOpt& a, const TwoOpt& b);
bool operator<(const TwoOpt& a, const TwoOpt& b);
std::ostream& operator<<(std::ostream& os, const TwoOpt& mv);

// in the explorer
mv = TwoOpt(first_edge, second_edge);
```

In EasyLocal 4 the Move is a `struct` with public members and no constructor:

<!-- snippet: tutorial/tsp.hpp:two-opt-move -->
```cpp title="EasyLocal 4"
// Move: reverse the part of the tour between positions i + 1 and j.
struct TwoOpt
{
    std::size_t i;
    std::size_t j;
};
```

- **A struct, with public members**: the explorer, the deltas and the tester
  read the attributes of a move directly, and the move has no invariant to
  protect.
- **No constructor**: the struct is an aggregate, so it is created with braces,
  `TwoOpt{i, j}` or `TwoOpt{.i = i, .j = j}`, as the explorer of step 8 does.
  It is also default-constructible, `TwoOpt{}` with zero members, which an
  explorer with a cursor (`first_move`, `next_move`) needs; prefer these
  implicit constructors to writing one.
- **No operator is required**, but the ones you have can stay, and are used
  when present:
  - `operator==` by the neighborhood checks of a Session
    ([chapter 14](tutorial/14-checking.md));
  - `operator<<`, or `describe(move)`, for the text the tester shows;
  - `operator<` and `operator!=` are not used: remove them if nothing else
    needs them.

  In C++20 a defaulted `operator==` is enough, and the struct stays an
  aggregate:

  ```cpp title="EasyLocal 4 — optional"
  struct TwoOpt
  {
      std::size_t i;
      std::size_t j;

      bool operator==(const TwoOpt&) const = default;
  };
  ```

### 4. Reading, writing and describing

EasyLocal 3 read the Input in its constructor and the State with `operator>>`,
and printed it with `operator<<`. **Those operators keep working**: the library
uses them when nothing more specific is there, so a port can start without
touching them.

```cpp title="EasyLocal 4 — what an EasyLocal 3 program already has"
// Read into a default-constructed Tsp, in place of the file constructor.
std::istream& operator>>(std::istream& in, Tsp& tsp);

// Read into Tour{tsp}: on this path the Solution keeps the constructor from
// the Input that step 2 could otherwise drop.
std::istream& operator>>(std::istream& in, Tour& tour);

// Written as it was, by the tester, cli::run and the REST service.
std::ostream& operator<<(std::ostream& out, const Tour& tour);
```

The dedicated hooks are free functions found by argument-dependent lookup,
which the library prefers when they are there:

- `read_input(std::type_identity<Input>, std::istream&)` returns the Input;
  the tag selects it by the type it reads, since a function cannot be
  overloaded on its return type alone;
- `read_solution(const Input&, std::istream&)` returns the Solution, and
  `write_solution(const Input&, const Solution&, std::ostream&)` writes it;
- `describe(value)` gives the text the tools show for an Input, a Solution or
  a Move.

They are worth writing because they receive the Input — reading a solution may
need the instance, here the number of cities, and the Solution no longer holds
it — and because the Solution then needs no constructor from the Input. The
tutorial's TSP writes all of them
([chapter 5](tutorial/05-running-a-search.md#reading-the-instance-printing-the-solution));
its two solution hooks are the counterpart of the two State operators above:

<!-- snippet: tutorial/tsp.hpp:solution-io -->
```cpp title="EasyLocal 4"
inline Tour read_solution(const Tsp& tsp, std::istream& in)
{
    Tour tour{std::vector<std::size_t>(tsp.cities())};
    for (auto& city : tour.order)
        if (!(in >> city))
            throw std::runtime_error{"invalid tour"};
    return tour;
}

inline void write_solution(const Tsp&, const Tour& tour, std::ostream& out)
{
    for (const auto city : tour.order)
        out << city << ' ';
    out << '\n';
}
```

| EasyLocal 3 | EasyLocal 4, in the order the library tries them |
| --- | --- |
| `Input(const std::string& path)` reading the file | a static `Input::read(in)`, `read_input(std::type_identity<Input>, in)`, or `operator>>` into a default-constructed Input |
| `operator>>(is, State&)` | a static `Solution::read(input, in)`, `read_solution(input, in)`, or `operator>>` into `Solution{input}` |
| `operator<<(os, const State&)` | a member `solution.write(input, out)`, `write_solution(input, solution, out)`, or `operator<<` |
| `PrintViolations`, "pretty print output" | `describe(input)`, `describe(solution)`, `describe(move)`, and the `describe(solution)` of a cost component (step 6) |

Whichever path the Input and the Solution take, it is the one the file helpers
(`load_input`, `load_solution`), the tester, `cli::run` and the REST service
use: reading and writing are never written again in `main`.

### 5. The SolutionManager

```cpp title="EasyLocal 3"
class TspSolutionManager : public SolutionManager<Input, Tour, CostStructure>
{
public:
    explicit TspSolutionManager(const Input& in)
        : SolutionManager<Input, Tour, CostStructure>(in, "TspSolutionManager") {}

    void RandomState(Tour& st) override
    {
        st.tour.resize(in.city_count);
        std::iota(st.tour.begin(), st.tour.end(), std::size_t{0});
        std::shuffle(st.tour.begin(), st.tour.end(), Random::GetGenerator());
    }

    bool CheckConsistency(const Tour& st) const override
    {
        return st.tour.size() == in.city_count;
    }
};
```

<!-- snippet: tutorial/tsp.hpp:solution-manager -->
```cpp title="EasyLocal 4"
class TourManager : public easylocal::solution_manager_base<Tsp, Tour>
{
public:
    using solution_manager_base::solution_manager_base;

    // The cities in index order: 0, 1, ..., n - 1.
    Tour initial_solution() const
    {
        Tour tour{std::vector<std::size_t>(input().cities())};
        std::ranges::iota(tour.order, std::size_t{0});
        return tour;
    }

    // The same cities in a random order.
    template<std::uniform_random_bit_generator RNG>
    Tour random_solution(RNG& rng) const
    {
        auto tour = initial_solution();
        std::shuffle(tour.order.begin(), tour.order.end(), rng);
        return tour;
    }

    // A tour is valid when it is a permutation of 0, 1, ..., n - 1: it has
    // n positions and visits every city exactly once.
    bool is_valid(const Tour& tour) const
    {
        return std::ranges::is_permutation(
            tour.order,
            std::views::iota(std::size_t{0}, input().cities()));
    }
};
```

| EasyLocal 3 | EasyLocal 4 |
| --- | --- |
| constructor `(in, name)` | inherited: `using solution_manager_base::solution_manager_base;` |
| `in` | `input()` |
| `RandomState(Solution&)` with `Random::` | `random_solution(RNG&)`, which returns the solution |
| `GreedyState(Solution&)` | `initial_solution()`, which returns the solution |
| `CheckConsistency` | `is_valid`: the representation is well formed |
| `CostFunctionComponents`, `AddCostComponent` | the recipe (steps 6 and 10) |
| `LowerBoundReached`, `OptimalStateReached` | a target cost for the run: `stop_at(cost)`, or `--target` |
| `StateDistance` | no counterpart |
| `PrintState`, `DumpState` | `describe(solution)`, `write_solution` (step 4) |

The function that creates a random state needs no other change than taking
the generator as a parameter: replace `Random::GetGenerator()` and
`Random::Uniform<T>(a, b)` with `rng` and
`std::uniform_int_distribution<T>{a, b}(rng)`.

### 6. The cost components

```cpp title="EasyLocal 3"
class TourLength : public CostComponent<Input, Tour, double>
{
public:
    explicit TourLength(const Input& in)
        : CostComponent<Input, Tour, double>(in, 1.0, false, "TourLength") {}

    double ComputeCost(const Tour& st) const override
    {
        double total = 0.0;
        for (std::size_t position = 0; position < st.tour.size(); ++position)
            total += in.Distance(st.tour[position],
                                 st.tour[(position + 1) % st.tour.size()]);
        return total;
    }

    void PrintViolations(const Tour&, std::ostream&) const override {}
};
```

<!-- snippet: tutorial/tsp.hpp:cost-component!component-text -->
```cpp title="EasyLocal 4"
class TourLength
{
public:
    explicit TourLength(const Tsp& input) : input_{input} {}

    double evaluate(const Tour& tour) const
    {
        const auto n = tour.order.size();
        double length = 0.0;
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto from = tour.order[k];
            const auto to =
                tour.order[(k + 1) % n]; // the last city goes back to the first
            length += input_.distance[from][to];
        }
        return length;
    }

private:
    const Tsp& input_;
};
```

- `ComputeCost` becomes `evaluate`; the component has no base class and keeps
  the Input it is constructed from.
- The weight and the hard flag leave the constructor and go into the cost
  expression of the recipe. A program with hard components `H1`, `H2` and soft
  components `S1` (weight 1) and `S2` (weight 5) becomes:

  ```cpp title="EasyLocal 4"
  el::solution_manager<Manager>()
      | el::cost::hard_soft(
          el::cost::sum(el::component<H1>(), el::component<H2>()),
          el::cost::sum(el::component<S1>(), el::component<S2>() * 5))
  ```

  `HARD_WEIGHT` is gone: the hard cost is compared first, so no soft gain can
  make up for a violation. When the weights of the hard components mattered
  only to guide the search, keep them inside the hard sum.

  This changes what the stochastic runners accept. In EasyLocal 3 the hard
  cost was weighted into one number, so Simulated Annealing accepted, now and
  then, a move that worsened the hard cost for a soft gain, and crossed
  infeasible regions; with `hard_soft` the delta of a hard degradation is
  infinite, and the Metropolis criterion never accepts it.
  To keep EasyLocal 3's behaviour, write the cost as one weighted sum,
  `el::cost::sum(el::component<H1>() * 1000, ..., el::component<S1>())`,
  and check that the result is feasible.
- `PrintViolations` becomes an optional member,
  `std::string describe(const Solution&) const`, which returns the text
  instead of printing it; an optional `name()` replaces the name given to the
  constructor. Without them, reports show the component's position and value
  ([chapter 11](tutorial/11-apps-and-tools.md#a-report-of-the-cost-components)).
- An `int` component stays `int`; `DefaultCostStructure<double>` and the
  `CFtype` parameters disappear, since the cost type follows from what
  `evaluate` returns. A cost of more than two levels, or several objectives,
  is a `cost::in_order` or a `cost::objectives` of components
  ([chapter 2](tutorial/02-cost.md)), where EasyLocal 3 had the fixed pair
  violations/objective.

### 7. The delta cost components

```cpp title="EasyLocal 3"
class TwoOptTourLengthDelta : public DeltaCostComponent<Input, Tour, TwoOpt, double>
{
public:
    TwoOptTourLengthDelta(const Input& in, TourLength& cc)
        : DeltaCostComponent<Input, Tour, TwoOpt, double>(in, cc, "TwoOptTourLengthDelta") {}

    double ComputeDeltaCost(const Tour& st, const TwoOpt& mv) const override
    {
        const auto n = st.tour.size();
        const auto first = st.tour[mv.first_edge];
        const auto first_next = st.tour[(mv.first_edge + 1) % n];
        const auto second = st.tour[mv.second_edge];
        const auto second_next = st.tour[(mv.second_edge + 1) % n];
        return in.Distance(first, second) + in.Distance(first_next, second_next)
            - in.Distance(first, first_next) - in.Distance(second, second_next);
    }
};
```

<!-- snippet: tutorial/tsp.hpp:delta -->
```cpp title="EasyLocal 4"
class TwoOptLengthDelta
{
public:
    explicit TwoOptLengthDelta(const Tsp& input) : input_{input} {}

    // The tour goes a -> b ... c -> d; after the move it goes a -> c ... b -> d,
    // the segment b ... c reversed, at the same cost with symmetric distances.
    double delta_evaluate(const Tour& tour, const TwoOpt& move) const
    {
        const auto n = tour.order.size();
        const auto a = tour.order[move.i];
        const auto b = tour.order[move.i + 1];
        const auto c = tour.order[move.j];
        const auto d = tour.order[(move.j + 1) % n];
        const auto& distance = input_.distance;
        return distance[a][c] + distance[b][d] - distance[a][b] - distance[c][d];
    }

private:
    const Tsp& input_;
};
```

- `ComputeDeltaCost` becomes `delta_evaluate`, with the same meaning: the
  change of the component's own value, without its weight.
- The link to the component leaves the constructor: the recipe states it,
  `delta<TourLength, TwoOptLengthDelta>()`. A delta can also be a member of the
  component itself (a *co-located* delta, [chapter 4](tutorial/04-delta-evaluation.md)).
- A component without a delta for some neighborhood needs nothing, as in
  EasyLocal 3 when it was added with `AddCostComponent` to the explorer: its
  change is computed on a copy of the solution with the move applied.
- A wrong delta is the classic porting bug: [checking the
  port](#checking-the-port) says how to catch it.

### 8. The NeighborhoodExplorer

```cpp title="EasyLocal 3"
class TwoOptNeighborhoodExplorer
    : public NeighborhoodExplorer<Input, Tour, TwoOpt, CostStructure>
{
public:
    TwoOptNeighborhoodExplorer(const Input& in, TspSolutionManager& sm)
        : NeighborhoodExplorer<Input, Tour, TwoOpt, CostStructure>(in, sm, "2-opt") {}

    bool FeasibleMove(const Tour& st, const TwoOpt& mv) const override;

    void RandomMove(const Tour& st, TwoOpt& mv) const override
    {
        const auto n = st.tour.size();
        if (n < 4)
            throw EmptyNeighborhood();
        while (true)
        {
            auto first_edge = Random::Uniform<std::size_t>(0, n - 1);
            auto second_edge = Random::Uniform<std::size_t>(0, n - 1);
            // ... ordered, and drawn again while not a valid move
        }
    }

    void FirstMove(const Tour& st, TwoOpt& mv) const override;   // throws EmptyNeighborhood
    bool NextMove(const Tour& st, TwoOpt& mv) const override;
    void MakeMove(Tour& st, const TwoOpt& mv) const override;
};
```

<!-- snippet: tutorial/tsp.hpp:two-opt!two-opt-move~comments -->
```cpp title="EasyLocal 4"
class TwoOptExplorer : public easylocal::neighborhood_explorer_base<TourManager, TwoOpt>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    static std::string_view name()
    {
        return "2-opt";
    }

    bool first_move(const Tour& tour, TwoOpt& move) const
    {
        move = TwoOpt{0, 1};
        return next_move(tour, move);
    }

    bool next_move(const Tour& tour, TwoOpt& move) const
    {
        const auto n = tour.order.size();
        do
        {
            if (++move.j == n)
            {
                ++move.i;
                move.j = move.i + 2;
            }
            if (move.j >= n)
                return false;
        }
        while (move.i == 0 && move.j + 1 == n);
        return true;
    }

    template<std::uniform_random_bit_generator RNG>
    std::optional<TwoOpt> random_move(const Tour& tour, RNG& rng) const
    {
        const auto n = tour.order.size();
        if (n < 4)
            return std::nullopt;
        std::uniform_int_distribution<std::size_t> pick{0, n - 1};
        while (true)
        {
            auto i = pick(rng);
            auto j = pick(rng);
            if (j < i)
                std::swap(i, j);
            if (i + 2 <= j && !(i == 0 && j + 1 == n))
                return TwoOpt{i, j};
        }
    }

    bool is_valid(const Tour& tour, const TwoOpt& move) const
    {
        const auto n = tour.order.size();
        return move.i + 2 <= move.j && move.j < n && !(move.i == 0 && move.j + 1 == n);
    }

    void make_move(Tour& tour, const TwoOpt& move) const
    {
        std::ranges::reverse(std::span{tour.order}.subspan(move.i + 1, move.j - move.i));
    }
};
```

| EasyLocal 3 | EasyLocal 4 |
| --- | --- |
| base `NeighborhoodExplorer<Input, Solution, Move, CostStructure>`, constructor `(in, sm, name)` | `neighborhood_explorer_base<SolutionManager, Move>`, inherited constructor |
| the name in the constructor | a static `name()`, used by the tester |
| `FirstMove` (throws `EmptyNeighborhood`), `NextMove` | `first_move`, `next_move`, both returning `false` when there is no move; or a generator `moves(solution)` ([chapter 3](tutorial/03-neighborhood.md)) |
| `RandomMove(st, mv)` (throws `EmptyNeighborhood`) | `random_move(solution, rng)`, returning `std::nullopt` when there is no move |
| `FeasibleMove` | `is_valid` |
| `MakeMove` | `make_move` |
| `AddDeltaCostComponent` | the recipe: `neighborhood<E>() \| delta<C, D>()` |
| the `Inverse` function given to `TabuSearch` | `inverse(solution, move, tabu_move)` in the explorer |

The bodies of the members barely change: `FirstMove` writes the first move into
`move` and returns `true` instead of throwing, and `RandomMove` returns the move
instead of writing it.

### 9. Several neighborhoods

A `MultimodalNeighborhoodExplorer` (set union with biases) becomes a
`neighborhood_union` of the explorers' recipes, each with its own deltas:

<!-- snippet: tutorial/main.cpp:union -->
```cpp title="EasyLocal 4"
auto both =
    el::neighborhood_union(
        el::neighborhood<TwoOptExplorer>()
            | el::delta<TourLength, TwoOptLengthDelta>(),
        el::neighborhood<SwapExplorer>())
    | el::random_biases(3.0, 1.0);

auto union_sa =
    el::make_runner<runners::SimulatedAnnealing<Classic>>({
        .temperature =
            {
                .initial_temperature = 10.0,
                .final_temperature = 0.1,
                .cooling_rate = 0.95,
                .samples_per_temperature = 50,
            },
    })
    | sm | both;
```

The moves of a union are a variant of the explorers' moves, so the
`ActiveMove` bookkeeping of EasyLocal 3 is no longer written by hand. A
component is evaluated by deltas only when every child has one
([chapter 6](tutorial/06-combining-neighborhoods.md)).

## Composing and running

### 10. The wiring becomes a recipe

In EasyLocal 3 the components were objects, created in `main` from the Input
and registered into each other; they had to stay alive as long as the search:

```cpp title="EasyLocal 3"
Input in(instance);
TspSolutionManager sm(in);
TourLength length(in);
TwoOptNeighborhoodExplorer nhe(in, sm);
TwoOptTourLengthDelta delta(in, length);
sm.AddCostComponent(length);
nhe.AddDeltaCostComponent(delta);
```

In EasyLocal 4 that graph is written once, before any Input exists, as two
*recipes*: which components form the cost, and which deltas the neighborhood
has. The recipe says what to build; the framework builds it when a runner is
bound to an Input.

<!-- snippet: tutorial/main.cpp:sm-recipe -->
```cpp title="EasyLocal 4"
auto sm = el::solution_manager<TourManager>() | el::component<TourLength>();
```

<!-- snippet: tutorial/main.cpp:nhe-recipe -->
```cpp title="EasyLocal 4"
auto nhe =
    el::neighborhood<TwoOptExplorer>() | el::delta<TourLength, TwoOptLengthDelta>();
```

The cost expression goes in the first recipe (step 6), the deltas in the
second (step 7), and a runner is the algorithm plus the two:
`make_runner<runners::FirstImprovement>() | sm | nhe`. An **app** adds a name
to each runner, which is what the command-line program, the tester and the
REST service run (step 11). Nothing is registered into anything, and nothing
has to outlive the search: a recipe is a value that can be copied, returned
from a function and shared between programs
([chapter 11](tutorial/11-apps-and-tools.md)).

### 11. The main program

A typical EasyLocal 3 `main` declared the parameters, built every object and
linked them, then either opened the tester or solved with the method named on
the command line:

```cpp title="EasyLocal 3"
int main(int argc, const char* argv[])
{
    ParameterBox main_parameters("main", "Main Program options");
    Parameter<std::string> instance("instance", "Input instance", main_parameters);
    Parameter<int> seed("seed", "Random seed", main_parameters);
    Parameter<std::string> method("method", "Solution method (empty for tester)", main_parameters);
    Parameter<std::string> output_file("output_file", "Write the output to a file", main_parameters);
    CommandLineParameters::Parse(argc, argv, false, true);

    if (!instance.IsSet())
        return 1;
    if (seed.IsSet())
        Random::SetSeed(seed);

    Input in(instance);
    // ... the objects and their links, as in step 10

    FirstDescent<Input, Tour, TwoOpt, CostStructure> fd(in, sm, nhe, "FD");
    SimulatedAnnealing<Input, Tour, TwoOpt, CostStructure> sa(in, sm, nhe, "SA");

    Tester<Input, Tour, CostStructure> tester(in, sm);
    MoveTester<Input, Tour, TwoOpt, CostStructure> two_opt_test(in, sm, nhe, "2-opt", tester);

    SimpleLocalSearch<Input, Tour, CostStructure> solver(in, sm, "TSP solver");
    if (!CommandLineParameters::Parse(argc, argv, true, false))
        return 1;

    if (!method.IsSet())
    {
        tester.RunMainMenu();
        return 0;
    }
    if (method == std::string("FD"))
        solver.SetRunner(fd);
    else
        solver.SetRunner(sa);
    auto result = solver.Solve();
    std::cout << "cost " << result.cost.total << '\n';
    if (output_file.IsSet())
        std::ofstream(output_file) << result.output;
    else
        std::cout << result.output;
}
```

The same program in EasyLocal 4 is `examples/tutorial/cli_main.cpp`: an app
that names the services and registers each runner under the name the command
line uses, and `cli::run`, which does the rest of `main`:

<!-- snippet: tutorial/cli_main.cpp:cli -->
```cpp title="EasyLocal 4"
auto application = el::app("tsp")
    | (el::solution_manager<TourManager>() | el::component<TourLength>())
    | (el::neighborhood<TwoOptExplorer>()
        | el::delta<TourLength, TwoOptLengthDelta>())
    | el::runner<runners::FirstImprovement>("fi")
    | el::runner<runners::SimulatedAnnealing<Classic>>(
        "sa",
        {.temperature = {.samples_per_temperature = 50}});

return el::cli::run(application, argc, argv);
```

| EasyLocal 3 | `cli::run` |
| --- | --- |
| `ParameterBox main_parameters`, `Parameter<T>`, `CommandLineParameters::Parse` | `--instance`, `--seed`, `--runner`, `--start`, `--solution`, `--output`, and `--config <file>` |
| `--main::method` and `SetRunner` | `--runner fi`: a runner registered in the app, by name |
| the runners' parameters, `--SA::cooling_rate` | `--runners.sa.temperature.cooling_rate`; `--help` lists them all, with their domains and current values |
| `Random::SetSeed(seed)` | `--seed`, the seed of the run's random generator |
| `--main::init_state` | `--solution <file>`, or `--start initial` / `random` |
| `LowerBoundReached`, a runner's `max_evaluations` and timeout | `--target`, `--max_evaluations`, `--timeout`, which bound the run whichever runner it uses |
| `PrintViolations` on the final state | `--report`: the value of each cost component, with its description |
| a `Trace::Channel` with a sink, set up in `main` | `--trace run.jsonl` ([what EasyLocal 4 adds](#what-easylocal-4-adds)) |
| a tuning scenario written by hand | `--tuning.irace <dir>` ([chapter 12](tutorial/12-tuning.md)) |
| `solver.Solve()`, printing `result.cost` and `result.output` | the run, then `cost`, `time`, the iterations, evaluations and termination, and the solution, or `--output <file>` |
| `IsSet()` checks | the parameters' own validation, with an exit status of 2 |

The program runs as before, with plain switches in place of `--main::`:

```text title="EasyLocal 4 — output"
$ easylocal_tutorial_cli --instance five.tsp --runner fi --seed 1
cost 26
time 1.9125e-05
iterations 2
evaluations 9
termination local optimum
0 1 3 4 2
```

Parameters of the program's own, such as the biases of EasyLocal 3 programs
that were not runner parameters, are given to `cli::run` as a parameter set,
`el::cli::run(application, argc, argv, {.program_parameters = own})`, and
parsed with the others ([chapter 11](tutorial/11-apps-and-tools.md)).

When a program needs a solver rather than a single run, `make_solver` wraps a
runner ([chapter 8](tutorial/08-solvers.md)): `solvers::LocalSearch` is the
counterpart of `SimpleLocalSearch`, `solvers::MultiStart` of
`MultiStartSearch`. A solve in stages is step 13.

### 12. The tester

The EasyLocal 3 tester was written by hand in the library, with no other
dependency: a `Tester` on the SolutionManager, a `MoveTester` for each
explorer, and text menus read from the standard input:

```cpp title="EasyLocal 3"
Tester<Input, Tour, CostStructure> tester(in, sm);
MoveTester<Input, Tour, TwoOpt, CostStructure> two_opt_test(in, sm, nhe, "2-opt", tester);
tester.RunMainMenu();
```

```text title="EasyLocal 3 — the tester's main menu"
MAIN MENU:
   (1) Move menu
   (3) Run menu
   (4) State menu
   (0) Exit
 Your choice:
```

In EasyLocal 4 the tester is the TextUI, an interactive terminal interface
built on FTXUI in the optional `TUI` component ([chapter 13](tutorial/13-tester.md)).
It takes the same app as `cli::run`: no tester object, no move tester to
register, since the app already names the neighborhood and the runners:

<!-- snippet: tutorial/tui_main.cpp:tui -->
```cpp title="EasyLocal 4"
auto application = el::app("tsp")
    | (el::solution_manager<TourManager>() | el::component<TourLength>())
    | (el::neighborhood<TwoOptExplorer>()
        | el::delta<TourLength, TwoOptLengthDelta>())
    | el::runner<runners::FirstImprovement>("fi")
    | el::runner<runners::SimulatedAnnealing<Classic>>("sa");

el::tui::run(
    application,
    {
        .title = "TSP tester",
        .seed = 2026, // the RNG for random solutions, moves and stochastic runners
        .input_path = EASYLOCAL_TUTORIAL_INSTANCE, // loaded with the read_input hook
    });
```

The three menus become three pages, switched with F3, F4 and F5, each with
its keys:

| EasyLocal 3, Move menu | TextUI, **Move** page |
| --- | --- |
| (1) Perform Best Move | `B` Best, then `A` Apply |
| (2) Perform First Improving Move | `I` First improving, then `A` |
| (3) Perform Random Move | `R` Random, then `A` |
| (5) Print All Neighbors | `P` List |
| (6) Print Neighborhood Statistics | `T` Stats |
| (7) Print Random Move Cost | the panel of the selected move: its incremental and full evaluation |
| (9) Check Neighborhood Costs | `C` Costs |
| (10) Check Move Independence | `D` Indep |
| (11) Check Random Move Distribution | `U` Distribution |
| (4), (8) Perform / Print Input Move | no counterpart: a move is selected, not typed |

![The Move page of the TextUI, with the best 2-opt move and its delta check](tutorial/images/tui-moves.svg)

| EasyLocal 3, State menu | TextUI, **Input/Output** page |
| --- | --- |
| (1) Random state, (2) Greedy state | `R` Random, `I` Initial |
| (3) Read from file, (5) Write to file | `Shift-L` Load solution, `W` Save |
| (4) Show input | the Input window (F1), which shows `describe(input)` |
| (6) Show state, (7) Show costs, (8) Print violations, (9) Pretty print output | the solution window (`S` or F2): the solution, each cost component's value and its `describe` text (step 6) |
| (10) Check state consistency | `C` Check: the app check of [chapter 14](tutorial/14-checking.md) |
| — | `L` Load input: the instance is changed without leaving the tester |

The **Run** page takes over the Run menu: it runs a registered runner from the
current solution, after editing its parameters in a window (the same ones a
configuration file sets), with live progress, a target cost, a time and
evaluation budget, and a stop key:

![The Run page of the TextUI after a Simulated Annealing run](tutorial/images/tui-run.svg)

An EasyLocal 3 tester could hold several move testers, one per explorer, on
the same state. The moves an EasyLocal 4 tester shows are those of its app's
neighborhood (a runner may bring its own, for searching), so the counterpart
is a launcher of apps, one per neighborhood, which share the Input and the
current solution ([chapter 13](tutorial/13-tester.md#several-apps-on-the-same-problem)):

```cpp title="EasyLocal 3"
MoveTester<Input, Tour, TwoOpt, CostStructure> two_opt_test(in, sm, two_opt_nhe, "2-opt", tester);
MoveTester<Input, Tour, Swap, CostStructure> swap_test(in, sm, swap_nhe, "Swap", tester);
```

<!-- snippet: tsp/tui_main.cpp:launcher -->
```cpp title="EasyLocal 4"
easylocal::tui::run_launcher(
    {
        .title = "EasyLocal TSP Tester",
        .tester =
            {
                .seed = 0,
                .input_path = EASYLOCAL_TSP_INSTANCE_FILE,
                .solution_path = EASYLOCAL_TSP_SOLUTION_FILE,
            },
    },
    tsp::two_opt_app(),
    tsp::swap_app(),
    tsp::union_app());
```

There `two_opt_app()` and `swap_app()` are two apps of the TSP example
(`examples/tsp/apps.hpp`) with the same SolutionManager recipe, one with the
2-opt neighborhood and one with the swaps.

### 13. A solve in two stages

The `main` of a real EasyLocal 3 solver is usually longer than the one of
step 11.
A common shape is a solve in two stages: first a search on the hard
constraints only, until the solution is feasible, then a search on the whole
cost from there. EasyLocal 3 could not evaluate only part of a cost, so the
program built a second SolutionManager with the hard components alone, a
second copy of each explorer and of their deltas, a second runner and a second
solver. Parameters were often changed after the instance was read, and the
final report printed the value of each component. On the TSP, with the edges
longer than 8 as violations (a component `MaxEdgeExcess`), the result looked
like this:

```cpp title="EasyLocal 3"
Input in(instance);
MaxEdgeExcess cc1(in, 1, true);   // hard
TourLength cc2(in, 1, false);     // soft
TwoOptDeltaMaxEdgeExcess dcc1(in, cc1);
TwoOptTourLengthDelta dcc2(in, cc2);

TspSolutionManager sm(in), smH(in);   // the whole cost, the hard cost only
TwoOptNeighborhoodExplorer nhe(in, sm), nheH(in, smH);
sm.AddCostComponent(cc1);
sm.AddCostComponent(cc2);
smH.AddCostComponent(cc1);
nhe.AddDeltaCostComponent(dcc1);
nhe.AddDeltaCostComponent(dcc2);
nheH.AddDeltaCostComponent(dcc1);

FirstDescent<Input, Tour, TwoOpt, CostStructure> fd(in, sm, nhe, "FD");
FirstDescent<Input, Tour, TwoOpt, CostStructure> fdH(in, smH, nheH, "FDH");
SimpleLocalSearch<Input, Tour, CostStructure> solver(in, sm, "solver"), solverH(in, smH, "solverH");
solver.SetRunner(fd);
solverH.SetRunner(fdH);
if (!CommandLineParameters::Parse(argc, argv, true, false))
    return 1;

if (evaluations_per_city.IsSet())   // depends on the instance
{
    fd.SetParameter("max_evaluations", evaluations_per_city * in.city_count);
    fdH.SetParameter("max_evaluations", evaluations_per_city * in.city_count);
}

auto stage_one = solverH.Solve();               // from a random state
auto result = solver.Resolve(stage_one.output); // from the stage-one solution
std::cout << "violations " << result.cost.violations << ", length " << result.cost.objective << '\n';
std::cout << "TourLength " << cc2.Cost(result.output) << ", MaxEdgeExcess " << cc1.Cost(result.output) << '\n';
```

The same program in EasyLocal 4 is `examples/tutorial/staged_main.cpp`. The
duplicated objects disappear: there is one recipe for the cost, one for the
neighborhood, and one runner, and the solver derives the hard-only stage from
it:

<!-- snippet: tutorial/staged_main.cpp:staged-recipes -->
```cpp title="EasyLocal 4"
// One hierarchical cost: edges longer than 8 are violations (hard), the
// length is the objective (soft). EasyLocal 3 needed a SolutionManager
// for each set of components (all, hard only); with_hard_cost() derives
// the hard-only runner from this one.
auto sm = el::solution_manager<TourManager>()
    | el::cost::hard_soft(
        el::cost::apply(
            [](double longest) { return std::max(0.0, longest - 8.0); },
            el::component<MaxEdge>()),
        el::component<TourLength>());

// The explorer and its deltas are written once: the hard-only stage
// ignores the delta of the soft TourLength.
auto descent =
    el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
    | sm
    | (el::neighborhood<TwoOptExplorer>()
        | el::delta<TourLength, TwoOptLengthDelta>());
```

`cli::run` runs runners by name, not solvers, so this program still reads its
parameters itself, as `load_and_apply` does
([chapter 9](tutorial/09-configuration.md)). Those that depend on the instance
are set on the runner once the Input is loaded, before the solver copies it:

<!-- snippet: tutorial/staged_main.cpp:staged-instance -->
```cpp title="EasyLocal 4"
// Parameters that depend on the instance, set once it is read, as
// EasyLocal 3's SetParameter("max_evaluations", ...) after the parsing.
const auto tsp = el::load_input<Tsp>(main_parameters.instance);
if (main_parameters.evaluations_per_city != 0)
    descent.parameters().max_evaluations =
        main_parameters.evaluations_per_city * tsp.cities();
```

`solvers::two_stage()` replaces the two solvers and the `Resolve` that passed
the solution from one to the other:

<!-- snippet: tutorial/staged_main.cpp:staged-run -->
```cpp title="EasyLocal 4"
// EasyLocal 3's two solvers, one per SolutionManager, and the Resolve that
// passed the solution from one to the other: the descent on the hard cost
// from a random tour until it is feasible, then on the whole cost.
auto stages =
    el::solvers::two_stage(descent)
        .initialization(el::initialization::random)
        .seed(main_parameters.seed);
const auto result = stages.solve(tsp);
```

- The first stage evaluates only the components of the hard branch of
  `cost::hard_soft`, and stops as soon as the hard cost is zero; the second
  continues from its solution on the whole cost.
- A delta attached to a soft component, `TwoOptLengthDelta` here, is ignored by
  the first stage and used by the second.
- One runner serves both stages; `two_stage(first, second)` takes two, for
  example with different parameters or neighborhoods.
- `result.iterations` and `result.evaluations` add up both stages, and
  `result.stages` reports each one.
- `two_stage()` is a pipeline of two stages, `(stage("first", descent) &
  until_feasible()) | stage("second", descent)` in `solvers`: a solver of more
  stages, each with its own runner, neighborhood and cost, is written the same
  way (see [Solvers](reference/solvers.md)). A stage can also be repeated:
  `& attempts(10)` on the first one restarts it from new random tours while
  the tour is not feasible.

The report reads the cost by branch and evaluates each component directly:
components are plain classes, constructed from the Input:

<!-- snippet: tutorial/staged_main.cpp:staged-report -->
```cpp title="EasyLocal 4"
std::cout << "violations " << result.cost.hard() << ", length " << result.cost.soft()
          << '\n'
          << "iterations " << result.iterations // of both stages
          << ", termination " << el::to_string(result.termination) << '\n';
// The value of each component: components are plain classes, constructed
// from the Input (EasyLocal 3's cc.Cost(out) on each component).
std::cout << "TourLength " << TourLength{tsp}.evaluate(result.solution)
          << ", MaxEdge " << MaxEdge{tsp}.evaluate(result.solution) << '\n';
el::write_solution(tsp, result.solution, std::cout);
```

```text title="EasyLocal 4 — output"
$ easylocal_tutorial_staged --main.instance five.tsp --main.seed 1
violations 0, length 26
iterations 3, termination local optimum
TourLength 26, MaxEdge 8
0 4 2 3 1
```

## Checking the port

The checks EasyLocal 3 offered inside the tester are available in a program
too, so a port can be verified in the test suite rather than by hand. In the
order to use them:

- **While you write a delta.** Compiling with `EASYLOCAL_VERIFY_DELTAS`
  defined makes every search compare the component values its deltas gave with
  a full evaluation after each move it keeps, and stop at the first
  disagreement naming the component
  ([chapter 4](tutorial/04-delta-evaluation.md)). This is the fastest way to
  catch a delta that was right in EasyLocal 3 and wrong after the port, for
  example because the move's attributes changed meaning.
- **For one component.** `testing::check_cost_component`,
  `check_delta_cost_component` and `check_neighborhood` check a component
  against a fixture of solutions, in a unit test
  ([chapter 10](tutorial/10-testing.md)): the counterpart of EasyLocal 3's
  `ComponentTester`, without the menu.
- **For the whole app.** `check(app, input)` builds everything, runs the
  checks above from the initial solution and from random ones, and reports
  them; it also catches what EasyLocal 3 could not check at all, such as a
  parameter without a domain or two runners with the same name
  ([chapter 14](tutorial/14-checking.md)):

<!-- snippet: tutorial/main.cpp:check -->
```cpp title="EasyLocal 4"
const auto report = el::check(application, tsp); // also: check(app, input, solution)
el::print_report(std::cout, report);
if (!report)
    return 1;
```

- **For a neighborhood, as the `MoveTester` did.** A Session runs the three
  checks of the Move menu on the current solution, and returns counters
  instead of printing:

<!-- snippet: tutorial/main.cpp:session-checks -->
```cpp title="EasyLocal 4"
// Each check enumerates the neighborhood of the current solution and
// returns a struct of counters (Session::..._result).

// The delta evaluation of each move against the full evaluation of the
// solution it leads to: moves (enumerated), mismatches (the two costs
// differ), invalid (moves that are not valid or lead to an invalid
// solution).
const auto costs = session.check_neighborhood_costs();

// What each move does to the solution: moves, null_moves (moves that leave
// it unchanged), repeated_states (moves that lead to a solution an earlier
// move reached), invalid.
const auto independence = session.check_move_independence();

// random_move against the enumeration: neighborhood_size (valid enumerated
// moves), samples (draws, 20 per move by default), out_of_neighborhood
// (draws that return no move or one the enumeration does not contain),
// unseen (enumerated moves never drawn), min_frequency and max_frequency
// (how often the least and the most drawn moves came up).
const auto sampling = session.check_random_move_distribution();

if (costs.mismatches != 0 || costs.invalid != 0 || sampling.out_of_neighborhood != 0)
    return 1;
```

The TextUI runs the same checks on the Move page, and `check(app, input)` on
the Input/Output page (step 12), so a port can be inspected interactively and
then nailed down in a test.

## What EasyLocal 4 adds

These have no EasyLocal 3 counterpart to port, but they replace things
EasyLocal 3 programs wrote by hand:

- **Tracing.** `easylocal::trace` records typed events of a run — `run_started`,
  `move_evaluated`, `move_accepted`, `incumbent_updated`, `local_optimum`,
  `temperature_changed`, `run_finished` and the Tabu Search ones — through a
  tracer: `memory_recorder` in a program, JSON Lines or the binary ELTR format
  with `--trace run.jsonl` ([chapter 16](tutorial/16-observing-and-controlling.md)
  and [Tracing](tracing.md)). EasyLocal 3's `Trace::Channel`, its sinks and its
  event mask are the same idea; here the events are typed and the tracer is a
  template parameter of the run, so an unused tracer costs nothing.
- **Control over a run.** `run_control` carries a `std::stop_token` and a
  progress observer, and `with(control)`, `stop_at(cost)` and `timeout(s)` are
  the run's options: a frontend stops a run from another thread, which
  EasyLocal 3 did with `Interruptible`.
- **Configuration files.** Every parameter block has a schema, so the same
  names serve the command line, a key-value file (`--config`), a TOML file
  with sections (the optional `ConfigTOML` component) and the tester's
  parameter window ([chapter 9](tutorial/09-configuration.md)).
- **Tuning.** Because the schema declares domains and conditions,
  `--tuning.irace <dir>` writes a complete irace scenario from the program's
  own parameters ([chapter 12](tutorial/12-tuning.md)).
- **A REST service.** The same app, exposed over HTTP with the optional `REST`
  component: clients submit runs, poll progress, cancel them and fetch the
  solutions ([chapter 15](tutorial/15-rest.md)).
- **Several objectives.** `cost::objectives` makes the cost a Pareto vector,
  a run returns the non-dominated solutions it reached, and
  `runners::ParetoLateAcceptanceHillClimbing` searches for a front
  ([chapter 2](tutorial/02-cost.md)).
- **Solvers as pipelines.** Stages with their own runner, cost and
  neighborhood, repeated or conditional, in place of a `Solver` subclass
  ([chapter 8](tutorial/08-solvers.md)).

## A porting checklist

When the port compiles, before trusting its results:

1. `check(app, input)` passes on a real instance, and on the smallest one.
2. A run with `EASYLOCAL_VERIFY_DELTAS` finishes without a delta disagreement.
3. The cost of a solution read back from a file is the cost the program
   printed: the I/O hooks of step 4 agree with each other.
4. The hard constraints are still hard. If the EasyLocal 3 program weighted
   them into the cost, decide between `cost::hard_soft` and one weighted sum
   (step 6), and remember that Simulated Annealing behaves differently under
   the two.
5. The runner parameters have the values the EasyLocal 3 program used:
   `--help` lists them with their current values, and the names changed.
6. The seeds: a run is reproducible given `--seed`, and the results of the two
   versions are compared over several seeds, never on one run.
7. Every parameter the program adds declares a domain, or `check` fails: that
   is also what makes it tunable (chapter 12).

## Next steps

The [tutorial](tutorial/README.md) builds the same TSP one capability per
chapter, and the [reference](reference/README.md) describes every component in
full.
