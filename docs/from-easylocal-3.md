# Coming from EasyLocal 3

This page is for readers with an EasyLocal 3 program to port. It migrates a
complete EasyLocal 3 program, the TSP with 2-opt moves, piece by piece, up to
the TSP of the [tutorial](tutorial/README.md): first the
[problem](#porting-the-problem), then the
[composition and the run](#composing-and-running), then
[checking](#checking-the-port) that the port does what the original did.
Two reference sections close it: the
[parameters](#parameters-configuration-and-tuning) of a program and the
[tables](#reference-the-two-frameworks-side-by-side) that put the two
frameworks side by side, concept by concept and runner by runner.

The EasyLocal 3 code quoted here is the TSP of the [benchmarks](benchmarks.md)
that compare the two versions, abridged, on the last EasyLocal 3 release,
[easylocal-legacy v3.4.1](https://github.com/iolab-uniud/easylocal-legacy/tree/v3.4.1);
the EasyLocal 4 code is the tutorial's, which is compiled and tested at every
build.

*:material-file-pdf-box: This section is also [a PDF](https://iolab-uniud.github.io/easylocal/pdf/easylocal-coming-from-easylocal-3.pdf), built with the site.*{ .pdf-of-this-section }

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
- Look up the runners and the solvers your program uses in the
  [tables at the end](#reference-the-two-frameworks-side-by-side) before you
  start: most have a counterpart under another name, a few have none yet.
- Two keywords appear in the code of this page and are worth a line each,
  since EasyLocal 3 programs rarely used them. Both are recommendations, not
  requirements.

    **`inline`**, on the free functions a problem defines in a header
    (`read_solution`, `describe`, the hooks of step 4). A function defined in a
    header is compiled into every file that includes it, and the linker
    refuses the copies; `inline` tells it that the copies are one function and
    to keep one. Define those functions in a `.cpp` instead and `inline` is not
    needed. It is not about speed: the compiler inlines what it wants either
    way.

    **`[[nodiscard]]`**, which the library writes on the functions whose result
    is the point of calling them — `check(app, input)`, the `apply` of a
    parameter set, the `random_move(explorer, solution, rng)` that returns an
    optional move. The compiler then warns when a call ignores
    the result, which is how a forgotten check is caught: a program that calls
    `check(application, tsp)` and never looks at the report is warned, and the
    examples of this page read `if (!report) return 1;` for that reason. Your
    own classes need none of it; write it where silently dropping the result
    would be a bug, and leave it out of the hooks the library calls, as the
    examples do.

### Building without CMake

The library is header-only and has no dependency beyond the standard library,
so a program that uses EasyLocal Core needs no build system of its own: a
compiler, the C++23 switch and one include path are enough. A hand-written
makefile for a program of several translation units is therefore as short as
it was for EasyLocal 3, and shorter, since there is nothing to link:

```make
EASYLOCAL ?= /usr/local            # the install prefix, or the source tree
CXX       ?= g++
CXXFLAGS  := -std=c++23 -O2 -Wall -I$(EASYLOCAL)/include

OBJECTS := main.o model.o
tsp: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f tsp $(OBJECTS)
```

- `$(EASYLOCAL)/include` is `include/` of an installed EasyLocal
  (`cmake --install <build> --prefix <prefix>`) or of a source checkout: the
  headers are the same, and nothing is generated by the build.
- There is no library to link, no `-D` to define and no Boost: where EasyLocal
  3 needed `-lboost_program_options`, the command line is parsed by headers.
  Core starts no thread of its own; add `-pthread` if your platform asks for it
  or if your program uses threads.
- Compile with optimizations. The library is written to be inlined, and a
  `-O0` build of a search is slower by an order of magnitude; `-ffast-math` is
  the one flag to avoid, since the library checks for NaN and infinite values.
- The optional components are the exception: the TextUI, REST and TOML
  adapters are header-only too, but they need their third-party library
  (FTXUI, Crow, toml++) on the include path and, for FTXUI and Crow, linked.
  Those are easier to let CMake fetch and configure
  ([dependency policy](dependency-policy.md)).

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

<!-- snippet: tutorial/tsp.hpp:model!solution-value!move-value -->
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

The EasyLocal 4 Solution is the same data as a value that can be copied, moved
and assigned:

<!-- snippet: tutorial/tsp.hpp:solution-value -->
```cpp title="EasyLocal 4"
// Solution: order[k] is the k-th city visited; after the last city the tour
// returns to order[0].
struct Tour
{
    std::vector<std::size_t> order;
};
```

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

In EasyLocal 4 the Move is a `struct`, or a `class`, whose attributes are
public, and it needs no constructor:

<!-- snippet: tutorial/tsp.hpp:two-opt-move -->
```cpp title="EasyLocal 4"
// Move: reverse the part of the tour between positions i + 1 and j.
struct TwoOpt
{
    std::size_t i;
    std::size_t j;
};
```

- **Public attributes**, because the code that builds a move and the code that
  reads it are not the same class: the explorer writes `move.i` and `move.j`
  while it enumerates, and the delta cost component of step 8 reads them to
  compute the change in cost. A move carries no invariant of its own — whether
  a pair of positions is a legal 2-opt is the explorer's `is_valid`, not the
  move's — so there is nothing for a `private` section to protect, and the
  library itself never calls a member on a move. A `class` with its attributes
  `public` is the same thing; the examples write `struct` for a type whose
  fields may each be set freely.
- **No constructor**: with no user-declared constructor the type is an
  aggregate, so it is created with braces, `TwoOpt{i, j}` or
  `TwoOpt{.i = i, .j = j}`, as the explorer of step 7 does, and it is
  default-constructible, `TwoOpt{}` with zero attributes, which the cursor of
  step 7 needs as something to write into. Prefer these implicit constructors
  to the EasyLocal 3 one with default arguments.
- **No operator is required.** EasyLocal 3 asked for `==`, `!=`, `<` and
  `<<`; here `<` and `!=` are never called, and the other two are used only
  where they exist:
  - `operator==`, or a defaulted `bool operator==(const TwoOpt&) const =
    default;` written inside the struct, which keeps it an aggregate, lets the
    neighborhood checks tell two moves apart
    ([checking the port](#checking-the-port));
  - `operator<<`, or better `describe(move)` (step 4), gives the text the
    interactive tester and the reports show for a move.

  A move also needs `std::hash` and `==` to be kept in a tabu list, unless its
  explorer defines `tabu_attribute(move)`.

### 4. Reading, writing and describing

EasyLocal 3 read the Input in the Input's constructor and the State with
`operator>>`, and printed it with `operator<<`. **Those operators keep
working**: the library uses them when nothing more specific is there, so a
port can leave them untouched and come back to this step later.

```cpp title="EasyLocal 4 — what an EasyLocal 3 program already has"
// Read into a default-constructed Tsp, in place of the file constructor.
std::istream& operator>>(std::istream& in, Tsp& tsp);

// Read into Tour{tsp}: on this path the Solution keeps the constructor from
// the Input that step 2 could otherwise drop.
std::istream& operator>>(std::istream& in, Tour& tour);

// Written as it was, by the tester, cli::run and the REST service.
std::ostream& operator<<(std::ostream& out, const Tour& tour);
```

What the operators cannot do is receive the Input, which reading a solution
usually needs — here the number of cities, which the Solution no longer holds.
The dedicated hooks are free functions next to the types, found by
argument-dependent lookup, and the library prefers them when they are there:
`read_input(std::type_identity<Input>, in)` returns the Input, the tag
selecting it by the type it reads;
`read_solution(const Input&, std::istream&)` returns the Solution and
`write_solution(const Input&, const Solution&, std::ostream&)` writes it; and
`describe(value)` gives the text the tools show for an Input, a Solution or a
Move. The two solution hooks of the tutorial's TSP are the counterpart of the
two State operators above:

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

For each of the three the library looks for the same three forms, in this
order, and uses the first it finds — so writing a hook overrides the operator
without removing it:

| EasyLocal 3 | EasyLocal 4, in the order the library tries them |
| --- | --- |
| `Input(const std::string& path)` reading the file | a static `Input::read(in)`, `read_input(std::type_identity<Input>, in)`, or `operator>>` into a default-constructed Input |
| `operator>>(is, State&)` | a static `Solution::read(input, in)`, `read_solution(input, in)`, or `operator>>` into `Solution{input}` |
| `operator<<(os, const State&)` | a member `solution.write(input, out)`, `write_solution(input, solution, out)`, or `operator<<` |
| `PrintViolations`, "pretty print output" | `describe(input)`, `describe(solution)`, `describe(move)`, and the `describe(solution)` of a cost component (step 6) |

Whichever form they take, they are read and written in one place: the file
helpers (`load_input`, `load_solution`), the interactive tester, `cli::run`
and the REST service all go through them, and `main` never opens a file
itself ([chapter 5](tutorial/05-running-a-search.md#reading-the-instance-printing-the-solution)).

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
    // The constructor of the base: from the Input, which input() gives back.
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
class TourLength : public easylocal::input_base<Tsp>
{
public:
    using input_base::input_base; // constructed from the Input, read by input()

    double evaluate(const Tour& tour) const
    {
        const auto n = tour.order.size();
        double length = 0.0;
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto from = tour.order[k];
            const auto to =
                tour.order[(k + 1) % n]; // the last city goes back to the first
            length += input().distance[from][to];
        }
        return length;
    }
};
```

- `ComputeCost` becomes `evaluate`. The base is no longer required and carries
  no virtual function: `input_base<Tsp>` only gives the constructor from the
  Input and `input()`, and a component that needs neither derives from
  nothing.
- The weight and the hard flag leave the constructor and go into the cost
  expression of the recipe. Take a program whose cost components are `H1` and
  `H2`, hard, and `S1` (weight 1) and `S2` (weight 5), soft — four classes
  like `TourLength` above, each with its own `evaluate`. Its cost becomes:

    ```cpp title="EasyLocal 4"
    el::solution_manager<Manager>()
        | el::cost::hard_soft(
            el::cost::sum(el::component<H1>(), el::component<H2>()),
            el::cost::sum(el::component<S1>(), el::component<S2>() * 5))
    ```

    `HARD_WEIGHT` is gone. `cost::hard_soft` makes the cost a pair compared
    **lexicographically**: the hard cost decides, and the soft cost is looked
    at only between solutions whose hard costs are equivalent. No soft gain,
    however large, can pay for a violation, which a large weight only made
    unlikely. When the weights of the hard components mattered to guide the
    search, keep them inside the hard sum.

    A lexicographic pair is not a number, and Simulated Annealing needs one:
    it accepts a worsening move with probability `exp(-delta / temperature)`.
    The rule that turns the pair into a number comes with `cost::hard_soft`,
    and it is the strict one — the change of the soft cost while the hard cost
    is unchanged, minus infinity when the hard cost improves, plus infinity
    when it worsens — so the Metropolis criterion never accepts a hard
    degradation. From an infeasible solution the search still walks towards
    feasibility, since a move that lowers the hard cost is always accepted;
    once there it stays, where EasyLocal 3, with the hard cost weighted into
    the one number, accepted a violation now and then and crossed the
    infeasible region to the other side. That is a real change of behaviour,
    worth knowing before comparing the two programs.

    To keep EasyLocal 3's behaviour, do what it did: one weighted sum,
    `el::cost::sum(el::component<H1>() * 1000, ..., el::component<S1>())`,
    which is a number already, and check that the result is feasible. A rule
    of your own goes in a `cost::apply` at the root of the expression
    ([Cost](reference/cost.md)).

- The member that corresponds to `PrintViolations` is
  `std::string describe(const Solution&) const`, optional, which returns the
  text instead of printing it, so that the tester, a report and an HTTP
  response can each put it where they need it. The name the component was
  given in its constructor is an optional `name()` beside it. Without them,
  reports show the component's position and its value
  ([chapter 11](tutorial/11-apps-and-tools.md#a-report-of-the-cost-components)).
- Nothing declares the type of the cost: it follows from what `evaluate`
  returns, so an `int` component stays `int`. A cost of more than the two
  levels of EasyLocal 3, or several objectives to keep apart, is a
  `cost::in_order` or a `cost::objectives` of components
  ([chapter 2](tutorial/02-cost.md)).

### 7. The NeighborhoodExplorer

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

An explorer lists the moves of a solution in one of two ways, and the port of
an EasyLocal 3 explorer uses the first.

A **cursor** is what EasyLocal 4 calls the pair `first_move`/`next_move`,
which EasyLocal 3 wrote as `FirstMove`/`NextMove`: no move is stored anywhere,
the caller holds the current one in a Move of its own and passes it by
reference, `first_move` writes the first into it and `next_move` turns it into
the following one; both return `false` when there is none left. An explorer
with a cursor is why a Move must be default-constructible (step 3): the caller
needs a Move to write into before the first one exists.

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
| the name in the constructor | a static `name()`, which the tester puts on its Move page |
| `FirstMove` (throws `EmptyNeighborhood`), `NextMove` | `first_move`, `next_move`, both returning `false` when there is no move; or a generator `moves(solution)` ([chapter 3](tutorial/03-neighborhood.md)) |
| `RandomMove(st, mv)` (throws `EmptyNeighborhood`) | `random_move(solution, rng)`, returning `std::nullopt` when there is no move |
| `FeasibleMove` | `is_valid` |
| `MakeMove` | `make_move` |
| `AddDeltaCostComponent` | the recipe: `neighborhood<E>() \| delta<C, D>()` |
| the `Inverse` function given to `TabuSearch` | `inverse(solution, move, tabu_move)` in the explorer |

The bodies of the members barely change: `FirstMove` writes the first move into
`move` and returns `true` instead of throwing, and `RandomMove` returns the move
instead of writing it.

A word on two things in that class that EasyLocal 3 had no equivalent of.

**`std::string_view`** (`name()`) is a read-only view of a string that someone
else owns: two words, a pointer and a length, copied as cheaply as an `int`
and with no allocation. Returning `"2-opt"` from a `static` member is
therefore free. A `std::string` works just as well — the library only asks for
something convertible to a view — at the price of allocating and copying the
text at each call; with a name that is a literal, the view is the natural
choice. The rule is the usual one: a view must not outlive the text it looks
at, which a literal always outlives.

**The other way of enumerating** is a `moves(solution)` generator, which an
explorer offers *instead of* the cursor. It is a coroutine: it computes one
move, hands it to the caller with `co_yield`, and goes on from there when the
caller asks for the next, so the moves are produced one at a time and none is
stored. The nested loops read as they would on paper:

<!-- snippet: tutorial/tsp.hpp:swap-moves -->
```cpp title="EasyLocal 4"
// Every pair of positions i < j, one move at a time.
easylocal::generator<SwapCities> moves(const Tour& tour) const
{
    const auto n = tour.order.size();
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 1; j < n; ++j)
            co_yield SwapCities{i, j};
}
```

This is the whole enumeration of a neighborhood of swaps: no state to carry
between calls, no `first`/`next` to keep in step, and the bound of each loop
written once. Both ways are lazy — a move is produced when the runner asks for
it, so First Improvement never produces the moves after the one it takes — and
no algorithm sees the difference; a generator is usually easier to write, a
cursor needs no coroutine and is the shape an EasyLocal 3 explorer already has
([chapter 3](tutorial/03-neighborhood.md) puts the two side by side). A port
can keep its cursor and rewrite it as a generator later, or never. An explorer
that has both is read with the cursor.

### 8. The delta cost components

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
class TwoOptLengthDelta : public easylocal::input_base<Tsp>
{
public:
    using input_base::input_base;

    // The tour goes a -> b ... c -> d; after the move it goes a -> c ... b -> d,
    // the segment b ... c reversed, at the same cost with symmetric distances.
    double delta_evaluate(const Tour& tour, const TwoOpt& move) const
    {
        const auto n = tour.order.size();
        const auto a = tour.order[move.i];
        const auto b = tour.order[move.i + 1];
        const auto c = tour.order[move.j];
        const auto d = tour.order[(move.j + 1) % n];
        const auto& distance = input().distance;
        return distance[a][c] + distance[b][d] - distance[a][b] - distance[c][d];
    }
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

### 9. Several neighborhoods

A `MultimodalNeighborhoodExplorer` (set union with biases) becomes a
`neighborhood_union` of the explorers' recipes, each with its own deltas:

<!-- snippet: tutorial/main.cpp:union!union-runner -->
```cpp title="EasyLocal 4"
auto both =
    el::neighborhood_union(
        el::neighborhood<TwoOptExplorer>()
            | el::delta<TourLength, TwoOptLengthDelta>(),
        el::neighborhood<SwapExplorer>())
    | el::random_biases(3.0, 1.0);
```

`random_biases(3.0, 1.0)` is what the biases of a multimodal explorer were:
when a runner draws a random move, the child is drawn with a probability
proportional to its bias, here three times out of four from the 2-opt moves.
**It is optional**: leave it out and every child has a bias of 1, so the
children are drawn with equal probability — note that this is equal per
*child*, not per move, so a neighborhood of ten moves is proposed as often as
one of a thousand. Set the biases when the children differ in size or in how
often they pay off; a bias of 0 leaves a child out of the random draws, while
the enumerations keep listing all of them. The biases are parameters like any
other (`--neighborhood.random_biases` in an app), so they can be changed
without recompiling and tuned with the rest.

The moves of a union are a variant of the explorers' moves, so the
`ActiveMove` bookkeeping of EasyLocal 3 is no longer written by hand. A
component is evaluated by deltas only when every child has one
([chapter 6](tutorial/06-combining-neighborhoods.md)).

## Composing and running

### 10. From the objects of `main` to a recipe

This is the step where an EasyLocal 3 program changes shape, so it is worth
reading even if the rest of the port was mechanical.

In EasyLocal 3 the components were objects. `main` read the instance, built
one object per component from it, and registered them into each other; the
whole graph had to stay alive for as long as the search, and a search on a
different cost, or on a second instance, meant a second graph:

```cpp title="EasyLocal 3"
Input in(instance);
TspSolutionManager sm(in);
TourLength length(in);
TwoOptNeighborhoodExplorer nhe(in, sm);
TwoOptTourLengthDelta delta(in, length);
sm.AddCostComponent(length);
nhe.AddDeltaCostComponent(delta);
```

EasyLocal 4 splits that into two things that happen at different times: a
**description**, written once and without an Input, and the **objects**, built
from the description for one Input when a search is about to run.

**The services are the same four kinds**, and they are still plain classes you
write: the SolutionManager, the cost components, the NeighborhoodExplorer and
the delta cost components of steps 5 to 8. What changed is that none of them
registers itself into another: `TourLength` does not know it belongs to a cost
function, and `TwoOptLengthDelta` does not know which component it is the delta
of. Each only takes what it needs to work — the Input, or the SolutionManager
for an explorer — and the relations between them are stated elsewhere.

**They are stated in two recipes.** The first describes the *cost layer*: the
SolutionManager to build and the cost to evaluate solutions with.

<!-- snippet: tutorial/main.cpp:sm-recipe -->
```cpp title="EasyLocal 4"
auto sm = el::solution_manager<TourManager>() | el::component<TourLength>();
```

`solution_manager<TourManager>()` names the class to build, and
`| component<TourLength>()` adds a cost component to it; with more than one
component the right-hand side is a cost expression,
`| cost::hard_soft(...)` or `| cost::sum(...)`, which also carries the weights
(step 6). This is `AddCostComponent`, moved out of `main` and made explicit
about how the components combine.

The names follow one rule, which saves looking them up: a **class** of yours
is passed as a template argument to a function that names what it is —
`solution_manager<M>()`, `component<C>()`, `neighborhood<E>()`,
`delta<C, D>()`, `runner<A>("name")` — and those functions return recipes, not
objects. The ones that *do* produce something to hold and use are spelled
`make_`: `make_runner<Algorithm>(parameters)` below, and `make_solver(runner)`
of [chapter 8](tutorial/08-solvers.md). Arguments a class needs besides the
Input go to the factory, which keeps them until it builds the class:
`component<Excess>(ExcessParameters{.bound = 8.0})`.

The second recipe describes the *delta cost layer*: the neighborhood and, for
each cost component that has one, the delta that evaluates a move
incrementally.

<!-- snippet: tutorial/main.cpp:nhe-recipe -->
```cpp title="EasyLocal 4"
auto nhe =
    el::neighborhood<TwoOptExplorer>() | el::delta<TourLength, TwoOptLengthDelta>();
```

`delta<TourLength, TwoOptLengthDelta>()` is `AddDeltaCostComponent`: the first
type is the component the delta belongs to, which in EasyLocal 3 the delta
learned from its constructor, and the second is the delta cost component
itself. A component with no delta for this neighborhood is simply not named
here, and the framework evaluates it on a copy of the solution with the move
applied.

Both are ordinary values. `sm` and `nhe` can be copied, returned from a
function, stored in a header and shared between several programs — the TSP
example keeps its recipes in `examples/tsp/apps.hpp` for exactly that reason.
At this point nothing has been constructed: a recipe holds the types to build
and the parameters to build them with, no `TourManager` and no `TourLength`
exist yet, and no Input has been read.

**A runner is an algorithm plus the two recipes:**

<!-- snippet: tutorial/main.cpp:first-improvement -->
```cpp title="EasyLocal 4"
auto fi =
    el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
    | sm | nhe;

auto search = fi.bind(tsp);
const auto result = search.run(search.initial_solution());
```

- `make_runner<runners::FirstImprovement>(parameters)` chooses the algorithm
  and holds its parameters, as the constructor of an EasyLocal 3 runner did,
  but without an Input, a SolutionManager or an explorer to pass it.
- `| sm` gives it the first recipe and with it two things: the Solution, which
  `TourManager` builds and the runner will copy and return, and the cost that
  evaluates it. `| nhe` gives it the second, again two: the Move, which
  `TwoOptExplorer` enumerates and applies, and the deltas that evaluate a move
  without evaluating the solution. Together they are the four template
  arguments of an EasyLocal 3 runner, `<Input, Solution, Move, CostStructure>`,
  deduced here from the classes themselves. The result, `fi`, is still a
  description: it is the line `FirstDescent<...> fd(in, sm, nhe, "FD");` with
  everything about *this* instance left out.
- The order is part of the composition: the SolutionManager recipe first, the
  neighborhood after it. `| nhe | sm` does not compile — the compiler says
  that the left operand of the first `|` is not a SolutionManager recipe.
- `fi.bind(tsp)` is where the objects of the EasyLocal 3 `main` are finally
  built: a `TourManager` on `tsp`, a `TourLength` on `tsp`, a
  `TwoOptExplorer` on that `TourManager`, a `TwoOptLengthDelta` on `tsp`, and
  First Improvement from the parameters. They are owned by the *bound runner*
  it returns, so nothing has to be kept alive by hand; `tsp` itself is only
  referenced, so it must outlive the bound runner, and `bind` refuses a
  temporary Input for that reason.
- `search.run(solution)` runs the algorithm from a solution, and returns the
  final solution, its cost, the iterations and evaluations spent and why it
  stopped. `search.initial_solution()` and `search.random_solution(rng)` ask
  the bound SolutionManager for a starting point, as `GreedyState` and
  `RandomState` did.

The same `fi` can be bound again, to another instance or on another thread.
What the two bound runners have in common is the description — the same
classes, composed the same way, with the same parameters — and the `const`
Input they both refer to; every object the description names is built again,
one set per bind, owned by that bound runner alone. Nothing one of them writes
is seen by the other, which is what makes two searches on one instance safe to
run side by side. In EasyLocal 3 the objects *were* the composition, so a
second search with a different cost needed a second SolutionManager, a second
explorer and a second set of deltas (step 13 shows that program, and what
replaces it).

A recipe that leaves out a piece the algorithm needs does not compile, and the
message says what to add rather than pointing inside the library. A
SolutionManager recipe with no cost, `solution_manager<TourManager>()` alone,
stops at

```text
a SolutionManager recipe needs a cost: the cost is always computed by cost
components, add `| component<C>()` or a cost expression such as
`| cost::sum(component<A>(), component<B>())`
```

which is the counterpart of an EasyLocal 3 program that compiled, ran, and
reported a cost of zero because nothing had been added to the SolutionManager.

**An app puts names on runners.** An app is the two recipes plus a named
runner for each algorithm the program offers — two here, but one is just as
valid, and that is the usual shape of a program with a single method:

```cpp title="EasyLocal 4"
auto application = el::app("tsp") | sm | nhe
    | el::runner<runners::FirstImprovement>("fi")
    | el::runner<runners::SimulatedAnnealing<Classic>>("sa");
```

The app is what the command-line program (step 11), the interactive tester
(step 12) and the REST service take: each of them binds it to an Input in a
`Session`, which keeps a current solution and runs a registered runner by its
name — `--runner sa` is `session.run("sa")`. That is the replacement for
EasyLocal 3's `Solver`, `SetRunner` and the branch over `--main::method`
([chapter 11](tutorial/11-apps-and-tools.md)).

**The pipes are a shortcut**, and every composition has a spelling that says
the same thing in words, which is the one to reach for while the vocabulary is
new:

<!-- snippet: tutorial/main.cpp:with-spelling -->
```cpp title="EasyLocal 4"
auto same_runner =
    el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
        .with_solution_manager(
            el::solution_manager<TourManager>().with_cost(
                el::component<TourLength>()))
        .with_neighborhood(
            el::neighborhood<TwoOptExplorer>()
                .with_delta<TourLength, TwoOptLengthDelta>());
```

`.with_solution_manager(...)`, `.with_cost(...)`, `.with_neighborhood(...)`
and `.with_delta<C, D>()` are what `|` is short for: the two build the same
runner, both are part of the API, and a program may use either or mix them.
The pipe is shorter where recipes are written once and composed often; the
named calls say out loud which part is being given.

### 11. The main program

The `main` of an EasyLocal 3 program is long, and all of it goes away. It
declared a `ParameterBox` and a `Parameter<T>` for the instance, the seed, the
method and the output file; parsed the command line twice, before and after
building the objects, so that the runners could add their own parameters;
built the Input, the SolutionManager, the components, the explorer, the deltas
and one runner per method; built a `Tester` and a `MoveTester` in case no
method was given; branched on `--main::method` to `SetRunner`, called
`Solve()`, and printed the cost and the solution, to a file when
`output_file.IsSet()`.

The counterpart is the app of step 10 and one call,
`examples/tutorial/cli_main.cpp` in full:

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
| the search progress printed by hand from the runner | `--trace run.jsonl`, the events of the run ([what EasyLocal 4 adds](#what-easylocal-4-adds)) |
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

EasyLocal 3's tester was a `Tester` on the SolutionManager plus a `MoveTester`
for each explorer, built in `main` next to the other objects, with text menus
on the standard input:

```cpp title="EasyLocal 3"
Tester<Input, Tour, CostStructure> tester(in, sm);
MoveTester<Input, Tour, TwoOpt, CostStructure> two_opt_test(in, sm, nhe, "2-opt", tester);
tester.RunMainMenu();
```

In EasyLocal 4 the tester is the TextUI, an interactive terminal interface in
the optional `TUI` component (`find_package(EasyLocal CONFIG REQUIRED
COMPONENTS Core TUI)`, then link `EasyLocal::TUI`). There is nothing to
register: it takes the app of step 10, which already names the neighborhood
and the runners, and the options say where it starts from.

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

Its three pages — Input/Output, Move, Run — cover what the State, Move and Run
menus did: creating and loading solutions, selecting a move and comparing its
incremental and full evaluation, running a registered runner with its
parameters and watching it. They are described, key by key, in
[chapter 13](tutorial/13-tester.md); here is the Move page showing the best
2-opt move, the counterpart of *Perform Best Move* followed by a cost check:

![The Move page of the TextUI, with the best 2-opt move and its delta check](tutorial/images/tui-moves.svg)

One EasyLocal 3 tester could hold several move testers on the same state,
because the explorers were objects it was given. An EasyLocal 4 tester shows
the moves of its app's neighborhood, so the counterpart is a launcher over
several apps — one per neighborhood, built from the same SolutionManager
recipe — between which the Input and the current solution are carried over:

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

`two_opt_app()` and `swap_app()` are two apps of the TSP example
(`examples/tsp/apps.hpp`), one with the 2-opt neighborhood and one with the
swaps ([chapter 13](tutorial/13-tester.md#several-apps-on-the-same-problem)).

### 13. A solve in two stages

The `main` of a real EasyLocal 3 solver is longer than the one of step 11. A
common shape is a solve in two stages: first a search on the hard constraints
only, until the solution is feasible, then a search on the whole cost from
there. EasyLocal 3 could not evaluate part of a cost, so everything below the
cost was built twice — a second SolutionManager with the hard component alone,
a second explorer on it, a second set of deltas, a second runner, a second
solver — and the two were joined by hand, `solverH.Solve()` and then
`solver.Resolve(stage_one.output)`. Parameters that depend on the instance
were set with `SetParameter` after the parsing, and the report printed each
component with `cc.Cost(output)`.

The same program in EasyLocal 4 is `examples/tutorial/staged_main.cpp`, with
the TSP's edges longer than 8 as the violations. Nothing is written twice:
one recipe for the cost, one for the neighborhood, one runner, and the solver
derives the hard-only stage from them:

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

Much of what EasyLocal 3 found at run time, or not at all, is a compile error
here, and that is the first check a port goes through: a hook with the wrong
signature, a delta whose value cannot be added to its component's, a
SolutionManager recipe with no cost, a runner composed on a neighborhood it
cannot use — each is reported where the pieces are composed, with the member
or the line to write (step 10). A port that compiles is a port whose pieces
fit together.

What compiles can still differ from the EasyLocal 3 program, and two things
differ silently: the delta cost components, easy to port with a wrong sign or
a stale index, and the neighborhood, whose enumeration or sampling may have
changed when `FirstMove`/`NextMove` became a cursor. EasyLocal 3 checked both
from the `MoveTester` menu, by hand, on whatever state happened to be current.
EasyLocal 4 offers the same checks as functions, so they run in the test
suite, on every instance, at every build.

**While you write a delta: `EASYLOCAL_VERIFY_DELTAS`.** Define it when
compiling (`-DEASYLOCAL_VERIFY_DELTAS`, or `target_compile_definitions`) and
every search, anywhere in the program, re-evaluates the solution in full after
each move it applies and compares the result with what the deltas said. The
first disagreement stops the run and names the component:

```text
EASYLOCAL_VERIFY_DELTAS: the delta of cost component #1 disagrees with its full evaluation after a move
```

It makes the search much slower, so it belongs to a debug build or to a test,
never to a measured run ([chapter 4](tutorial/04-delta-evaluation.md)). It is
the fastest way to find a delta that was right in EasyLocal 3 and is wrong
here, usually because the attributes of the move changed meaning: EasyLocal
3's `TwoOpt` named the two *edges* it removed, while the tutorial's names the
two *positions* that bound the reversed segment.

**Component by component, in a unit test.** A *fixture* is an Input with the
solutions to check on — give it one that is not the identity, so that a delta
mixing up positions and cities cannot pass — and each check builds one
component from it, as an app would, and returns a report a test asserts on
([chapter 10](tutorial/10-testing.md)):

| Check | What it verifies |
| --- | --- |
| `check_solution_manager(f)` | the fixture's solutions, the initial one and random ones are valid |
| `check_cost_component<Component>(f)` | evaluating the same solution twice gives equivalent values |
| `check_neighborhood<NHE>(f)` | enumerated and sampled moves are valid, keep the solution valid and change it; random moves are enumerated ones, drawn from the generator given |
| `check_delta_cost_component<NHE, Component, Delta>(f)` | after each valid move, `value + delta` equals the full re-evaluation |

They live in `easylocal::testing`, and `run_checks(...)` runs several and
returns a program's exit status. This is EasyLocal 3's `ComponentTester` and
the cost checks of the `MoveTester`, with the menu replaced by an assertion.

**The whole app at once.** `check(app, input)` builds every service the app
describes, runs the checks above from the initial solution and from random
ones, and reports how many passed:

<!-- snippet: tutorial/main.cpp:check -->
```cpp title="EasyLocal 4"
const auto report = el::check(application, tsp); // also: check(app, input, solution)
el::print_report(std::cout, report);
if (!report)
    return 1;
```

```text
EasyLocal tsp check: 503 checks passed
composition: solution_managers=1, cost_components=1, neighborhoods=1, delta_bindings=1, runner_registrations=2
```

It also covers what EasyLocal 3 had no way to check: that the registered
runner names are distinct and usable on the command line, that each runner can
be built from its parameters, and that every parameter declares a domain — the
last one being what makes a program tunable (see
[the parameters](#parameters-configuration-and-tuning) below). The full list
of checks is in [chapter 14](tutorial/14-checking.md).

**The Move menu's three checks, on a current solution.** When the port is
being compared with the EasyLocal 3 program on one instance, a `Session` holds
an Input and a current solution, like the tester, and runs the same three
checks the `MoveTester` offered under *Check Neighborhood Costs*, *Check Move
Independence* and *Check Random Move Distribution*. They return counters
instead of printing, so a program decides what to do with them:

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

The TextUI runs these three on its Move page and `check(app, input)` on its
Input/Output page (step 12): the same checks, from the keyboard while you look
at the problem, or from a test once you know what to assert.

## Parameters, configuration and tuning

In EasyLocal 3 a parameter was an object: a `Parameter<T>` registered in a
`ParameterBox`, read from the command line as `--box::name` by
`CommandLineParameters::Parse`, asked whether it was set with `IsSet()` and
changed later with `SetParameter("name", value)`. Runners declared their own
that way, and a program's own parameters sat next to them in `main`.

In EasyLocal 4 a parameter belongs to a **parameter block**: a plain struct
whose fields have defaults, with a `parameter_schema()` that describes them.
Every algorithm, temperature policy, tabu list, cost expression and
neighborhood has one, and so may your own classes:

```cpp title="EasyLocal 4"
struct ExcessParameters
{
    double bound{8.0};

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"bound", &ExcessParameters::bound>(
                "Longest edge that is not a violation",
                easylocal::config::range(0.0, easylocal::unlimited)));
    }
};
```

The schema is read at compile time and gives each field three things EasyLocal
3 had no place for: a **description**, shown by `--help` and by the tester; a
**domain**, the values the field may take (`config::range(0.0, 1.0).open()`,
`config::one_of(...)`, or `easylocal::unlimited` for anything); and, when
fields relate, a **condition** (`.only_if(...)`, for a parameter that matters
only when another is set) or a **requirement**
(`config::require(value<"final_temperature"> < value<"initial_temperature">,
"...")`). What EasyLocal 3 checked by hand after parsing, or did not check at
all, is stated once and enforced everywhere
([chapter 9](tutorial/09-configuration.md)).

The blocks are collected into a **parameter set**, where each one has a path.
`runner.configuration()` gives the parameters of a composed runner under
`search` for the algorithm, `cost` for the cost expression and
`neighborhood` for the neighborhood, and a program adds that set under a
prefix of its own (`configuration.add("descent", descent.configuration())`
makes them `--descent.search.*`). An app does it for every runner it
registers, so in a program built on `cli::run` the paths are the switches:
`--runners.<name>.*` for each runner's algorithm, and `--cost.*` and
`--neighborhood.*` for what the app's runners share.

| EasyLocal 3 | EasyLocal 4 |
| --- | --- |
| `Parameter<double> cooling("cooling_rate", "...", box)` | a field of the block, and a line of its `parameter_schema()` |
| `--SA::cooling_rate 0.95` | `--runners.sa.temperature.cooling_rate 0.95`, listed by `--help` with its domain and its current value |
| the weights given to each `CostComponent` constructor | `--cost.weights`, the weights of the `cost::sum`, set without recompiling |
| `parameter.IsSet()` | nothing to ask: every field has a default, and the diagnostics say what was rejected |
| `runner.SetParameter("max_evaluations", n)` | `runner.parameters().max_evaluations = n`, before the runner is bound (step 13) |
| `CommandLineParameters::Parse(argc, argv, ...)` | `cli::run(app, argc, argv)`, or `config::load_and_apply(argc, argv, set)` in a program that is not built on an app |
| — | the parameter window of the interactive tester, which edits the same set |

Values are applied all or none and validated before anything runs: an invalid
one leaves every parameter unchanged, prints which path was rejected and why,
and ends the program with status 2.

Three things follow from the schema, none of which an EasyLocal 3 program
could get for free:

- **Configuration files.** `--config <file>` reads `path = value` lines, with
  `#` comments; the command line wins over the file. With the optional
  `ConfigTOML` component the same paths are a TOML file, where each table is a
  prefix — `[solver.search.temperature]` holds `cooling_rate` — which is the
  readable way to keep the settings of an experiment next to its results
  ([chapter 9](tutorial/09-configuration.md#configuration-files)).
- **Tuning.** A parameter with a finite domain can be tuned, so
  `cli::run --tuning.irace <dir>` writes a complete
  [irace](https://mlopez-ibanez.github.io/irace/) scenario — the parameter
  file with the domains, the conditions as irace conditions, the requirements
  as `[forbidden]` expressions, and the stub that runs the program — from the
  program's own parameters, which cannot drift from the switches it accepts
  ([chapter 12](tutorial/12-tuning.md)).
- **The domain check.** `check(app, input)` fails on a parameter that declares
  no domain, since nothing could tune it and `--help` could not describe it.
  Giving a temperature `config::range(1.0, 100.0).log()` instead of
  `unlimited` is usually all it takes.

## What EasyLocal 4 adds

These have no EasyLocal 3 counterpart to port, but they replace things
EasyLocal 3 programs wrote by hand:

- **Tracing.** `easylocal::trace` records typed events of a run — `run_started`,
  `move_evaluated`, `move_accepted`, `incumbent_updated`, `local_optimum`,
  `temperature_changed`, `run_finished` and the Tabu Search ones — through a
  tracer: `memory_recorder` in a program, JSON Lines or the binary ELTR format
  with `--trace run.jsonl` ([chapter 16](tutorial/16-observing-and-controlling.md)
  and [Tracing](tracing.md)). The tracer is a template parameter of the run, so
  a program that records nothing pays nothing, and `scripts/eltr.py` decodes a
  recorded run afterwards.
- **Control over a run.** `run_control` carries a `std::stop_token` and a
  progress observer, and `with(control)`, `stop_at(cost)` and `timeout(s)` are
  the options of a run: an interactive frontend shows its progress and stops it
  from another thread, and every runner honours both through `search_run`.
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
   is also what makes it tunable
   ([the parameters](#parameters-configuration-and-tuning)).

## Reference: the two frameworks side by side

### The concepts

Each row is one idea, not a mechanical rename: the EasyLocal 4 column says
where that idea lives now, and the step of this page that ports it.

| EasyLocal 3 | EasyLocal 4 |
| --- | --- |
| Input, Solution and Move (the State of `RandomState`, `PrintState`) | the same three values, plain: no base class, no back pointer to the Input (steps 1 to 3) |
| `SolutionManager` (`RandomState`, `GreedyState`, `CheckConsistency`) | SolutionManager: `random_solution`, `initial_solution`, `is_valid` — construction and validity only, no cost (step 5) |
| `CostComponent::ComputeCost` | a cost component's `evaluate`, with no base class; the recipe attaches it, `component<C>()` (step 6) |
| `PrintViolations` | an optional `describe(solution)` of the cost component, which returns the text instead of printing it |
| the hard flag and the weight of a component, `HARD_WEIGHT` | a cost expression in the recipe: `cost::hard_soft`, `cost::sum`, `cost::weighted`, `cost::in_order`, `cost::apply`, `cost::objectives`; hard is a level, not a big weight (step 6) |
| `CostStructure`, `DefaultCostStructure<CFtype>`, the pair violations/objective | nothing to declare: the cost and its type follow from the components and the expression |
| `DeltaCostComponent::ComputeDeltaCost`, `AddDeltaCostComponent` | a delta cost component's `delta_evaluate`, bound to its component by `delta<C, D>()` in the recipe (steps 8 and 10) |
| `NeighborhoodExplorer` (`FirstMove`, `NextMove`, `RandomMove`, `MakeMove`, `FeasibleMove`) | a NeighborhoodExplorer with a cursor (`first_move`/`next_move`) or a `moves()` generator, plus `random_move`, `make_move`, `is_valid` (step 7) |
| `MultimodalNeighborhoodExplorer`, `ActiveMove` | `neighborhood_union`, whose move is a variant of the children's moves, with `random_biases` (step 9) |
| `ParallelNeighborhoodExplorer` (TBB) | no counterpart ([roadmap](roadmap.md#parallel-search)) |
| `Kicker`, `KickerTester` | no counterpart ([roadmap](roadmap.md#kicks-iterated-local-search-and-variable-neighborhood-descent)) |
| `Runner` subclasses, `MoveRunner` | algorithm classes in `easylocal::runners` with one `run` member, composed with the recipes into a runner (step 10) |
| `Solver` (`SimpleLocalSearch`, `MultiStartSearch`, ...), `SetRunner`, `Solve`, `Resolve` | `easylocal::solvers` and their pipelines, or an app whose runners a Session runs by name (steps 10, 11 and 13) |
| `Tester`, `MoveTester` | `tui::run(app, options)`, the TextUI adapter (step 12), and the checks of `check` and of a Session |
| `ComponentTester` | `easylocal::testing`, which checks a component against a fixture in a unit test |
| `ParameterBox`, `Parameter<T>`, `CommandLineParameters`, `SetParameter` | a parameter block per component, with a schema, domains and conditions, gathered in a `parameter_set` ([the parameters](#parameters-configuration-and-tuning)) |
| `Random::Uniform`, `Random::SetSeed`, the global generator | an RNG passed to the members that need one, and `--seed` for the run's |
| `Boost.program_options`, TBB | no dependency beyond the standard library: Core is header-only and parses the command line itself |

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
| `GRASP` (`GreedyState(alpha, k)`) | not planned: a greedy randomized construction is a `random_solution` of its own, searched by `solvers::MultiStart` |
| `ParallelNeighborhoodExplorer` | [Parallel search](roadmap.md#parallel-search) |
| the modelling layer (`AutoState`, expressions) | [A modelling layer](roadmap.md#a-modelling-layer), a brainstorming item |

Nothing on the roadmap is a promise: see [API stability](stability.md).

## Next steps

The [tutorial](tutorial/README.md) builds the same TSP one capability per
chapter, and the [reference](reference/README.md) describes every component in
full.
