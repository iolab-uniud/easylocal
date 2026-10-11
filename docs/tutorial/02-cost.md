# 2. The cost

The search needs a way to compare tours. We start with tour length, then
combine it with other measures without changing the solution or move code.

## Cost components

Costs are minimized by default. To maximize a profit, use `cost::apply` to
return its negation or the lost profit. [Cost semantics](03-structured-costs.md#cost-semantics)
in the next chapter also explains how to define a custom comparison.

A **cost component** computes one term of the objective:

<!-- snippet: tutorial/tsp.hpp:cost-component!component-text -->
```cpp
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

- The only requirement is `evaluate(const Solution&) const -> Value`.
- `Value` is usually a number, as here. It may also be a struct of your own, a
  *domain value*: an advanced use, described in
  [Domain values](03-structured-costs.md#domain-values).
- `input_base<Tsp>` supplies `input()` and the constructor inherited by
  `using input_base::input_base;`. The SolutionManager in
  [chapter 1](01-problem-model.md) uses the same base. You can also provide
  `input()` yourself or omit it if the component needs no Input (see the
  [convenience bases](../reference/problem-model.md#convenience-bases)).
- Binding the runner constructs the component as
  `Component{const Input&, args...}`, or `Component{args...}` for a component
  that needs no Input. Precompute instance data in this constructor so that
  `evaluate` only needs the solution. For example:
  `C(const Tsp& input) : input_base{input}, table_{build(input)} {}`.
- `(k + 1) % n` makes the last city connect back to the first one.

Components are attached to the SolutionManager with a **recipe**
(`el` is the alias of the namespace `easylocal`, see the
[conventions](README.md#conventions-of-the-code)):

<!-- snippet: tutorial/main.cpp:sm-recipe -->
```cpp
auto sm = el::solution_manager<TourManager>() | el::component<TourLength>();
```

This recipe describes a `TourManager` with `TourLength` as its cost. Binding
it to an Input will construct the services. The named spelling is
`solution_manager<TourManager>().with_cost(component<TourLength>())`.

## Cost expressions

With several components, a **cost expression** combines their values. For
example, this weighted sum balances tour length against the longest edge:

<!-- snippet: tutorial/main.cpp:cost-expression -->
```cpp
auto weighted_sm = el::solution_manager<TourManager>()
    | el::cost::sum(el::component<TourLength>(), el::component<MaxEdge>() * 10.0);
```

`MaxEdge` is a second component, the longest edge of the tour:

<!-- snippet: tutorial/tsp.hpp:second-component -->
```cpp
class MaxEdge : public easylocal::input_base<Tsp>
{
public:
    using input_base::input_base;

    double evaluate(const Tour& tour) const
    {
        const auto n = tour.order.size();
        double longest = 0.0;
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto from = tour.order[k];
            const auto to = tour.order[(k + 1) % n];
            longest = std::max(longest, input().distance[from][to]);
        }
        return longest;
    }
};
```

- `child * w` is shorthand for `cost::weighted(child, w)`; either form marks a
  term of a `cost::sum`, and only there. The weights are parameters, here
  `cost.weights` (see [Configuration](10-configuration.md)).
- `cost::sum` adds numbers, and `cost::apply(f, c1, ..., cn)` computes
  anything else from the values of its children:
  `cost::apply([](double longest) { return std::max(0.0, longest - 8.0); },
  component<MaxEdge>())` is the excess of the longest edge over 8.
- Expressions can contain components or other expressions. This keeps the cost
  structure and weights together, so you can change the objective while
  reusing the components.

A single number does not always say which of two solutions is better: a
constraint may have to come before the objective, or two objectives may have
no common unit. [Chapter 3](03-structured-costs.md) builds those costs, and
the rest of the tutorial uses the sum of this chapter.

## See also

- [Cost](../reference/cost.md): cost components, cost expressions, cost models and
  semantics.

## Next steps

[Chapter 3](03-structured-costs.md) compares solutions by priority, by
objective, or by a rule of your own.
