# 2. The cost

## Cost components

A **cost component** computes one term of the objective:

<!-- snippet: tutorial/tsp.hpp:cost-component!component-text -->
```cpp
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

- The only requirement is `evaluate(const Solution&) const -> Value`.
- `Value` is usually a number, as here. It may also be a struct of your own, a
  *domain value*: an advanced use, described in [Domain values](#domain-values)
  below.
- The component keeps a reference to the Input, `input_`, received in its
  constructor: the framework constructs the component when the runner is
  bound, as `Component{const Input&, args...}` (preferred) or
  `Component{args...}` (accepted for stateless components). The constructor
  runs once per Input, so it is also the place to precompute data derived
  from the instance; `evaluate` then takes only the solution. Every service
  receives the Input in the same way (see the
  [problem model](../reference/problem-model.md#design-choices)).
- `(k + 1) % n` makes the last city connect back to the first one.

Components are attached to the SolutionManager with a **recipe**
(`el` is the alias of the namespace `easylocal`, see the
[conventions](README.md#conventions-of-the-code)):

<!-- snippet: tutorial/main.cpp:sm-recipe -->
```cpp
auto sm = el::solution_manager<TourManager>() | el::component<TourLength>();
```

`solution_manager<TourManager>() | component<TourLength>()` describes a
SolutionManager whose cost is one cost component; it is built later, from the
Input. The equivalent explicit spelling is
`solution_manager<TourManager>().with_cost(component<TourLength>())`.

## Cost expressions

With several components, a **cost expression** says how their values make the
cost. A weighted sum:

<!-- snippet: tutorial/main.cpp:cost-expression -->
```cpp
auto weighted_sm = el::solution_manager<TourManager>()
    | el::cost::sum(el::component<TourLength>(), el::component<MaxEdge>() * 10.0);
```

`MaxEdge` is a second component, the longest edge of the tour:

<!-- snippet: tutorial/tsp.hpp:second-component -->
```cpp
class MaxEdge
{
public:
    explicit MaxEdge(const Tsp& input) : input_{input} {}

    double evaluate(const Tour& tour) const
    {
        const auto n = tour.order.size();
        double longest = 0.0;
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto from = tour.order[k];
            const auto to = tour.order[(k + 1) % n];
            longest = std::max(longest, input_.distance[from][to]);
        }
        return longest;
    }

private:
    const Tsp& input_;
};
```

The expression has one node per way of combining costs:

| Expression | Cost |
| --- | --- |
| `cost::sum(t1, ..., tn)` | `Σ wᵢ · costᵢ` over arithmetic costs; `child * w` gives a term its weight (default 1) |
| `cost::in_order(c1, ..., cn)` | a `cost::lexicographic` cost: compared child by child |
| `cost::hard_soft(hard, soft)` | a `cost::hierarchical` cost: `hard` has strict priority over `soft` |
| `cost::apply(f, c1, ..., cn)` | `f(cost₁, ..., costₙ)`, for anything else |

The children of a node are components or other expressions, so the structure
of the cost is written where the components are listed. A classic
hard/soft model:

```cpp
auto sm = el::solution_manager<TimetableManager>()
        | el::cost::hard_soft(
              el::cost::sum(el::component<Conflicts>(),
                            el::component<Unavailability>()),
              el::cost::sum(el::component<Compactness>() * 2,
                            el::component<Preferences>() * 5));
```

- `child * w` is shorthand for `cost::weighted(child, w)`; either form marks a
  term of a `cost::sum`, and only there.
- The weights are parameters: here `cost.hard.weights` and
  `cost.soft.weights` (see [Configuration](09-configuration.md)).
- `cost::sum` adds numbers. A component with a domain value is turned into one
  with `cost::apply`, for example
  `cost::apply([](const Load& l) { return l.overload; }, component<Capacity>())`.
- `solvers::two_stage()` (see [Solvers](08-solvers.md)) evaluates only the
  components of the `hard` branch in its first stage.

## Structured costs

Not every objective is one number. `<easylocal/cost.hpp>` provides two cost
types made of several values:

- `cost::lexicographic<Ts...>` compares its values in order: the second one
  matters only between costs whose first one is equal, and so on;
- `cost::hierarchical<Hard, Soft>` gives the hard branch strict priority over
  the soft one: no improvement of the soft cost can make up for a worse hard
  cost.

You rarely write these types: the cost expressions `cost::in_order` and
`cost::hard_soft` build them from the component values. Two examples on the
TSP, with the components `MaxEdge` and `TourLength`:

<!-- snippet: tutorial/main.cpp:structured-costs -->
```cpp
// Lexicographic: the longest edge first; between tours with the same
// longest edge, the shorter one.
auto bottleneck_sm = el::solution_manager<TourManager>()
    | el::cost::in_order(el::component<MaxEdge>(), el::component<TourLength>());

// Hierarchical: edges longer than 8 are a violation, measured by how much
// the longest edge exceeds 8 (hard); the tour length is the objective (soft).
auto bounded_sm = el::solution_manager<TourManager>()
    | el::cost::hard_soft(
        el::cost::apply(
            [](double longest) { return std::max(0.0, longest - 8.0); },
            el::component<MaxEdge>()),
        el::component<TourLength>());
