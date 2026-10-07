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
            config::field<"size", &MyParameters::size>(
                "Description", config::range(1, easylocal::unlimited)),
            config::group<"schedule", &MyParameters::schedule>("Description"));
    }

    config::validation_result validate() const; // success() or failure(reason)
};
```

Fields may be integral and floating-point types, `bool`, `easylocal::limit`,
`std::string`, `std::filesystem::path`, and `std::array`s and `std::vector`s
of those (written `[a, b, c]`; `[]` is an empty vector; see
[Values as text](#values-as-text)). A `group` nests another
block: its fields are under `schedule.`, and `config::check_schema` of the
enclosing block runs its `validate()` once the enclosing block's own fields
pass, so that the enclosing block's `validate()` checks it too.

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
| `range(low, high)` | the numbers from `low` to `high`, both included, of one type; for a number or a `limit` |
| `range(low, easylocal::unlimited)` | the numbers from `low` up, with no upper bound, its upper end included: infinity for a floating-point field, `unlimited` for a `limit` (`[1, unlimited]`); `.open_high()` leaves the end out, a finite number or a count (`[1, unlimited)`) |
| `.open()`, `.open_low()`, `.open_high()` | the same range without both bounds, the lower or the upper one |
| `.log()` | the same range, which a configurator samples on a logarithmic scale; `low` must be positive |
| `one_of(a, b, ...)` | the values given: text for a `std::string`, numbers for a number (numbers of different types are held as their common type: `one_of(1, 1.5, 2)` holds 1.0, 1.5 and 2.0) |
| `easylocal::unlimited` | any value of the field's type: a seed, free text, a path, a number that may be negative |

The name of a field or group is a segment of its paths: a letter or `_`,
then letters, digits and `_`. The schema is computed once at compile time, so
a mistake in it does not compile, and the error names `parameter_schema()`.

For an array or a vector the domain applies to each element. The library
handles NaN and infinity in parameters and costs (a domain rejects NaN, a
range with no upper bound includes infinity): `-ffast-math`, which assumes
neither occurs, breaks these checks. A domain that
does not fit the field's type, a range whose bounds are not in order or a
logarithmic range from zero do not compile, and neither do a range of two
types, `range(0.0, 1)`, or a count where a domain goes, `field<...>("...", 5)`:
`easylocal::unlimited` is a tag of its own type, `unlimited_t`, which
converts to the unlimited `limit` and which no number converts to.

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
`<=`, `>`, `>=`, `==`, `!=`, `&&`, `||`, `!`, `+`, `-`, `*` and `/`. Numbers
are computed in `double`, as R computes them, so that an expression means the
same in its irace export: `7 / 2` is 3.5, a `limit` is its count and
`unlimited` is +infinity, and `-count` is negative for an unsigned count.
`config::evaluate(expression, block)` computes one, `config::is_active(field,
block)` tells whether a field's condition holds, and
`describe_expression(expression, prefix)` gives an `expression_info`, its text
with the full paths of its references (`text_with`, `references`): in R
syntax, booleans are `TRUE` and `FALSE`, infinity and NaN `Inf` and `NaN`,
and a text escapes its quotes and backslashes.

### Checking a schema

The validation of a parameter set checks each block against its schema before
its `validate()`: the fields that matter outside their domains, with the
field's path (`expected a value in (0, 1), got 1.5`), and the requirements
that do not hold, with the block's path and their message; then each nested
group, under its own path; the block's `validate()` runs only when they pass. A `validate()` checks the same with
`config::check_schema`, so that a block made in the code is checked too; its
reason, the first that fails, has the same text, with the field's name first.
A `validation_result` owns its message, so `failure()` takes one built at run
time as well as a literal:

```cpp
config::validation_result validate() const
{
    if (const auto schema = config::check_schema(*this); !schema)
        return schema; // "cooling_rate: expected a value in (0, 1), got 2", or a requirement's message
    // ... the checks the schema does not declare
}
```

`config::require_valid(block)` returns the block when it passes these checks,
its groups included, and throws `std::invalid_argument` otherwise, the path of
the field first (`temperature.cooling_rate: expected a value in (0, 1), got
1.5`); `config::require_valid(set)` does the same for a parameter set, with
each invalid block as `<path>: <message>`. The runners, their policies and the
neighborhood recipes call it on the parameters they are made from, and an app
on its whole configuration when it is bound.

Every field declares a domain: `check(app, ...)` fails for a parameter of the
app without one, and the library's tests for a built-in one; a boolean's
domain is true and false. A field that takes any value says so with
`easylocal::unlimited`. `config::undeclared_domains(set)` lists the paths of
the parameters of a set that declare none. Automatic configurators read the
domains: a domain is where the values are valid, not necessarily where they
are worth trying, and a program can narrow it for tuning; a range with no
upper bound, or any value, is not tuned until it is given a finite range.

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
| `apply(text_overrides)` | apply `path = value` overrides (views, or overrides that own their text, as a file gives them), all or none |

A set refers to the objects it was built from: they must outlive it and stay
in place. Adding a path that is already in the set throws
`std::invalid_argument`.

`apply` parses each override into a copy of its block and validates the copy,
and validates every other block as it is; only when every override names a
parameter and every block is valid are the copies committed, so a batch may
repair invalid defaults, a requirement of an enclosing block included.
`load_and_apply` applies the file and the command line in one such batch. Errors: `unknown_parameter`, `duplicate_path`,
`parse_error`, `validation_error` (with the path of the invalid block),
`read_only_parameter`.

## Configurable objects

One rule makes a class configurable: its `parameters_type` is a parameter
block, and it is constructed from it. The framework holds the parameters in
the class's recipe or registration, builds the class from them when a runner
or an app is bound (again after they change), and exposes them at the path of
the class's role:

| Class | Constructed as | Its parameters given by | Path in a runner | Path in an app |
| --- | --- | --- | --- | --- |
| an algorithm | `A(parameters)` | `make_runner<A>(parameters)`, `runner<A>("name", parameters)`, `stage<A>("name", parameters)` | `search.*` | `runners.<name>.*`; `runners.<pipeline>.<stage>.search.*` for a stage |
| a neighborhood explorer | `NHE(sm, parameters, args...)` | `neighborhood<NHE>(parameters, args...)` | `neighborhood.*` | `neighborhood.*`; `runners.<name>.neighborhood.*` for a runner's own |
| a SolutionManager | `SM(input, parameters, args...)` | `solution_manager<SM>(parameters, args...)` | `solution_manager.*` | `solution_manager.*` |
| a cost component | `C(input, parameters, args...)` or `C(parameters, args...)` | `component<C>(parameters, args...)` | `cost.<name>.*` | `cost.<name>.*` |
| a `cost::apply` function | `F(parameters)` | `cost::apply<F>(parameters, children...)` | `cost.<name>.*` | `cost.<name>.*` |

Without a first argument of the `parameters_type`, the parameters are the
defaults. A component or a function is configured under its `name()`, which
must be static, wherever it is in the cost expression. Only `parameters_type`
is read: a class with a `parameters()` that returns a parameter block, or a
`configuration()`, and no such `parameters_type` does not compile, and the
message says what to write. An algorithm registered in an app whose
`parameters_type` is not a block still runs, unconfigured, and
`check(app, ...)` reports it as `runner parameters`.

The structure of the framework's own objects is configurable too: the weights
and tolerance of a cost expression, the biases of a neighborhood union, the
budgets of pipeline stages. An object gives its parameters when
`configuration()` returns a `parameter_set`, with paths relative to it;
whoever composes it chooses the prefix.

The set refers to the object, so configure the object that will run, after
its last copy: `configuration()` does not compile on a temporary
(`(make_runner<A>() | sm | nhe).configuration()`), whose set would refer to
an object already gone.

| Provider | Paths |
| --- | --- |
| a parameterized algorithm (`FirstImprovement`, `BestImprovement`, `HillClimbing`, `GreatDeluge`, `LateAcceptanceHillClimbing`, `ParetoLateAcceptanceHillClimbing`, `SimulatedAnnealing`, the Tabu Search family), through its runner | the fields of its `parameters_type`; `temperature.*` for Simulated Annealing, `tabu_list.*` for Tabu Search |
| the cost expression of a SolutionManager recipe | `weights` of a `cost::sum`; children by position (`0.*`, `1.*`), `hard.*` and `soft.*` of a `cost::hard_soft`; `tolerance.*` of a `cost::approximately`; each component and `cost::apply` function with a `parameters_type` under its `name()` (`<name>.*`) |
| `neighborhood_union` with `random_biases` | `random_biases`, and each child's parameters under its position (`0.*`, `1.*`) |
| `neighborhood<NHE>(parameters, args...)` for an explorer with `parameters_type` | the explorer's parameters |
| `solution_manager<SM>(parameters, args...)` for a SolutionManager with `parameters_type` | the SolutionManager's parameters, as `solution_manager.*` in a runner or an app |
| `runner.configuration()` | `search.*`, `cost.*`, `solution_manager.*`, `neighborhood.*` |
| an app, `app.configuration()` (also a `Session`'s) | `cost.*`, `solution_manager.*`, `neighborhood.*`, `runners.<name>.*` |
| `MultiStart`, `LocalSearch`, `Pipeline` solvers | `starts` and the runner's (MultiStart), the runner's (LocalSearch), each stage's runner and its own `attempts`, `timeout` and `max_evaluations` under its name (Pipeline; `first.*` and `second.*` for `two_stage()`); an algorithm stage of an app's pipeline has its algorithm's `search.*` and its own neighborhood's, the cost and the SolutionManager being the app's |

## Values as text

Every frontend reads a value from the same text, and `config::format_value`
writes it back in that syntax:

| Type | Text |
| --- | --- |
| `bool` | `true` or `false` |
| an integer | a decimal number within the type's range (no sign for an unsigned type) |
| a floating-point number | a decimal or scientific number, `inf`, `-inf` |
| `easylocal::limit` | a count, or `unlimited` |
| `std::string`, `std::filesystem::path` | the text as it is |
| `std::array`, `std::vector` | `[a, b, c]`, the brackets optional; `[]` is an empty vector, and an array needs exactly its size |

Spaces around a value or an element are ignored, except in a text. A list is
split at the commas outside nested brackets, `[[1, 2], [3, 4]]` included, and
there is no quoting: a text element of a list cannot contain a comma or a
bracket, so `format_value` and the parser are each other's inverse except for
such lists. An error names what was expected, the element of a list by its
position (`element 2: expected a number`), and the size of an array
(`expected 2 elements, got 3`).

## Frontends

| Function | Purpose |
| --- | --- |
| `config::parse_cli(argc, argv)` | the overrides `--path=value` and `--path value`, `--config <file>` and `--help` of a command line, with its errors |
| `config::parse_config_text(text)` | the overrides of the text of a configuration file, as `load_config_file` reads them |
| `config::overlay_overrides(base, top)` | one batch of two: the overrides of `top` replace those of `base` with the same path |
| `config::require_valid(set)` | throws `std::invalid_argument` unless every block is valid |
| `config::undeclared_domains(set)` | the paths of the parameters that declare no domain |
| `config::load_and_apply(argc, argv, parameters)` | apply `--config <file>` and `--path.to.field=value` |
| `config::load_config_file(path)` | the overrides of a file of `path = value` lines: `#` starts a whole-line comment (a `#` after a value is part of it), a UTF-8 byte order mark is skipped, a directory is an error |
| `config::cli_help(program, parameters)` | help text: each parameter that can be changed, with its description, its values (the domain; `true` or `false` for a boolean), when it matters (`only if ...`) and its current value |
| `config::print_diagnostics(out, result)` | report errors |
| `config::apply_overrides(parameters, text_overrides)` | the same as `parameters.apply(...)` |
| `config::format_value(value)` | a value as text, in the syntax overrides use |
| `config::load_toml_file(path)` (TOML adapter) | the overrides of a TOML file, each value by its TOML type (a boolean, an integer, a float that sets only a floating-point field, a string, or an array of numbers, booleans and such arrays; not of strings); a parse error gives its `line` and `column`, and its message starts with `file:line:column:` |

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
