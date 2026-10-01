# 1. Modelling the problem

A problem is described by three plain value types and a SolutionManager.

## Input, Solution and Move

<!-- snippet: tutorial/tsp.hpp:model -->
```cpp
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
```

- The **Input** is built once (for instance by your parser) and treated as
  immutable while services are bound to it.
- A **Solution** does not own or reference the Input.
- A **Move** does not own or reference a Solution: the NeighborhoodExplorer
  applies it to one (chapter 3).

None of them derives from a framework class.

## The SolutionManager

The SolutionManager owns *solution semantics*: which solutions are valid and how
to build one.

<!-- snippet: tutorial/tsp.hpp:solution-manager -->
```cpp
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

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] auto random_solution(RNG& rng) const -> Tour
    {
        auto tour = initial_solution();
        std::shuffle(tour.order.begin(), tour.order.end(), rng);
        return tour;
    }

    [[nodiscard]] auto is_valid(const Tour& tour) const -> bool
    {
        return tour.order.size() == input_.cities;
    }
};
```

- `easylocal::solution_manager_base<Input, Solution>` provides the `input_type`
  and `solution_type` aliases, the constructor, `input()` and the protected
  `input_` reference. It is optional and non-virtual; a hand-written class with
  the same members works as well.
- `is_valid` is required and checks *structural* validity: the representation is
  well formed. A solution that violates problem constraints is still valid; such
  violations are expressed as cost.
- `initial_solution()` and `random_solution(rng)` are optional. Write them when
  something needs to build solutions: a solver, the Tester, or you through
  `bound.initial_solution()`. The RNG is always passed in, so whoever runs the
  search controls seeding.

The SolutionManager never computes the cost: the cost always comes from cost
components, the subject of the next chapter.

## See also

- [Problem model](../reference/problem-model.md): optional I/O and display
  hooks for Input, Solution and Move.
- [SolutionManager](../reference/solution-manager.md): the full contract.

## Next steps

[Chapter 2](02-cost.md) gives the tour a cost.