```

- `bottleneck_sm` has a `cost::lexicographic<double, double>` cost: a tour is
  better if its longest edge is shorter, and between two tours with the same
  longest edge, the shorter tour is better. The order of the children is the
  order of comparison.
- `bounded_sm` treats edges longer than 8 as a constraint. Its hard cost
  measures the violation: `cost::apply` turns the value of `MaxEdge` into the
  excess over 8, zero when every edge is short enough. Its soft cost is the
  length. The result is a `cost::hierarchical<double, double>`: a tour with
  a smaller excess is always better, whatever its length.

They are run like any other SolutionManager, with a runner (chapter 5) and
the 2-opt neighborhood (chapters 3 and 4). The cost of the result is then read
by position for a lexicographic cost, by branch for a hierarchical one:

<!-- snippet: tutorial/main.cpp:structured-costs-read -->
```cpp
auto bottleneck =
    (el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
        | bottleneck_sm | nhe)
        .bind(tsp);
const auto by_edge = bottleneck.run(bottleneck.initial_solution());
// A lexicographic cost is read by position, a hierarchical one by branch.
const double longest_edge = by_edge.cost.get<0>();
const double length_after_edge = by_edge.cost.get<1>();

auto bounded =
    (el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
        | bounded_sm | nhe)
        .bind(tsp);
const auto within_bound = bounded.run(bounded.initial_solution());
const double excess = within_bound.cost.hard();
const double length_within_bound = within_bound.cost.soft();
```

On the five cities both give a longest edge of 8 and a length of 26: here the
shortest tour also has the shortest possible longest edge, so the constraint
costs nothing. The Assignment example (`examples/assignment/cost.hpp`) nests
the two: a lexicographic hard cost, computed from one component by
`cost::apply`, inside a `cost::hard_soft`.

## Domain values

Sometimes a component measures more than one number at once. `LongEdges`
counts the edges longer than 6 and adds up how much they exceed 6, in a single
pass over the tour:

<!-- snippet: tutorial/tsp.hpp:domain-value -->
```cpp
// A domain value: the edges longer than 6, as a total excess and a count.
struct LongEdges
{
    double excess{};     // the total length beyond 6 of the long edges
    std::size_t count{}; // how many edges are longer than 6

    // Needed only when LongEdges is itself the cost: tours are then compared
    // by excess first and by count between equal excesses.
    auto operator<=>(const LongEdges&) const = default;
};

class LongEdgesComponent
{
public:
    explicit LongEdgesComponent(const Tsp& input) : input_{input} {}

    LongEdges evaluate(const Tour& tour) const
    {
        const auto n = tour.order.size();
        LongEdges value;
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto edge = input_.distance[tour.order[k]][tour.order[(k + 1) % n]];
            if (edge > 6.0)
            {
                value.excess += edge - 6.0;
                ++value.count;
            }
        }
        return value;
    }

private:
    const Tsp& input_;
};
```

The component returns the struct, and the framework stores it like any other
value. What the struct needs depends on how the cost expression uses it:

<!-- snippet: tutorial/main.cpp:domain-value-recipe -->
```cpp
// The struct is the cost: it is compared with its operator<=>.
auto long_edges_sm =
    el::solution_manager<TourManager>() | el::component<LongEdgesComponent>();

// A function turns it into a number: here the excess is a hard cost.
auto excess_sm = el::solution_manager<TourManager>()
    | el::cost::hard_soft(
        el::cost::apply(
            [](const LongEdges& value) { return value.excess; },
            el::component<LongEdgesComponent>()),
        el::component<TourLength>());
```

- **The struct is the cost** (`long_edges_sm`). Algorithms compare costs, so
  the struct must be ordered: `better` and `equivalent` default to `<` and
  `==` (see [Cost semantics](#cost-semantics) below). The defaulted
  `operator<=>` provides both, comparing the members in declaration order:
  first `excess`, then `count`. Without it, the runner does not compile.
- **A function turns it into a number** (`excess_sm`). `cost::apply` maps the
  struct to its `excess`, a number that is then the hard cost. Here the
  struct needs no operators: the algorithms compare the numbers `cost::apply`
  returns. `cost::apply` is also how a domain value enters a `cost::sum`,
  which adds numbers only.
- **With a delta cost component** (chapter 4), the delta is added to the value, so
  the struct also needs `operator+(Value, Delta)`.

With First Improvement and the 2-opt moves, `long_edges_sm` stops at an
excess of 3 over two edges. That is a local optimum: no 2-opt move improves
it, although a tour with the same excess on a single edge exists.
`excess_sm` finds the minimum excess, 3, and then the shortest tour with it,
of length 26.

## Cost semantics

Algorithms never compare costs with operators: they ask whether a cost is
`better`, `equivalent` or `better_or_equivalent` than another. These default to
`<`, `==` and `<=` of the cost type; the function of a `cost::apply` at the
root of the expression may redefine them, for example to compare
floating-point costs with a tolerance.

## See also

- [Cost](../reference/cost.md): cost components, cost expressions, cost models and
  semantics.

## Next steps

[Chapter 3](03-neighborhood.md) defines the moves.
