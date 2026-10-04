# Configuration

`<easylocal/config/*.hpp>` (`easylocal::config`); TOML in
`<easylocal/adapters/toml.hpp>`

Parameters are plain structs that describe themselves; configurable objects
expose them as a **parameter set**: paths, descriptions and textual values,
which frontends (command line, files, TOML, the TextUI) read and change.

## Parameter blocks

```cpp
struct MyParameters
{
    std::size_t size{10};
    Schedule schedule{};          // itself a parameter block

    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"size", &MyParameters::size>("Description"),
            config::group<"schedule", &MyParameters::schedule>("Description"));
    }

    config::validation_result validate() const; // success() or failure("reason")
};
```

Fields may be integral and floating-point types, `bool`, `std::string`,
`std::filesystem::path`, and `std::array`s and `std::vector`s of those
(written `[a, b, c]`; `[]` is an empty vector). A `group` nests another
block: its fields are under `schedule.`, and its `validate()` runs with the
enclosing block's.

### Domains

A field may declare the values it takes as the second argument of `field`:

```cpp
config::field<"cooling_rate", &P::cooling_rate>(
    "Multiplicative cooling factor", config::range(0.0, 1.0).open()),
config::field<"samples", &P::samples>("Proposals", config::range(1, 1000).log()),
config::field<"policy", &P::policy>("Tabu list", config::one_of("fixed", "random")),
```

| Domain | Values |
| --- | --- |
| `range(low, high)` | the numbers from `low` to `high`, both included; for a number or a `limit` (never `unlimited`) |
| `.open()`, `.open_low()`, `.open_high()` | the same range without both bounds, the lower or the upper one |
| `.log()` | the same range, which a configurator samples on a logarithmic scale; `low` must be positive |
| `one_of(a, b, ...)` | the values given: text for a `std::string`, numbers for a number |

For an array or a vector the domain applies to each element. A domain that
does not fit the field's type, a range whose bounds are not in order or a
logarithmic range from zero do not compile.

### Conditions and requirements

```cpp
config::field<"initial_acceptance", &P::initial_acceptance>("...", config::range(0.0, 1.0).open())
    .only_if(config::value<"calibration_samples"> > 0),
config::require(
    config::value<"final_temperature"> < config::value<"initial_temperature">,
    "final_temperature must be smaller than initial_temperature"),
```

| Declaration | Meaning |
| --- | --- |
| `field(...).only_if(expression)` | the field matters only when the expression holds: otherwise its domain is not checked and a configurator does not tune it |
| `require(expression, message)` | an element of `fields(...)`: the expression must hold, else the block is invalid with `message` (which must outlive it) |
| `value<"name">`, `value<"group.name">` | a field of the block, or of a nested group, by its path relative to the block; a path that names no field does not compile |

Expressions combine values and constants (numbers, `bool`, text) with `<`,
`<=`, `>`, `>=`, `==`, `!=`, `&&`, `||`, `!`, `+`, `-`, `*` and `/`; integers
compare by value whatever their signs, and a `limit` as its count.
`config::evaluate(expression, block)` computes one, `config::is_active(field,
block)` tells whether a field's condition holds, and
`describe_expression(expression, prefix)` gives an `expression_info`, its text
with the full paths of its references (`text_with`, `references`).

### Checking a schema

The validation of a parameter set checks each block against its schema before
its `validate()`: the fields that matter outside their domains, with the
field's path (`expected a value in (0, 1), got 1.5`), and the requirements
that do not hold, with the block's path and their message; the block's
`validate()` runs only when they pass. A `validate()` checks the same with
`config::check_schema`, so that a block made in the code is checked too:

```cpp
config::validation_result validate() const
{
    if (const auto schema = config::check_schema(*this); !schema)
        return schema; // "cooling_rate is out of its range", or a requirement's message
    // ... the checks the schema does not declare
}
```

A field without a domain takes any value of its type. Automatic configurators
read the domains: a domain is where the values are valid, not necessarily where
they are worth trying, and a program can narrow it for tuning.

