# 9. Configuration

Runners, temperature policies, cost expressions, neighborhoods and your own
classes expose their parameters as a **parameter set**: each parameter has a
path, such as `search.temperature.cooling_rate`, a description and a value. A
runner's `configuration()` gives the parameters of its algorithm (`search`), of
its cost (`cost`), of its SolutionManager (`solution_manager`, when it has
any) and of its neighborhood (`neighborhood`), with paths relative to the
runner. The program puts them in its own set, under a prefix of its choice, and
applies the command line and configuration files to it:

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

Values are applied in place and validated, so the runner sees them when it is
bound: the program binds and runs `sa` again after `load_and_apply`, and
prints its result as `configured annealing`. They are applied all or none: when one value is invalid, nothing changes.
The set refers to the runner, so the runner must stay where it is while the set
is used.

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

The bound of the hierarchical cost of
[chapter 2](02-cost.md#structured-costs), edges longer than 8, is a literal in
its function. To let the program change it, the function becomes a class with
a parameter block:

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

class Excess
{
public:
    using parameters_type = ExcessParameters; // configurable, built from it

    explicit Excess(ExcessParameters parameters) : bound_{parameters.bound} {}

    // Its parameters are configured under its name: cost.excess.*
    static std::string_view name()
    {
        return "excess";
    }

    double operator()(double longest) const
    {
        return std::max(0.0, longest - bound_);
    }

private:
    double bound_;
};
```

The rule is the same for every class the framework builds: its
`parameters_type` is a parameter block, and it is constructed from it. The
recipe holds the parameters and builds the class from them when a runner or
an app is bound; `cost::apply<Excess>(parameters, children...)` gives them to a
function, `component<C>(parameters, args...)` to a component:

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

The same rule makes a runner configurable, as in chapter 7 (`search`), a
neighborhood explorer (`neighborhood<NHE>(parameters, args...)`, under
`neighborhood`) and a SolutionManager (`solution_manager<SM>(parameters,
args...)`, constructed as `SM(input, parameters, args...)`, under
`solution_manager`). A class that declares its parameters another way, with a
`parameters()` or a `configuration()` and no such `parameters_type`, does not
compile, with a message saying what to write.

The prefix is the program's choice: `"solver"` here gives `--solver.search.*`.
A program that configures a single runner could add its parameters without a
prefix, `configuration.add(sa.configuration())`, for `--search.*`; one that
combines several things gives each its own prefix, so that their parameters do
not collide.

A program's own limits of a run fit a block of the library,
`easylocal::RunParameters`: `target` (a cost as text), `timeout` (seconds) and
`max_evaluations`, added as `configuration.add("run", run_parameters)` for
`--run.target=0`; `run_parameters.options<Cost>(input)` turns them into the
options of a run, as `easylocal::timeout(seconds)`, `max_evaluations(n)` and
`stop_at(cost)` do one by one.

An app (chapter 11) gathers the parameters of all its runners, under
`runners.<name>`, with those of its cost, SolutionManager and neighborhood; the same paths serve
the command line, the TextUI and the REST service, together with a target cost
that stops a run.

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

A schema says more than the names of the fields: the values each one may take,
when it matters, and how fields relate. Simulated Annealing's classic schedule
declares all three:

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

- A **domain**, the second argument of `field`, is the set of valid values:
  `config::range(low, high)`, closed unless `.open()`, `.open_low()` or
  `.open_high()` says otherwise, with `.log()` when its values span orders of
  magnitude, `config::range(0.0, easylocal::unlimited)` for no upper bound
  (infinity included, unless `.open_high()`), `config::one_of("fixed",
  "random")`, or `easylocal::unlimited` for any value
  (a seed, a file name). Every field declares one, a boolean excepted:
  `check(app, ...)` (chapter 14) fails for a parameter without a domain.
- A **condition**, `.only_if(...)`, says when a field matters: the initial
  acceptance is used only to estimate the initial temperature, from
  `calibration_samples` moves. When the condition is false, the field's
  domain is not checked.
- A **requirement**, `config::require(expression, message)`, relates fields;
  its message is the error when it does not hold.

Conditions and requirements are expressions over the fields of the block:
`config::value<"name">` is a field, `value<"temperature.cooling_rate">` a
field of a nested group, combined with comparisons, `&&`, `||`, `!` and
arithmetic. The validation of a set checks all of it, as the errors above
show, and the block's `validate()` checks it with `config::check_schema`,
before what the schema cannot say:

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
