# 9. Configuration

Expose the search parameters to try new settings without recompiling. A
**parameter set** collects them under paths such as
`search.temperature.cooling_rate`, with a description and current value.

A runner's `configuration()` collects settings from its algorithm (`search`),
cost (`cost`), SolutionManager (`solution_manager`, when configurable) and
neighborhood (`neighborhood`). Add this configuration to your program's set
under a prefix, then load values from the command line or a file:

<!-- snippet: tutorial/main.cpp:configuration -->
```cpp
el::config::parameter_set configuration;
configuration.add("solver", sa.configuration());       // --solver.search.*
configuration.add("limited", limited.configuration()); // --limited.cost.*

const auto configured = el::config::load_and_apply(argc, argv, configuration);
if (configured.help_requested)
{
    std::cout << el::config::cli_help(argv[0], configuration);
    return 0;
}
if (!configured)
{
    el::config::print_diagnostics(std::cerr, configured);
    return 2;
}
```

```text
$ ./easylocal_tutorial --help
  --solver.search.temperature.initial_temperature <value>
      Initial annealing temperature
      values: (0, unlimited)
      current: 10
  ...
$ ./easylocal_tutorial --solver.search.temperature.cooling_rate=2
error: solver.search.temperature.cooling_rate: expected a value in (0, 1), got 2
$ ./easylocal_tutorial --solver.search.temperature.final_temperature=20
error: solver.search.temperature: final_temperature must be smaller than initial_temperature
```

`load_and_apply` validates the values and updates the runner in place. The
example then binds `sa` again and prints the new result as `configured
annealing`. Updates are all-or-nothing: one invalid value leaves every
parameter unchanged.

The set refers to the runner's parameters. Keep the runner alive and in the
same location while using the set.

