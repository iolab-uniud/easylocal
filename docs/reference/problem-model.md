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

Each kind of hook may be written in three ways, tried in this order:

| Hook | Forms, in order | Used by |
| --- | --- | --- |
| read an Input | `static Input::read(std::istream&)`; `read_input(std::type_identity<Input>, std::istream&)` by ADL; `operator>>` on a default-constructed Input | `read_input`, `load_input`, Session, TextUI |
| read a Solution | `static Solution::read(const Input&, std::istream&)`; `read_solution(const Input&, std::istream&)` by ADL; `operator>>` on `Solution{input}` | `read_solution`, `load_solution`, Session, TextUI |
| write a Solution | `Solution::write(const Input&, std::ostream&) const`; `write_solution(const Input&, const Solution&, std::ostream&)` by ADL; `operator<<` | `write_solution`, `save_solution`, Session, TextUI |
| describe a value | `describe() const` member; `describe(const T&)` by ADL; `operator<<` | `describe`, TextUI |
| read a cost | `read_cost(const Input&, std::string_view)` by ADL; else `cost::from_text` | `read_cost`, Session, TextUI, `RunParameters` (see Cost) |
| compare solutions | the SolutionManager's `equal(const Solution&, const Solution&)`; `operator==` on Solution ([solution identity](solution-manager.md#solution-identity)) | the checks of `easylocal::testing`, `Session::check_move_independence`, TextUI, reactive tabu list |
| compare moves | `operator==` on Move | tests, Session and TextUI |

The TextUI shows a Solution without a describe hook as its write hook writes
it.

## Reading and writing

`<easylocal/app/io.hpp>` reads and writes values through the hooks, in any
program:

| Function | |
| --- | --- |
| `read_input<Input>(std::istream&)`, `load_input<Input>(path)` | an Input |
| `read_solution<Solution>(input, std::istream&)`, `load_solution<Solution>(input, path)` | a Solution of `input` |
| `write_solution(input, solution, std::ostream&)`, `save_solution(input, solution, path)` | writes a Solution |
| `describe(value) -> std::string` | the text of a value, for people |

The concepts `readable_input<Input>`, `readable_solution<Input, Solution>`,
`writable_solution<Input, Solution>` and `describable<T>` tell whether a
hook exists; `has_describe<T>` whether `T` has a `describe` of its own, member
or free, not just `operator<<`. The functions throw `std::runtime_error` when a stream fails (or
what a hook throws); the file functions report errors as
`std::runtime_error` naming the file, for `save_solution` those of the final
flush and close too. `write_solution` and `describe` are function objects, so
the argument-dependent lookup of a problem's own hook never finds them.

## Design choices

- **Plain values.** Entities carry no references to each other, so a Solution
  can be copied, stored and compared freely, and a Move can be applied to any
  compatible Solution. Behaviour lives in services (SolutionManager,
  NeighborhoodExplorer), not in the values.
- **Immutable, borrowed Input.** Services borrow the Input by `const&` for their
  whole lifetime; binding a temporary Input is rejected. Concurrent runs share
  only the Input.
- **Who receives the Input.** Three kinds of code, three rules:
  - values (Input, Solution, Move) hold no Input;
  - services (SolutionManager, NeighborhoodExplorers, cost components, delta
    evaluators) receive it once, in their constructor, when a runner or an
    app is bound, and keep it; their members take only the solution and the
    move;
  - hooks, free functions that are not services (`read_solution`,
    `write_solution`, `read_cost`), receive it as a parameter.

  A service is built once per Input, so its constructor is where data derived
  from the instance is computed, such as the conflicts of each exam or a
  neighbour list: once per bind, owned by the service that uses it, and kept
  out of the Input, which stays the instance as read. Passing the Input to
  every call instead would leave such data nowhere but in the Input itself or
  in a mutable cache.
- **Structural vs. feasible.** Validity (see [SolutionManager](solution-manager.md))
  is about the representation; constraint violations are part of the cost.
