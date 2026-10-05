#pragma once

/// \file
/// Command-line frontend of the parameters: parse_cli reads overrides
/// (--path value or --path=value), an optional --config file and --help from
/// argv; cli_help lists the parameters of a set with their descriptions and
/// values.

#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameter_set.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace easylocal::config
{

/// An error of the command line.
enum class cli_error
{
    /// An argument that is not a long option (`--path`).
    unexpected_argument,
    /// An option with an empty path (`--` or `--=value`).
    malformed_option,
    /// An option, or `--config`, without a value.
    missing_value,
    /// `--config` given more than once.
    duplicate_config_file,
};

/// An error of the command line, with the argument that caused it.
struct cli_diagnostic
{
    /// The kind of error.
    cli_error error;
    /// The argument, as written.
    std::string argument;
    /// A description of the error.
    std::string message;
};

/// What parse_cli read: the help request, the configuration file, the overrides
/// and the errors.
///
/// It converts to true when there are no errors.
struct cli_parse_result
{
    /// Whether `-h` or `--help` was given.
    bool help_requested{};
    /// The file given with `--config`, if any.
    std::optional<std::filesystem::path> config_file;
    /// The overrides `--path value` and `--path=value`, in order.
    std::vector<text_override> overrides;
    /// The errors, in the order of the arguments.
    std::vector<cli_diagnostic> diagnostics;

    /// Whether there are no diagnostics.
    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return diagnostics.empty();
    }
};

/// Reads the help request, the configuration file and the overrides of the
/// arguments of a program, without its name.
///
/// It recognizes `-h` and `--help`, `--config <file>` (or `--config=<file>`),
/// and the overrides `--path value` and `--path=value`, which refer to the text
/// of the arguments. A malformed argument gives a diagnostic, and the reading
/// goes on.
[[nodiscard]]
inline cli_parse_result parse_cli(const std::span<const std::string_view> arguments)
{
    cli_parse_result result{};

    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const auto argument = arguments[index];

        if (argument == "-h" || argument == "--help")
        {
            result.help_requested = true;
            continue;
        }

        if (argument == "--config" || argument.starts_with("--config="))
        {
            std::string_view value;
            if (argument.starts_with("--config="))
            {
                value = argument.substr(std::string_view{"--config="}.size());
            }
            else if (index + 1 < arguments.size() &&
                     !arguments[index + 1].starts_with("--") &&
                     arguments[index + 1] != "-h")
            {
                value = arguments[++index];
            }

            if (value.empty())
            {
                result.diagnostics.push_back({
                    .error = cli_error::missing_value,
                    .argument = std::string{argument},
                    .message = "missing value for --config",
                });
                continue;
            }

            if (result.config_file.has_value())
            {
                result.diagnostics.push_back({
                    .error = cli_error::duplicate_config_file,
                    .argument = std::string{argument},
                    .message = "--config may be specified at most once",
                });
                continue;
            }

            result.config_file = std::filesystem::path{std::string{value}};
            continue;
        }

        if (!argument.starts_with("--"))
        {
            result.diagnostics.push_back({
                .error = cli_error::unexpected_argument,
                .argument = std::string{argument},
                .message = "expected a long option of the form --path value or --path=value",
            });
            continue;
        }

        auto option = argument.substr(2);
        if (option.empty())
        {
            result.diagnostics.push_back({
                .error = cli_error::malformed_option,
                .argument = std::string{argument},
                .message = "configuration option path must not be empty",
            });
            continue;
        }

        const auto equals = option.find('=');
        if (equals != std::string_view::npos)
        {
            const auto path = option.substr(0, equals);
            if (path.empty())
            {
                result.diagnostics.push_back({
                    .error = cli_error::malformed_option,
                    .argument = std::string{argument},
                    .message = "configuration option path must not be empty",
                });
                continue;
            }

            result.overrides.push_back({
                .path = path,
                .value = option.substr(equals + 1),
            });
            continue;
        }

        const auto path = option;
        if (index + 1 == arguments.size() ||
            arguments[index + 1].starts_with("--") ||
            arguments[index + 1] == "-h")
        {
            result.diagnostics.push_back({
                .error = cli_error::missing_value,
                .argument = std::string{argument},
                .message = "missing value for configuration option",
            });
            continue;
        }

        result.overrides.push_back({
            .path = path,
            .value = arguments[++index],
        });
    }

    return result;
}

/// Reads the help request, the configuration file and the overrides of `argv`,
/// skipping the program name.
///
/// The overrides refer to the strings of `argv`.
[[nodiscard]]
inline cli_parse_result parse_cli(const int argc, char* const argv[])
{
    std::vector<std::string_view> arguments;
    if (argc > 1)
    {
        arguments.reserve(static_cast<std::size_t>(argc - 1));
    }

    for (int index = 1; index < argc; ++index)
    {
        arguments.emplace_back(argv[index]);
    }

    return parse_cli(std::span<const std::string_view>{arguments});
}

/// The help text of a program: its usage, the options `--help` and `--config`,
/// and every parameter of the set that can be changed, with its description,
/// the values it may take, when it matters and its current value.
[[nodiscard]]
inline std::string cli_help(
    const std::string_view program_name,
    const parameter_set& parameters)
{
    std::string output;
    output += "Usage: ";
    output.append(program_name);
    output += " [options]\n\nOptions:\n";
    output += "  -h, --help\n      Show this help message\n";
    output += "  --config <file>\n      Read configuration overrides from a file\n";

    for (const auto& parameter : parameters.parameters())
    {
        // A read-only parameter cannot be set from the command line.
        if (parameter.read_only)
            continue;
        const std::string_view placeholder =
            parameter.kind == parameter_kind::boolean ? "<true|false>" : "<value>";
        output += "  --" + parameter.path + " ";
        output.append(placeholder);
        output += '\n';
        if (!parameter.description.empty())
        {
            output += "      ";
            output.append(parameter.description);
            output += '\n';
        }
        if (parameter.domain)
            output += "      values: " + parameter.domain.text() + '\n';
        if (parameter.condition)
        {
            output += "      only if "
                + parameter.condition->text_with(
                    [](const std::string_view path) { return std::string{path}; },
                    false)
                + '\n';
        }
        output += "      current: " + parameter.value + '\n';
    }

    return output;
}

} // namespace easylocal::config