## Parameter sets

```cpp
config::parameter_set parameters;
parameters.add("application", app_parameters);    // a block
parameters.add("solver", runner.configuration());  // another set, under a prefix
```

| Member | Purpose |
| --- | --- |
| `add([prefix,] block)` | the fields of a parameter block; read-only when the block is `const` |
| `add([prefix,] object)` | an object with `parameters()` and `configure(block) -> validation_result`, for objects that rebuild something from their parameters |
| `add([prefix,] set)` | the parameters of another set |
| `parameters()` | every parameter: `path`, `description`, `value` (as text), `read_only`, `kind` (`boolean`, `integer`, `real`, `limit`, `text`, `path`, `list`), `domain` (a `domain_info`, empty when none is declared), `active` (whether its condition holds) and `condition` (an `expression_info` with full paths, empty when it has none) |
| `requirements()` | the requirements of every block: `path` of the block, `message`, `expression` (full paths) and `satisfied` |
| `validate()` | the fields outside their domains, the requirements that do not hold and the diagnostics of every block's `validate()`, by path |
| `apply(text_overrides)` | apply `path = value` overrides, all or none |

A set refers to the objects it was built from: they must outlive it and stay
in place. Adding a path that is already in the set throws
`std::invalid_argument`.

`apply` parses each override into a copy of its block and validates the copy;
only when every override names a parameter and every touched block is valid
are the copies committed. Errors: `unknown_parameter`, `duplicate_path`,
`parse_error`, `validation_error` (with the path of the invalid block),
`read_only_parameter`.

## Configurable objects

An object provides parameters when `configuration()` returns a
`parameter_set`, with paths relative to it; whoever composes it chooses the
prefix.

| Provider | Paths |
| --- | --- |
| a parameterized algorithm (`FirstImprovement`, `BestImprovement`, `HillClimbing`, `GreatDeluge`, `LateAcceptanceHillClimbing`, `SimulatedAnnealing`), through its runner | the fields of its `parameters_type`; `temperature.*` for Simulated Annealing |
| the cost expression of a SolutionManager recipe | `weights` of a `cost::sum`; children by position (`0.*`, `1.*`), `hard.*` and `soft.*` of a `cost::hard_soft`; a `cost::apply` function's own |
| `neighborhood_union` with `random_biases` | `random_biases`, and each child's parameters under its position (`0.*`, `1.*`) |
| `neighborhood<NHE>(parameters, args...)` for an explorer with `parameters_type` | the explorer's parameters |
| `runner.configuration()` | `search.*`, `cost.*`, `neighborhood.*` |
| an app, `app.configuration()` (also a `Session`'s) | `cost.*`, `neighborhood.*`, `runners.<name>.*` |
| `MultiStart`, `LocalSearch`, `Pipeline` solvers | `starts` and the runner's (MultiStart), the runner's (LocalSearch), each stage's runner and `attempts` under its name (Pipeline; `first.*` and `second.*` for `two_stage()`) |

## Frontends

| Function | Purpose |
| --- | --- |
| `config::load_and_apply(argc, argv, parameters)` | apply `--config <file>` and `--path.to.field=value` |
| `config::cli_help(program, parameters)` | help text with current values |
| `config::print_diagnostics(out, result)` | report errors |
| `config::apply_overrides(parameters, text_overrides)` | the same as `parameters.apply(...)` |
| `config::format_value(value)` | a value as text, in the syntax overrides use |
| `config::load_toml_file(path)` (TOML adapter) | the overrides of a TOML file |

## Design choices

- **Configuration is independent of construction.** A set refers to live
  objects; nothing is rebuilt from a parameter box, and binding a runner later
  sees the applied values.
- **Typed fields, no reflection macros.** Schemas are `consteval` descriptions
  of member pointers, checked at compile time; only the paths are matched at
  run time, as the command line and files already require.
- **Relative paths.** Components do not choose their own top-level names, so
  one program can combine several of them under prefixes of its choice.
- **Validation lives with the parameters**, so every frontend reports the same
  errors.
