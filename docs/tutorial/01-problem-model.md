# 1. Modelling the problem

Start by describing the data the search will use: an Input, a Solution and a
Move. A SolutionManager will create solutions and check their validity.

## Representing tours

Before writing code, decide what a solution looks like and how the search
changes it.

**A solution** is a tour, stored as the sequence of the cities in the order
they are visited. `[0, 1, 2, 3, 4]` is the tour 0 → 1 → 2 → 3 → 4 → 0: the
return to the first city is implicit. Its length is 2 + 6 + 8 + 6 + 7 = 29:

```mermaid
graph LR
    c0((0)) -->|2| c1((1)) -->|6| c2((2)) -->|8| c3((3)) -->|6| c4((4)) -->|7| c0
```

Not every sequence is a tour. A tour of n cities is a *permutation* of 0, 1,
..., n − 1: n positions, every city exactly once. `[0, 1, 1, 3, 4]` visits
city 1 twice and city 2 never; `[0, 1, 2]` forgets two cities.

**A move** changes a tour. We start with `SwapCities{i, j}`, which exchanges
the cities at positions `i` and `j`; chapter 3 adds a second kind of move. On
`[0, 1, 2, 3, 4]`, `SwapCities{1, 3}` gives `[0, 3, 2, 1, 4]`: the edges 0–1
and 3–4 (dashed) leave the tour, the edges 0–3 and 1–4 (red) enter it, and the
length goes from 29 to 34.

```mermaid
flowchart TB
    subgraph before["[0, 1, 2, 3, 4]: length 29"]
        direction LR
        a0((0)) -->|2| a1((1)) -->|6| a2((2)) -->|8| a3((3)) -->|6| a4((4)) -->|7| a0
    end
    subgraph after["after SwapCities{1, 3}: [0, 3, 2, 1, 4], length 34"]
        direction LR
        b0((0)) -->|10| b3((3)) -->|8| b2((2)) -->|6| b1((1)) -->|3| b4((4)) -->|7| b0
    end
    before ~~~ after
    linkStyle 0,3 stroke-dasharray: 4 4
    linkStyle 5,8 stroke: #d33, stroke-width: 3px
```

The edges 1–2 and 2–3 stay, traversed in the other direction. In general a
swap replaces the two edges around each of the two positions, four edges in
all; when the positions are next to each other, only two. A swap always turns a
permutation into a permutation, so it never breaks a tour.

## Input, Solution and Move

In code, the three are plain types:

<!-- snippet: tutorial/tsp.hpp:model -->
```cpp
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

- The **Input** is built once, for example by your parser, and treated as
  immutable while services are bound to it. The distance matrix is a vector of
  rows: the distance between `a` and `b` is `distance[a][b]`.
- A **Solution** does not own or reference the Input.
- A **Move** does not own or reference a Solution: the NeighborhoodExplorer
  applies it to one (chapter 3).

None of them derives from a framework class or needs any operator: plain
structs are enough.

## The SolutionManager

The SolutionManager defines which solutions are valid and how to build them:

<!-- snippet: tutorial/tsp.hpp:solution-manager!random-solution -->
```cpp
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

- `solution_manager_base<Input, Solution>` supplies the type aliases,
  constructor and `input()` accessor. This optional base has no virtual
  functions; a class providing the same members works too.
- `is_valid` checks the representation. Here, `std::ranges::is_permutation`
  verifies that `tour.order` contains exactly the cities 0 through n − 1.
  `std::views::iota` supplies that sequence without storing it. The check can
  take quadratic time; for large tours, marking visited cities in a
  `std::vector<bool>` gives a linear check.
- `initial_solution()` creates a starting tour. It is optional when you
  supply your own solution, but lets runners, solvers and the tester create
  one for you. Chapter 8 adds random construction.

Validity here means that the representation is well formed. A tour may still
violate problem constraints; those violations become part of the cost in
chapter 2.

> **Add capabilities as you need them.** This is enough to start a search.
> Later we add I/O hooks for files (chapter 5), `random_solution` for random
> starts (chapter 8), and equality for neighborhood checks (chapter 14).
> Missing capabilities produce a compile error when required; the interactive
> tester simply omits commands that the problem does not support.

The SolutionManager never computes the cost: the cost always comes from cost
components, the subject of the next chapter.

## See also

- [Problem model](../reference/problem-model.md): optional I/O and display
  hooks for Input, Solution and Move.
- [SolutionManager](../reference/solution-manager.md): the full contract.

## Next steps

[Chapter 2](02-cost.md) gives the tour a cost.
