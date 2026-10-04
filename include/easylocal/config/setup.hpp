#pragma once

/// \file
/// load_and_apply: the usual setup of a program's parameters, from its defaults,
/// then an optional configuration file, then the command line, with all the
/// diagnostics collected and printed together.

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

enum class setup_diagnostic_source
{
    validation,
    command_line,
    config_file,
    override,
};

struct setup_diagnostic
{
    setup_diagnostic_source source;
    std::string subject;
    std::string value;
    std::size_t line{};
    std::string message;
};

struct setup_result
{
    bool help_requested{};
    std::vector<setup_diagnostic> diagnostics;

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
    const configuration_validation_result& validation)
{
    for (const auto& diagnostic : validation.diagnostics)
    {
        result.diagnostics.push_back({
            .source = setup_diagnostic_source::validation,
            .subject = diagnostic.path,
            .value = {},
            .line = 0,
            .message = diagnostic.message,
        });
    }
}

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

inline bool block_has_direct_override(
    const std::string_view block_path,
    const std::span<const owned_text_override> overrides)
{
    for (const auto& candidate : overrides)
    {
        const std::string_view path{candidate.path};
        // A block at the root of the set: its fields are the paths without a dot.
        if (block_path.empty())
        {
            if (path.find('.') == std::string_view::npos)
                return true;
            continue;
        }
        if (!path.starts_with(block_path) ||
            path.size() <= block_path.size() + 1 ||
            path[block_path.size()] != '.')
        {
            continue;
        }

        const auto field = path.substr(block_path.size() + 1);
        if (field.find('.') == std::string_view::npos)
        {
            return true;
        }
    }

    return false;
}

inline void append_diagnostics(
    setup_result& result,
    const override_result& overrides)
{
    for (const auto& diagnostic : overrides.diagnostics)
    {
        result.diagnostics.push_back({
            .source = setup_diagnostic_source::override,
            .subject = diagnostic.path,
            .value = diagnostic.value,
            .line = 0,
            .message = diagnostic.message,
        });
    }
}

} // namespace detail

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

    // Invalid defaults are allowed when this batch directly overrides that
    // parameter block: apply_overrides() will validate the staged candidate
    // before committing anything. Invalid untouched blocks, however, make the
    // whole transaction fail before any mutation can occur.
    const auto baseline_validation = parameters.validate();
    for (const auto& diagnostic : baseline_validation.diagnostics)
    {
        if (!detail::block_has_direct_override(
                diagnostic.path,
                std::span<const owned_text_override>{effective_overrides}))
        {
            result.diagnostics.push_back({
                .source = setup_diagnostic_source::validation,
                .subject = diagnostic.path,
                .value = {},
                .line = 0,
                .message = diagnostic.message,
            });
        }
    }

    if (!result)
    {
        return result;
    }

    const auto effective_views = override_views(
        std::span<const owned_text_override>{effective_overrides});
    const auto overrides =
        parameters.apply(std::span<const text_override>{effective_views});
    detail::append_diagnostics(result, overrides);

    if (!result)
    {
        return result;
    }

    // At this point every untouched block was valid at baseline and every
    // touched block was validated transactionally by apply_overrides().
    // Keep this as a defensive check of that invariant.
    const auto final_validation = parameters.validate();
    detail::append_diagnostics(result, final_validation);
    return result;
}

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
