#include <easylocal/config/cli.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>

namespace
{

using easylocal::config::apply_overrides;
using easylocal::config::cli_error;
using easylocal::config::cli_help;
using easylocal::config::parse_cli;
using easylocal::runners::temperature::FixedLengthParameters;

struct AppParameters
{
    std::filesystem::path instance_file{"initial.dat"};
    std::uint64_t seed{17U};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<
                "instance_file",
                &AppParameters::instance_file>(
                    "Problem instance file"),
            easylocal::config::field<"seed", &AppParameters::seed>(
                "Pseudo-random generator seed"));
    }

    [[nodiscard]]
    auto validate() const noexcept -> easylocal::config::validation_result
    {
        return instance_file.empty()
            ? easylocal::config::validation_result::failure(
                  "instance_file must not be empty")
            : easylocal::config::validation_result::success();
    }
};

void equals_and_separate_value_forms_are_supported()
{
    constexpr std::array arguments{
        std::string_view{"--application.instance_file=sample.tsp"},
        std::string_view{"--application.seed"},
        std::string_view{"2026"},
    };

    const auto parsed = parse_cli(
        std::span<const std::string_view>{arguments.data(), arguments.size()});

    assert(parsed);
    assert(!parsed.help_requested);
    assert(parsed.overrides.size() == 2);
    assert(parsed.overrides[0].path == "application.instance_file");
    assert(parsed.overrides[0].value == "sample.tsp");
    assert(parsed.overrides[1].path == "application.seed");
    assert(parsed.overrides[1].value == "2026");
}

void parser_accumulates_syntax_diagnostics()
{
    constexpr std::array arguments{
        std::string_view{"positional"},
        std::string_view{"--"},
        std::string_view{"--application.seed"},
        std::string_view{"--solver.temperature.cooling_rate=0.8"},
    };

    const auto parsed = parse_cli(
        std::span<const std::string_view>{arguments.data(), arguments.size()});

    assert(!parsed);
    assert(parsed.diagnostics.size() == 3);
    assert(parsed.diagnostics[0].error == cli_error::unexpected_argument);
    assert(parsed.diagnostics[1].error == cli_error::malformed_option);
    assert(parsed.diagnostics[2].error == cli_error::missing_value);
    assert(parsed.overrides.size() == 1);
}

void empty_values_and_paths_are_reported()
{
    constexpr std::array arguments{
        std::string_view{"--config"},
        std::string_view{"--=0.8"},
    };

    const auto parsed =
        parse_cli(std::span<const std::string_view>{arguments.data(), arguments.size()});

    assert(!parsed);
    assert(!parsed.config_file.has_value());
    assert(parsed.diagnostics.size() == 2);
    assert(parsed.diagnostics[0].error == cli_error::missing_value);
    assert(parsed.diagnostics[0].message == "missing value for --config");
    assert(parsed.diagnostics[1].error == cli_error::malformed_option);
    assert(parsed.overrides.empty());
}

void help_is_a_frontend_action_not_an_override()
{
    constexpr std::array arguments{
        std::string_view{"--help"},
    };

    const auto parsed = parse_cli(
        std::span<const std::string_view>{arguments.data(), arguments.size()});

    assert(parsed);
    assert(parsed.help_requested);
    assert(parsed.overrides.empty());
}

void config_file_option_is_frontend_metadata()
{
    constexpr std::array arguments{
        std::string_view{"--config=solver.cfg"},
        std::string_view{"--application.seed=42"},
    };

    const auto parsed = parse_cli(
        std::span<const std::string_view>{arguments.data(), arguments.size()});

    assert(parsed);
    assert(parsed.config_file == std::filesystem::path{"solver.cfg"});
    assert(parsed.overrides.size() == 1);
    assert(parsed.overrides[0].path == "application.seed");
}

void duplicate_config_file_is_reported()
{
    constexpr std::array arguments{
        std::string_view{"--config"},
        std::string_view{"first.cfg"},
        std::string_view{"--config=second.cfg"},
    };

    const auto parsed = parse_cli(
        std::span<const std::string_view>{arguments.data(), arguments.size()});

    assert(!parsed);
    assert(parsed.config_file == std::filesystem::path{"first.cfg"});
    assert(parsed.diagnostics.size() == 1);
    assert(parsed.diagnostics[0].error == cli_error::duplicate_config_file);
}

void cli_batch_reuses_transactional_textual_overrides()
{
    AppParameters app{};
    FixedLengthParameters temperature{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    };

    easylocal::config::parameter_set tree;
    tree.add("application", app);
    tree.add("solver.temperature", temperature);

    constexpr std::array arguments{
        std::string_view{"--application.instance_file"},
        std::string_view{"sample.tsp"},
        std::string_view{"--solver.temperature.cooling_rate=0.8"},
        std::string_view{"--solver.temperature.max_iterations"},
        std::string_view{"500"},
    };

    const auto parsed = parse_cli(
        std::span<const std::string_view>{arguments.data(), arguments.size()});
    assert(parsed);

    const auto applied = apply_overrides(
        tree,
        std::span<const easylocal::config::text_override>{parsed.overrides});

    assert(applied);
    assert(app.instance_file == std::filesystem::path{"sample.tsp"});
    assert(temperature.cooling_rate == 0.8);
    assert(temperature.max_iterations == 500);
}

void cli_validation_errors_leave_configuration_unchanged()
{
    AppParameters app{};
    FixedLengthParameters temperature{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    };

    easylocal::config::parameter_set tree;
    tree.add("application", app);
    tree.add("solver.temperature", temperature);

    constexpr std::array arguments{
        std::string_view{"--application.seed=2026"},
        std::string_view{"--solver.temperature.cooling_rate=1.5"},
    };

    const auto parsed = parse_cli(
        std::span<const std::string_view>{arguments.data(), arguments.size()});
    assert(parsed);

    const auto applied = apply_overrides(
        tree,
        std::span<const easylocal::config::text_override>{parsed.overrides});

    assert(!applied);
    assert(app.seed == 17U);
    assert(temperature.cooling_rate == 0.75);
}

void help_is_generated_from_the_configuration_tree()
{
    AppParameters app{};
    FixedLengthParameters temperature{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    };

    easylocal::config::parameter_set tree;
    tree.add("application", app);
    tree.add("solver.temperature", temperature);

    const auto help = cli_help("solver", tree);

    assert(help.find("Usage: solver [options]") != std::string::npos);
    assert(help.find("--config <file>") != std::string::npos);
    assert(help.find("--application.instance_file <value>") !=
           std::string::npos);
    assert(help.find("Problem instance file") != std::string::npos);
    assert(help.find("current: initial.dat") != std::string::npos);
    assert(help.find("--solver.temperature.cooling_rate <value>") !=
           std::string::npos);
    assert(help.find("current: 0.75") != std::string::npos);
}

} // namespace

int main()
{
    equals_and_separate_value_forms_are_supported();
    parser_accumulates_syntax_diagnostics();
    empty_values_and_paths_are_reported();
    help_is_a_frontend_action_not_an_override();
    config_file_option_is_frontend_metadata();
    duplicate_config_file_is_reported();
    cli_batch_reuses_transactional_textual_overrides();
    cli_validation_errors_leave_configuration_unchanged();
    help_is_generated_from_the_configuration_tree();
    return 0;
}
