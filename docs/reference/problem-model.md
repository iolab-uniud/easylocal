# Problem model

The problem is described by three value types you define: **Input**,
**Solution** and **Move**. The framework imposes no base class and no member on
them; the hooks below are optional and used only by the tools.

## Contract

| Type | Requirements | Notes |
| --- | --- | --- |
| Input | none | built once, immutable while services are bound to it |
| Solution | copyable, movable | does not reference the Input |
| Move | copyable | does not reference a Solution; default-constructible if the explorer uses a cursor |

## Optional hooks

| Hook | Used by |
| --- | --- |
| `static Input::read(std::istream&) -> Input`, or `operator>>` | Tester, TextUI: load an Input |
| `static Solution::read(const Input&, std::istream&) -> Solution` | Tester, TextUI: load a Solution |
| `Solution::write(const Input&, std::ostream&) const`, or `operator<<` | Tester, TextUI: save a Solution |
| `describe() const -> std::string` on Input, Solution or Move | TextUI: display |
| `operator==` on Move | tests, Tester |

Free functions found by ADL are accepted instead of the member hooks:

```cpp
auto read_input(std::type_identity<Input>, std::istream&) -> Input;
auto read_solution(const Input&, std::istream&) -> Solution;
void write_solution(const Input&, const Solution&, std::ostream&);
```

## Design choices

- **Plain values.** Entities carry no references to each other, so a Solution
  can be copied, stored and compared freely, and a Move can be applied to any
  compatible Solution. Behaviour lives in services (SolutionManager,
  NeighborhoodExplorer), not in the values.
- **Immutable, borrowed Input.** Services borrow the Input by `const&` for their
  whole lifetime; binding a temporary Input is rejected. Concurrent runs share
  only the Input.
- **Structural vs. feasible.** Validity (see [SolutionManager](solution-manager.md))
  is about the representation; constraint violations are part of the cost.
