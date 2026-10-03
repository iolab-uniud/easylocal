# 9. Configuration

Runners, temperature policies, cost expressions and neighborhood unions expose
their parameters as a **parameter set**: each parameter has a path, such as
`search.temperature.cooling_rate`, a description and a value. A runner's
`configuration()` gives the parameters of its algorithm (`search`), of its cost
(`cost`) and of its neighborhood (`neighborhood`), with paths relative to the
runner. The program puts them in its own set, under a prefix of its choice, and
applies the command line and configuration files to it:

<!-- snippet: tutorial/main.cpp:configuration -->
```cpp
el::config::parameter_set configuration;
configuration.add("solver", sa.configuration()); // --solver.search.*

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
      current: 10
  ...
$ ./easylocal_tutorial --solver.search.temperature.cooling_rate=2
error: solver.search.temperature: cooling_rate must be finite and in the open interval (0, 1)
```

Values are applied in place and validated, so the runner sees them when it is
bound. They are applied all or none: when one value is invalid, nothing changes.
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

The prefix is the program's choice: `"solver"` here gives `--solver.search.*`.
A program that configures a single runner could add its parameters without a
prefix, `configuration.add(sa.configuration())`, for `--search.*`; one that
combines several things gives each its own prefix, so that their parameters do
not collide.

An app (chapter 11) gathers the parameters of all its runners, under
`runners.<name>`, with those of its cost and neighborhood; the same paths serve
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
            easylocal::config::field<"instance_file", &AppParameters::instance_file>("TSP instance"),
            easylocal::config::field<"seed", &AppParameters::seed>("RNG seed"));
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

## Configuration files

`load_and_apply` also reads a file given as `--config <file>`, with one
`path = value` per line and `#` for comments:

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
`apply_overrides`:

<!-- snippet: tutorial/toml_main.cpp:toml -->
```cpp
el::config::parameter_set configuration;
configuration.add("solver", sa.configuration()); // --solver.search.*

// Read the file: every key becomes a "path = value" override.
const auto file =
    el::config::load_toml_file(argc > 1 ? argv[1] : EASYLOCAL_TUTORIAL_CONFIG);
if (!file)
{
    for (const auto& diagnostic : file.diagnostics)
        std::cerr << diagnostic.path << ": " << diagnostic.message << '\n';
    return 2;
}

// Apply the overrides: all of them, validated, or none.
const auto applied = el::config::apply_overrides(
    configuration,
    el::config::override_views(file.overrides));
if (!applied)
{
    for (const auto& diagnostic : applied.diagnostics)
        std::cerr << diagnostic.path << ": " << diagnostic.message << '\n';
    return 2;
}
```

- `load_toml_file` fails on a malformed file, and on values that have no
  parameter counterpart, such as dates; its diagnostics give the path.
- `apply_overrides` checks every path and validates every value before
  changing anything: an unknown key (a typo such as `coolin_rate`) or an
  invalid value (`cooling_rate = 2.0`) leaves the parameters as they were.
- The runner here has default parameters and a cost with two weights; with
  the file, Simulated Annealing finds the cost 66, the length 26 plus 5 times
  the longest edge, 8.
- To let the command line override the file, merge the two lists with
  `overlay_overrides(file.overrides, cli.overrides)`, where `cli` is
  `parse_cli(argc, argv)`, and apply the result.

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
