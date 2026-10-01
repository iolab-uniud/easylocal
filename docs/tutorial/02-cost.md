# 2. The cost

## Cost components

A **cost component** computes one term of the objective:

<!-- snippet: tutorial/tsp.hpp:cost-component -->
```cpp
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
auto sm = el::solution_manager<TourManager>()
        | el::component<TourLength>();

auto nhe = el::neighborhood<TwoOptExplorer>()
         | el::delta<TourLength, TwoOptLengthDelta>();
```

`solution_manager<TourManager>() | component<TourLength>()` describes a
SolutionManager with one cost component; it is built later, from the Input.
The equivalent explicit spelling is
`solution_manager<TourManager>().with_component<TourLength>()`.

## Aggregation

With a single component, its value is the cost. With several components, an
**aggregator** combines their values, in declaration order, into the cost:

<!-- snippet: tutorial/main.cpp:aggregation -->
```cpp
auto weighted_sm = el::solution_manager<TourManager>()
                 | el::component<TourLength>()
                 | el::component<MaxEdge>()
                 | el::aggregator(el::cost::weighted_sum{1.0, 10.0});
```

`MaxEdge` is a second component, the longest edge of the tour:

<!-- snippet: tutorial/tsp.hpp:second-component -->
```cpp
class MaxEdge
{
public:
    explicit MaxEdge(const Tsp& tsp) : tsp_{tsp} {}

    [[nodiscard]] auto evaluate(const Tour& tour) const -> double
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

You may omit the aggregator only when no choice is involved:

| Components | Implicit aggregator |
| --- | --- |
| one, any value type | identity: its value is the cost |
| several, all arithmetic | unit-weight `cost::weighted_sum`, with a warning; weights configurable as `cost.weights` |
| several, with domain values | none: attach an aggregator |

EasyLocal provides `cost::weighted_sum` and
`cost::weighted_sum_with_hard_penalty`; any function object over the component
values works as an aggregator.

## Structured costs

`<easylocal/cost.hpp>` provides cost types beyond scalars:

- `cost::lexicographic<Ts...>` compares its values in order;
- `cost::hierarchical<Hard, Soft>` gives the hard branch strict priority over
  the soft one.

Both are built directly: `cost::hierarchical{hard, soft}`. The Assignment
example (`examples/assignment/cost.hpp`) uses a lexicographic hard cost inside a
hierarchical cost, produced by a custom aggregator.

## Cost semantics

Algorithms never compare costs with operators: they ask whether a cost is
`better`, `equivalent` or `better_or_equivalent` than another. These default to
`<`, `==` and `<=` of the cost type; an aggregator may redefine them, for
instance to compare floating-point costs with a tolerance.

## See also

- [Cost](../reference/cost.md): cost components, aggregators, cost models and
  semantics.

## Next steps

[Chapter 3](03-neighborhood.md) defines the moves.