The weights of the cost expression are parameters too, under `cost`. For the
weighted sum of [chapter 2](02-cost.md#cost-expressions) in a runner configured
as `solver`:

```text
$ ./program '--solver.cost.weights=[1, 20]'
```

A `cost::hard_soft` names its branches, so its sums are
`solver.cost.hard.weights` and `solver.cost.soft.weights`; children of
`cost::in_order` and `cost::apply` are named by position (`0`, `1`, ...).

### Parameters of your own classes

In [chapter 2](02-cost.md#structured-costs), we treated edges longer than 8 as
violations. To make that bound configurable, replace the lambda with a class
that takes a parameter block:

<!-- snippet: tutorial/tsp.hpp:cost-parameters -->
```cpp
// The bound of chapter 2 as a parameter: how much the longest edge exceeds it.
struct ExcessParameters
{
    double bound{8.0}; // the longest edge allowed

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"bound", &ExcessParameters::bound>(
                "Longest edge allowed",
                easylocal::config::range(0.0, easylocal::unlimited)));
    }

    easylocal::config::validation_result validate() const
    {
        return easylocal::config::check_schema(*this);
    }
};

class Excess : public easylocal::parameters_base<ExcessParameters>
{
public:
    // The base declares parameters_type, which makes the class configurable,
    // and keeps the block, which parameters() gives back.
    using parameters_base::parameters_base;

    // Its parameters are configured under its name: cost.excess.*
    static std::string_view name()
    {
        return "excess";
    }

    double operator()(double longest) const
    {
        return std::max(0.0, longest - parameters().bound);
    }
};
```

Every configurable class follows the same rule: declare a parameter block as
`parameters_type` and accept it in the constructor.
`parameters_base<ExcessParameters>` supplies both, stores the block and exposes
it through `parameters()`. You can also write these members yourself.

Pass the block to the recipe, which keeps it until binding constructs the
class. Use `cost::apply<Excess>(parameters, children...)` for a function or
`component<C>(parameters, args...)` for a component:

<!-- snippet: tutorial/main.cpp:cost-parameters-use -->
```cpp
// The same hierarchical cost, its bound a parameter: cost.excess.bound.
auto limited =
    el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
    | (el::solution_manager<TourManager>()
        | el::cost::hard_soft(
            el::cost::apply<Excess>({.bound = 8.0}, el::component<MaxEdge>()),
            el::component<TourLength>()))
    | nhe;
```

A function or a component is configured under its `name()`, which must be
static: `cost.excess.bound` here, wherever it is in the expression. Added to the
program's set as `limited`, the bound is `--limited.cost.excess.bound`:

```text
$ ./easylocal_tutorial --limited.cost.excess.bound=5
...
limited 3, 26
```

The same convention applies to runners (chapter 7), explorers and
SolutionManagers. Pass blocks through `neighborhood<NHE>(parameters, args...)`
or `solution_manager<SM>(parameters, args...)`; the latter constructs
`SM(input, parameters, args...)`. Their paths begin with `neighborhood` and
`solution_manager`. A class exposing `parameters()` or `configuration()`
without the required `parameters_type` produces a diagnostic explaining the
missing declaration.

Choose prefixes to keep independent configurations distinct. With one runner,
`configuration.add(sa.configuration())` gives paths such as `--search.*`.
Adding it under `"solver"` gives `--solver.search.*`.

For run limits, the library provides `RunParameters`: `target` as cost text,
`timeout` in seconds and `max_evaluations`. Add it with
`configuration.add("run", run_parameters)` to accept `--run.target=0`.
Then `run_parameters.options<Cost>(input)` produces the run options, just as
`timeout(seconds)`, `max_evaluations(n)` and `stop_at(cost)` do individually.

Apps (chapter 11) collect these settings automatically, using
`runners.<name>` for algorithms. The command line, TextUI and REST service
all use the same paths and validation rules.

Your own parameters take part by describing themselves with a schema:

```cpp
struct AppParameters
{
    std::filesystem::path instance_file;
    std::uint64_t seed{0};

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"instance_file", &AppParameters::instance_file>(
                "TSP instance", easylocal::unlimited),
            easylocal::config::field<"seed", &AppParameters::seed>(
                "RNG seed", easylocal::unlimited));
    }

    easylocal::config::validation_result validate() const;
};

easylocal::config::parameter_set configuration;
configuration.add("application", app_parameters); // --application.instance_file
configuration.add("solver", runner.configuration()); // --solver.search.*
```

A block may nest another one, as a group of the schema:
`config::group<"temperature", &Parameters::temperature>("...")` puts the nested
block's fields under `temperature.`, and validates it together with the
enclosing one.

### Domains, conditions and requirements

A schema also describes allowed values and relationships between fields.
Declare these once to use them for both validation and automatic tuning.
Simulated Annealing's classic schedule shows the three kinds of rule:

```cpp
static consteval auto parameter_schema()
{
    return config::fields(
        config::field<"initial_temperature", &ClassicParameters::initial_temperature>(
            "Initial annealing temperature",
            config::range(0.0, easylocal::unlimited).open_low()),
        config::field<"final_temperature", &ClassicParameters::final_temperature>(
            "Final annealing temperature",
            config::range(0.0, easylocal::unlimited).open_low()),
        config::field<"cooling_rate", &ClassicParameters::cooling_rate>(
            "Multiplicative cooling factor", config::range(0.0, 1.0).open()),
        // ... samples_per_temperature, calibration_samples
        config::field<"initial_acceptance", &ClassicParameters::initial_acceptance>(
            "Acceptance probability of an average worsening move at the "
            "estimated initial temperature",
            config::range(0.0, 1.0).open())
            .only_if(config::value<"calibration_samples"> > 0),
        config::require(
            config::value<"final_temperature"> < config::value<"initial_temperature">,
            "final_temperature must be smaller than initial_temperature"));
}
```

- A **domain**, the second argument to `field`, gives the allowed values.
  Use `config::range(low, high)`, `config::one_of("fixed", "random")` or
  `easylocal::unlimited` for an unrestricted value, such as a filename.
  Except for booleans, every field needs an explicit domain to pass
  `check(app, ...)` (chapter 14).
- A **condition**, `.only_if(...)`, says when a field applies. Initial
  acceptance matters only during temperature calibration; when calibration
  is disabled, that field's domain is not checked.
- A **requirement**, `config::require(expression, message)`, relates fields.
  Validation reports the message if the relationship does not hold.

Ranges include both endpoints by default. Use `.open()`, `.open_low()` or
`.open_high()` to exclude endpoints, and `.log()` for values spanning orders
of magnitude. `config::range(0.0, easylocal::unlimited)` has no upper bound;
it includes infinity unless you add `.open_high()`.

Build conditions and requirements from field references such as
`config::value<"name">` or `value<"temperature.cooling_rate">` for a nested
group. Combine them with comparisons, arithmetic, `&&`, `||` and `!`.

Set validation checks these rules automatically. In the block's own
`validate()`, call `config::check_schema` before any checks that the schema
cannot express:

```cpp
config::validation_result validate() const
{
    if (const auto schema = config::check_schema(*this); !schema)
        return schema;
    // ... the checks the schema does not declare
    return config::validation_result::success();
}
```

Automatic configurators read the same declarations: irace (chapter 12) tunes
the fields with a domain, only when their condition holds, and never proposes
values that break a requirement.

## Configuration files

`load_and_apply` also reads a file given as `--config <file>`, with one
`path = value` per line and `#` for comments, which take whole lines (a `#`
after a value is part of the value):

```text
# annealing.conf
solver.search.temperature.cooling_rate = 0.9
solver.search.temperature.samples_per_temperature = 100
```

Values on the command line win over the file.

## TOML files

For files with sections, types and comments, the optional `ConfigTOML`
component reads TOML. Each table is a prefix and each key a parameter, so the
file mirrors the paths:

<!-- snippet: tutorial/annealing.toml -->
```toml
# Simulated Annealing for the tutorial's TSP (chapter 9).
# Each table is a prefix of the paths, each key a parameter:
# [solver.search.temperature] cooling_rate is solver.search.temperature.cooling_rate.

[solver.search.temperature]
initial_temperature = 20.0
final_temperature = 0.05
cooling_rate = 0.9
samples_per_temperature = 100

# The weights of cost::sum(TourLength, MaxEdge), in order.
[solver.cost]
weights = [1.0, 5.0]
```

The program reads the file with `load_toml_file`, which turns every key into
the same `path = value` override as the command line, and applies them with
the set's `apply`:

<!-- snippet: tutorial/toml_main.cpp:toml -->
```cpp
el::config::parameter_set configuration;
configuration.add("solver", sa.configuration()); // paths solver.*

// Read the file: every key becomes a "path = value" override.
const auto file =
    el::config::load_toml_file(argc > 1 ? argv[1] : EASYLOCAL_TUTORIAL_CONFIG);
if (!file)
{
    // Each error with its line; a value's names its path.
    el::config::print_diagnostics(std::cerr, file);
    return 2;
}

// Apply the overrides: all of them, validated, or none.
const auto applied = configuration.apply(file.overrides);
if (!applied)
{
    for (const auto& diagnostic : applied.diagnostics)
        std::cerr << diagnostic.path << ": " << diagnostic.message << '\n';
    return 2;
}
```

- `load_toml_file` fails on a malformed file, whose diagnostic gives its line
  and column, and on values that have no parameter counterpart, such
  as dates or an array of strings; their diagnostics give the path. An array
  of arrays, `[[1, 2], [3]]`, sets a list of lists.
- Each value is read by its TOML type: `true` and `false` set a `bool`, an
  integer sets a number, a float sets only a floating-point number (`3.0`
  does not set an integer).
- `apply` checks every path and validates every value before
  changing anything: an unknown key (a typo such as `coolin_rate`) or an
  invalid value (`cooling_rate = 2.0`) leaves the parameters as they were.
- The runner here has default parameters and a cost with two weights; with
  the file, Simulated Annealing finds the cost 66, the length 26 plus 5 times
  the longest edge, 8.
- To let the command line override the file, give `load_toml_file` to
  `load_and_apply`, which reads it for `--config`:
  `el::config::load_and_apply(argc, argv, configuration,
  el::config::load_toml_file)`; an app does the same with
  `cli::options::read_config` (chapter 11).

The program is `examples/tutorial/toml_main.cpp`, built when the component is
enabled (`-DEASYLOCAL_ENABLE_CONFIG_TOML=ON`), and linked with
`EasyLocal::ConfigTOML`:

```cmake
find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core ConfigTOML)
target_link_libraries(tsp PRIVATE EasyLocal::Core EasyLocal::ConfigTOML)
```

## See also

- [Configuration](../reference/configuration.md).

## Next steps

[Chapter 10](10-testing.md) checks the components.
