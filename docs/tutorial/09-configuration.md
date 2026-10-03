# 9. Configuration

Runners, temperature policies, cost expressions and neighborhood unions expose their
parameters as a **configuration tree**. Combine it with your own parameters and
apply the command line and configuration files to it:

<!-- snippet: tutorial/main.cpp:configuration -->
```cpp
const auto configuration = el::config::root(sa.configuration<"solver">());

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
bound.

The weights of the cost expression are parameters too, under `cost`. For the
weighted sum of [chapter 2](02-cost.md#cost-expressions) in a runner configured
as `solver`:

```text
$ ./program '--solver.cost.weights=[1, 20]'
```

A `cost::hard_soft` names its branches, so its sums are
`solver.cost.hard.weights` and `solver.cost.soft.weights`; children of
`cost::in_order` and `cost::apply` are named by position (`0`, `1`, ...).

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

// easylocal::config::root(easylocal::config::named<"application">(app_parameters),
//                         runner.configuration<"solver">())
```

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
component reads TOML. Each table is a node of the configuration tree and each
key a parameter, so the file mirrors the tree:

<!-- snippet: tutorial/annealing.toml -->
```toml
# Simulated Annealing for the tutorial's TSP (chapter 9).
# Each table is a node of the configuration tree, each key a parameter:
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
const auto configuration = el::config::root(sa.configuration<"solver">());

// Read the file: every key becomes a "path = value" override.
const auto file =
    el::config::load_toml_file(argc > 1 ? argv[1] : EASYLOCAL_TUTORIAL_CONFIG);
if (!file)
{
    for (const auto& diagnostic : file.diagnostics)
        std::cerr << diagnostic.path << ": " << diagnostic.message << '\n';
    return 2;
}

// Apply the overrides to the tree: all of them, validated, or none.
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
