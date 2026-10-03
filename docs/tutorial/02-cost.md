# 2. The cost

## Cost components

A **cost component** computes one term of the objective:

<!-- snippet: tutorial/tsp.hpp:cost-component -->
```cpp
class TourLength
{
public:
    explicit TourLength(const Tsp& tsp) : tsp_{tsp} {}

    double evaluate(const Tour& tour) const
    {
        double length = 0.0;
        for (std::size_t k = 0; k < tour.order.size(); ++k)
            length += tsp_.d(tour.order[k], tour.order[(k + 1) % tour.order.size()]);
        return length;
    }

private:
    const Tsp& tsp_;
};
```

- The only requirement is `evaluate(const Solution&) const -> Value`.
- `Value` may be an arithmetic type, as here, or a domain type (a struct with a
  total and a count, for instance).
- The framework constructs the component when the runner is bound:
  `Component{const Input&, args...}` is preferred, `Component{args...}` is
  accepted for stateless components.

Components are attached to the SolutionManager with a **recipe**:

<!-- snippet: tutorial/main.cpp:recipes -->
```cpp
auto sm = el::solution_manager<TourManager>() | el::component<TourLength>();

auto nhe =
    el::neighborhood<TwoOptExplorer>() | el::delta<TourLength, TwoOptLengthDelta>();
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
    explicit MaxEdge(const Tsp& tsp) : tsp_{tsp} {}

    double evaluate(const Tour& tour) const
    {
        double longest = 0.0;
        for (std::size_t k = 0; k < tour.order.size(); ++k)
        {
            longest = std::max(
                longest,
                tsp_.d(tour.order[k], tour.order[(k + 1) % tour.order.size()]));
        }
        return longest;
    }

private:
    const Tsp& tsp_;
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
  with `cost::apply`, for instance
  `cost::apply([](const Load& l) { return l.overload; }, component<Capacity>())`.
- `TwoStage` (see [Solvers](08-solvers.md)) evaluates only the components of
  the `hard` branch in its first stage.

## Structured costs

`<easylocal/cost.hpp>` provides cost types beyond scalars:

- `cost::lexicographic<Ts...>` compares its values in order;
- `cost::hierarchical<Hard, Soft>` gives the hard branch strict priority over
  the soft one.

`cost::in_order` and `cost::hard_soft` build them; a `cost::apply` function may
also build them directly, `cost::hierarchical{hard, soft}`. The Assignment
example (`examples/assignment/main.cpp`) uses a lexicographic hard cost,
computed from one component by `cost::apply`, inside a `cost::hard_soft`.

## Cost semantics

Algorithms never compare costs with operators: they ask whether a cost is
`better`, `equivalent` or `better_or_equivalent` than another. These default to
`<`, `==` and `<=` of the cost type; the function of a `cost::apply` at the
root of the expression may redefine them, for instance to compare
floating-point costs with a tolerance.

## See also

- [Cost](../reference/cost.md): cost components, cost expressions, cost models and
  semantics.

## Next steps

[Chapter 3](03-neighborhood.md) defines the moves.
