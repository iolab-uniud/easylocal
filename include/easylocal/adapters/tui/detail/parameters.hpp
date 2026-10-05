#pragma once

/// \file
/// The fields of the interactive tester's parameters window: the parameters
/// of a set as editable text, their changes and the errors that reject them.

#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameter_set.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace easylocal::tui::detail
{

// One editable parameter of the parameters window: its full configuration
// path, the label shown (the path without its first segment), the value when
// the tester started, the value when the window opened, and the edited text.
struct parameter_field
{
    std::string path;
    std::string label;
    std::string description;
    std::string original;
    std::string current;
    std::string text;
    int cursor{}; // the input's cursor, at the end of the text when it opens
};

// The parameters of a set whose path starts with prefix, as editable fields;
// the label is the path without the prefix when strip is set.
[[nodiscard]] inline std::vector<parameter_field> parameter_fields(
    const easylocal::config::parameter_set& parameters,
    const std::string_view prefix,
    const bool strip = true)
{
    std::vector<parameter_field> fields;
    for (const auto& parameter : parameters.parameters())
    {
        if (!parameter.path.starts_with(prefix))
            continue;
        fields.push_back(
            parameter_field{
                .path = parameter.path,
                .label = strip ? parameter.path.substr(prefix.size()) : parameter.path,
                .description = std::string{parameter.description},
                .original = parameter.value,
                .current = parameter.value,
                .text = parameter.value,
                .cursor = static_cast<int>(parameter.value.size()),
            });
    }
    return fields;
}

// The overrides of the fields whose text was edited; they refer to the
// fields' strings.
[[nodiscard]] inline std::vector<easylocal::config::text_override> changed_parameters(
    const std::vector<parameter_field>& fields)
{
    std::vector<easylocal::config::text_override> overrides;
    for (const auto& field : fields)
        if (field.text != field.current)
            overrides.push_back({.path = field.path, .value = field.text});
    return overrides;
}

// The diagnostics of a rejected change, one per line, with the paths as the
// window shows them (without prefix); a diagnostic about the whole block the
// window shows has no path left, only its message.
[[nodiscard]] inline std::string parameter_errors(
    const easylocal::config::override_result& result,
    const std::string_view prefix = {})
{
    std::string text;
    for (const auto& diagnostic : result.diagnostics)
    {
        if (!text.empty())
            text += '\n';
        std::string_view path = diagnostic.path;
        if (path.starts_with(prefix))
            path.remove_prefix(prefix.size());
        else if (!prefix.empty() && prefix.substr(0, prefix.size() - 1) == path)
            path = {};
        if (!path.empty())
            text += std::string{path} + ": ";
        text += diagnostic.message;
    }
    return text;
}

} // namespace easylocal::tui::detail
