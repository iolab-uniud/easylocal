# 3. Structured costs

The sum of [chapter 2](02-cost.md) weighs every term against the others: a
longer tour is acceptable when some other term improves enough. Some problems
do not work that way. A constraint must be satisfied before the objective
counts; two objectives have no exchange rate; a component measures several
numbers at once. This chapter builds those costs, from the same components.

## Choosing an expression

Choose an expression that matches how you want to compare solutions:

| Expression | Cost |
| --- | --- |
| `cost::sum(t1, ..., tn)` | `Σ wᵢ · costᵢ` over arithmetic costs; `child * w` gives a term its weight (default 1) |
| `cost::in_order(c1, ..., cn)` | a `cost::lexicographic` cost: compared child by child |
| `cost::hard_soft(hard, soft)` | a `cost::hierarchical` cost: `hard` has strict priority over `soft` |
| `cost::objectives(c1, ..., cn)` | a `cost::pareto` cost: one value per objective, compared by Pareto dominance |
| `cost::apply(f, c1, ..., cn)` | `f(cost₁, ..., costₙ)`, for anything else |

An expression nests: a branch of one is a component or another expression,
so the structure and the weights stay together. A hard/soft model is:

```cpp
auto sm = el::solution_manager<TimetableManager>()
        | el::cost::hard_soft(
              el::cost::sum(el::component<Conflicts>(),
                            el::component<Unavailability>()),
              el::cost::sum(el::component<Compactness>() * 2,
                            el::component<Preferences>() * 5));
```

- Each branch has its own weights, named after its path: here
  `cost.hard.weights` and `cost.soft.weights`
  (see [Configuration](10-configuration.md)).
- `cost::sum` adds numbers. A component with a domain value is turned into one
  with `cost::apply`, for example
  `cost::apply([](const Load& l) { return l.overload; }, component<Capacity>())`;
  [Domain values](#domain-values) below is about those components.
- `solvers::two_stage(first, second)` (see [Solvers](09-solvers.md))
  evaluates only the components of the `hard` branch in its first stage.

## Costs compared by priority

`cost::in_order` and `cost::hard_soft` build the two types of
`<easylocal/cost.hpp>` that compare values by priority:

- `cost::lexicographic<Ts...>` compares its values in order: the second one
  matters only between costs whose first one is equal, and so on;
- `cost::hierarchical<Hard, Soft>` gives the hard branch strict priority over
  the soft one: no improvement of the soft cost can make up for a worse hard
  cost. Each branch is compared with its `operator<=>`, or else with `<`
  and `==` alone; the hierarchical cost then has all the comparisons, `<=`
  included.

For separate objectives, `cost::objectives` builds a third type,
`cost::pareto`, compared by Pareto dominance. This supports searches that
return a front of non-dominated solutions (see [Cost](../reference/cost.md)).

You rarely need to spell the types: `cost::in_order` and `cost::hard_soft`
deduce them from the components. Here are two ways to combine `MaxEdge` and
`TourLength`:

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
  a smaller excess is always better, whatever its length. The bound is a
  literal here; [chapter 10](10-configuration.md#parameters-of-your-own-classes)
  makes it a parameter.

Both recipes work with the runner from chapter 6 and the 2-opt neighborhood
from chapters 4 and 5. Read a lexicographic result by position and a
hierarchical result by branch:

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

On the five-city instance, both find a tour of length 26 with a longest edge
of 8. Here the shortest tour also minimizes the longest edge, so the
constraint does not increase the length. For a nested example, see
`examples/assignment/cost.hpp`: its `cost::hard_soft` has a lexicographic hard
branch computed with `cost::apply`.

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

class LongEdgesComponent : public easylocal::input_base<Tsp>
{
public:
    using input_base::input_base;

    LongEdges evaluate(const Tour& tour) const
    {
        const auto n = tour.order.size();
        LongEdges value;
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto edge = input().distance[tour.order[k]][tour.order[(k + 1) % n]];
            if (edge > 6.0)
            {
                value.excess += edge - 6.0;
                ++value.count;
            }
        }
        return value;
    }
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
  the struct must be ordered: `better`, `equivalent` and
  `better_or_equivalent` default to `<`, `==` and `<=` (see
  [Cost semantics](#cost-semantics) below). The defaulted
  `operator<=>` provides them all, comparing the members in declaration order:
  first `excess`, then `count`. Without it, the runner does not compile.
- **A function turns it into a number** (`excess_sm`). `cost::apply` maps the
  struct to its `excess`, a number that is then the hard cost. Here the
  struct needs no operators: the algorithms compare the numbers `cost::apply`
  returns. `cost::apply` is also how a domain value enters a `cost::sum`,
  which adds numbers only.
- **With a delta cost component** (chapter 5), the delta is added to the value, so
  the struct also needs `operator+(Value, Delta)`.

With First Improvement and the 2-opt moves, `long_edges_sm` stops at an
excess of 3 over two edges. That is a local optimum: no 2-opt move improves
it, although a tour with the same excess on a single edge exists.
`excess_sm` finds the minimum excess, 3, and then the shortest tour with it,
of length 26.

## Cost semantics

Algorithms compare costs through `better`, `equivalent` and
`better_or_equivalent`. These use the cost type's `<`, `==` and `<=` by
default. To customize all three, give the function at the root `cost::apply`
a `compare(a, b)` member returning `std::partial_ordering`; `less` means that
`a` is better.

Simulated Annealing also reads `cost::delta` and treats a negative delta as an
improvement. Negating a profit is therefore the simplest way to maximize it.
For floating-point costs, wrap the expression in `cost::approximately(...)`
to treat differences within a tolerance as equivalent.

## See also

- [Cost](../reference/cost.md): cost components, cost expressions, cost models and
  semantics.

## Next steps

[Chapter 4](04-neighborhood.md) defines the moves.
