#pragma once

/// \file
/// Command-line frontend of the parameters: parse_cli reads overrides
/// (--path value or --path=value), an optional --config file and --help from
/// argv; cli_help lists the parameters of a set with their descriptions and
/// values.

#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/utils/detail/meta.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::config
{

enum class cli_error
{
    unexpected_argument,
    malformed_option,
    missing_value,
    duplicate_config_file,
};

struct cli_diagnostic
{
    cli_error error;
    std::string argument;
    std::string message;
};

struct cli_parse_result
{
    bool help_requested{};
    std::optional<std::filesystem::path> config_file;
    std::vector<text_override> overrides;
    std::vector<cli_diagnostic> diagnostics;

    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return diagnostics.empty();
    }
};

namespace detail
{

} // namespace detail

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
        output += "  --" + parameter.path + " <value>\n";
        if (!parameter.description.empty())
        {
            output += "      ";
            output.append(parameter.description);
            output += '\n';
        }
        output += "      current: " + parameter.value + '\n';
    }

    return output;
}

} // namespace easylocal::config
