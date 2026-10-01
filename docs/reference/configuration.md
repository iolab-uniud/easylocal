# Configuration

`<easylocal/config/*.hpp>` (`easylocal::config`); TOML in
`<easylocal/adapters/toml.hpp>`

Parameters are plain structs that describe themselves; configurable objects
expose them through a **configuration tree**, which frontends (command line,
files, TextUI) read and update.

## Parameter blocks

```cpp
struct MyParameters
{
    std::size_t size{10};

    static consteval auto parameter_schema()
    {
        return config::fields(config::field<"size", &MyParameters::size>("Description"));
    }

    auto validate() const -> config::validation_result; // success() or failure("reason")
};
```

Leaves may be integral and floating-point types, `bool`, `std::string`,
`std::filesystem::path`, and `std::array`s of those.

## Configurable objects

An object is a configuration provider when it exposes `configuration()`; most
use `config::endpoint<"name">(*this)` together with `parameters()` and
`configure(parameters) -> validation_result`.

| Provider | Node |
| --- | --- |
| `FirstImprovement`, `BestImprovement`, `SimulatedAnnealing` | `search` |
| temperature policies | `search.temperature` |
| `cost::weighted_sum`, `weighted_sum_with_hard_penalty`, implicit weighted sum | `cost` |
| `neighborhood_union` with `random_biases` | the union's biases |
| `runner.configuration<"name">()` | a named node over the algorithm and recipes |

## Trees and frontends

| Function | Purpose |
| --- | --- |
| `config::root(nodes...)`, `config::named<"name">(nodes...)` | build a tree |
| `config::load_and_apply(argc, argv, tree)` | apply `--config <file>` and `--path.to.leaf=value` |
| `config::cli_help(program, tree)` | help text with current values |
| `config::print_diagnostics(out, result)` | report errors |
| `config::apply_overrides(tree, text_overrides)` | apply `path = value` overrides |
| `config::for_each_config_parameter(tree, visitor)` | iterate the leaves |

Values are parsed, applied in place and validated per block; a failed block
keeps its previous values.

## Design choices

- **Configuration is independent of construction.** The tree references live
  objects; nothing is rebuilt from a parameter box, and binding a runner later
  sees the applied values.
- **Typed leaves, no reflection macros.** Schemas are `consteval` descriptions
  of member pointers.
- **Validation lives with the parameters**, so every frontend reports the same
  errors.
