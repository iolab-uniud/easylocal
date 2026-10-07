#pragma once

/// \file
/// load_and_apply: the usual setup of a program's parameters, from its
/// defaults, then an optional configuration file, then the command line, with
/// all the diagnostics collected and printed together.

#include <easylocal/config/cli.hpp>
#include <easylocal/config/file.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameter_set.hpp>

#include <cstddef>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace easylocal::config
{

/// The step of load_and_apply an error comes from.
enum class setup_diagnostic_source
{
    /// The validation of a parameter block.
    validation,
    /// The command line (parse_cli).
    command_line,
    /// The configuration file (load_config_file).
    config_file,
    /// The application of the overrides (parameter_set::apply()).
    override,
};

/// An error of load_and_apply, from any of its steps.
struct setup_diagnostic
{
    /// The step it comes from.
    setup_diagnostic_source source;
    /// What it is about: a path, an argument, or a line of the configuration
    /// file.
    std::string subject;
    /// The value of the override it is about, if any.
    std::string value;
    /// The number of the line of the configuration file, from 1; 0 otherwise.
    std::size_t line{};
    /// A description of the error.
    std::string message;
};

/// What load_and_apply did: whether help was requested, and the errors.
///
/// It converts to true when there are no errors.
struct setup_result
{
    /// Whether `-h` or `--help` was given; then nothing was applied.
    bool help_requested{};
    /// The errors of every step.
    std::vector<setup_diagnostic> diagnostics;

    /// Whether there are no diagnostics.
    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return diagnostics.empty();
    }
};

namespace detail
{

inline void append_diagnostics(
    setup_result& result,
    const cli_parse_result& cli)
{
    for (const auto& diagnostic : cli.diagnostics)
    {
        result.diagnostics.push_back({
            .source = setup_diagnostic_source::command_line,
            .subject = diagnostic.argument,
            .value = {},
            .line = 0,
            .message = diagnostic.message,
        });
    }
}

inline void append_diagnostics(
    setup_result& result,
    const config_file_parse_result& file)
{
    for (const auto& diagnostic : file.diagnostics)
    {
        result.diagnostics.push_back({
            .source = setup_diagnostic_source::config_file,
            .subject = diagnostic.text,
            .value = {},
            .line = diagnostic.line,
            .message = diagnostic.message,
        });
    }
}

inline void append_diagnostics(
    setup_result& result,
    const override_result& overrides)
{
    for (const auto& diagnostic : overrides.diagnostics)
    {
        // A block that is not valid, with the batch applied, is a validation
        // error; the others are about one override.
        result.diagnostics.push_back({
            .source = diagnostic.error == override_error::validation_error
                ? setup_diagnostic_source::validation
                : setup_diagnostic_source::override,
            .subject = diagnostic.path,
            .value = diagnostic.value,
            .line = 0,
            .message = diagnostic.message,
        });
    }
}

} // namespace detail

/// Applies to the parameters the overrides of an optional `--config` file and
/// then those of the command line, which take precedence.
///
/// Nothing is changed unless every override applies and leaves its block
/// valid, and every other block is valid. With `--help`, nothing is read or
/// applied: the program prints cli_help() instead.
[[nodiscard]]
inline setup_result load_and_apply(
    const int argc,
    char* const argv[],
    const parameter_set& parameters)
{
    setup_result result{};

    const auto cli = parse_cli(argc, argv);
    result.help_requested = cli.help_requested;
    detail::append_diagnostics(result, cli);

    // Help is a frontend action. It must remain available even when the
    // program intentionally starts from incomplete/invalid defaults.
    if (result.help_requested)
    {
        return result;
    }

    config_file_parse_result file_configuration{};
    if (cli.config_file.has_value())
    {
        file_configuration = load_config_file(*cli.config_file);
        detail::append_diagnostics(result, file_configuration);
    }

    if (!result)
    {
        return result;
    }

    const auto effective_overrides = overlay_overrides(
        std::span<const owned_text_override>{file_configuration.overrides},
        std::span<const text_override>{cli.overrides});

    const auto overrides =
        parameters.apply(std::span<const owned_text_override>{effective_overrides});
    // apply() validates every block, the touched ones on their copies, before
    // committing anything: invalid defaults pass when the batch fixes them.
    detail::append_diagnostics(result, overrides);
    return result;
}

/// Writes each error of a setup to `output`, one per line, after `error: `.
inline void print_diagnostics(
    std::ostream& output,
    const setup_result& result)
{
    for (const auto& diagnostic : result.diagnostics)
    {
        output << "error: ";

        switch (diagnostic.source)
        {
        case setup_diagnostic_source::validation:
        case setup_diagnostic_source::command_line:
            if (!diagnostic.subject.empty())
            {
                output << diagnostic.subject << ": ";
            }
            output << diagnostic.message;
            break;

        case setup_diagnostic_source::config_file:
            if (diagnostic.line != 0)
            {
                output << "config line " << diagnostic.line << ": ";
            }
            output << diagnostic.message;
            if (!diagnostic.subject.empty())
            {
                output << " ('" << diagnostic.subject << "')";
            }
            break;

        case setup_diagnostic_source::override:
            // The subject is empty for a block at the root of the set.
            if (!diagnostic.subject.empty())
            {
                output << diagnostic.subject;
                if (!diagnostic.value.empty())
                    output << " = '" << diagnostic.value << '\'';
                output << ": ";
            }
            output << diagnostic.message;
            break;
        }

        output << '\n';
    }
}

} // namespace easylocal::config
