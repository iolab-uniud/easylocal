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

    auto validate() const -> easylocal::config::validation_result;
};

// easylocal::config::root(easylocal::config::named<"application">(app_parameters),
//                         runner.configuration<"solver">())
```

TOML configuration files are supported by the optional
`<easylocal/adapters/toml.hpp>` adapter.

## See also

- [Configuration](../reference/configuration.md).

## Next steps

[Chapter 10](10-testing.md) checks the components.
