#pragma once

#include <easylocal/config/cli.hpp>
#include <easylocal/config/file.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/validation.hpp>

#include <cstddef>
#include <ostream>
#include <span>
#include <string>
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
        result.diagnostics.push_back({
            .source = setup_diagnostic_source::override,
            .subject = diagnostic.path,
            .value = diagnostic.value,
            .message = diagnostic.message,
        });
    }
}

} // namespace detail

template<class... Children>
[[nodiscard]]
auto load_and_apply(
    const int argc,
    char* const argv[],
    const detail::root_node<Children...>& tree) -> setup_result
{
    setup_result result{};

    const auto baseline_validation = validate(tree);
    detail::append_diagnostics(result, baseline_validation);
    if (!result)
    {
        return result;
    }

    const auto cli = parse_cli(argc, argv);
    result.help_requested = cli.help_requested;
    detail::append_diagnostics(result, cli);

    // Help is a frontend action. Preserve the existing CLI semantics: once
    // requested, do not perform file I/O or mutate configuration.
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
    const auto effective_views = override_views(
        std::span<const owned_text_override>{effective_overrides});
    const auto overrides = apply_overrides(
        tree,
        std::span<const text_override>{effective_views});
    detail::append_diagnostics(result, overrides);

    if (!result)
    {
        return result;
    }

    // Keep the postcondition explicit even though apply_overrides validates
    // every modified block transactionally: untouched blocks are part of the
    // same configuration contract.
    const auto final_validation = validate(tree);
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
            if (!diagnostic.subject.empty())
            {
                output << diagnostic.subject << ": ";
            }
            output << diagnostic.message;
            break;

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
            output << diagnostic.subject;
            if (!diagnostic.value.empty())
            {
                output << " = '" << diagnostic.value << '\'';
            }
            output << ": " << diagnostic.message;
            break;
        }

        output << '\n';
    }
}

} // namespace easylocal::config
