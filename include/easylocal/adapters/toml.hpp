#pragma once

/// \file
/// TOML configuration files (optional component, needs toml++): a TOML document
/// read as the path = value overrides of a config::parameter_set, nested tables
/// giving the dotted paths.

#include <easylocal/config/overrides.hpp>

#include <toml++/toml.hpp>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace easylocal::config
{

/// The kind of an error found while reading a TOML document.
enum class toml_config_error
{
    /// The text is not valid TOML.
    parse_error,
    /// A value that is not a string, a number, a boolean or an array of numbers
    /// and booleans.
    unsupported_value,
};

/// An error found while reading a TOML document.
struct toml_config_diagnostic
{
    /// The kind of the error.
    toml_config_error error;
    /// The dotted path of the value, empty for a parse error.
    std::string path;
    /// The description of the error.
    std::string message;
};

/// The overrides read from a TOML document, and the errors found.
///
/// A value that cannot be read is reported and skipped, the others are still
/// read; a parse error leaves no overrides.
struct toml_config_parse_result
{
    /// The path = value overrides, nested tables giving the dotted paths.
    std::vector<owned_text_override> overrides;
    /// The errors found, empty when the document was read in full.
    std::vector<toml_config_diagnostic> diagnostics;

    /// Whether the document was read without errors.
    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return diagnostics.empty();
    }
};

namespace detail
{

inline void append_toml_path_segment(
    std::string& path,
    const std::string_view segment)
{
    if (!path.empty())
    {
        path += '.';
    }
    path.append(segment);
}

[[nodiscard]]
inline bool toml_scalar_text(const toml::node& node, std::string& output)
{
    if (const auto value = node.value<std::string>())
    {
        output = *value;
        return true;
    }
    if (const auto value = node.value<std::int64_t>())
    {
        output = std::to_string(*value);
        return true;
    }
    if (const auto value = node.value<double>())
    {
        output = format_value(*value);
        return true;
    }
    if (const auto value = node.value<bool>())
    {
        output = *value ? "true" : "false";
        return true;
    }

    return false;
}

[[nodiscard]]
inline bool toml_value_text(const toml::node& node, std::string& output)
{
    if (toml_scalar_text(node, output))
    {
        return true;
    }

    const auto* const array = node.as_array();
    if (array == nullptr)
    {
        return false;
    }

    output = '[';
    bool first = true;
    for (const auto& element : *array)
    {
        std::string element_text;
        if (element.is_string() || !toml_scalar_text(element, element_text))
        {
            return false;
        }

        if (!first)
        {
            output += ", ";
        }
        output += element_text;
        first = false;
    }
    output += ']';
    return true;
}

inline void flatten_toml_table(
    const toml::table& table,
    const std::string& prefix,
    toml_config_parse_result& result)
{
    for (const auto& [key, node] : table)
    {
        auto path = prefix;
        append_toml_path_segment(path, key.str());

        if (const auto* const child = node.as_table())
        {
            flatten_toml_table(*child, path, result);
            continue;
        }

        std::string value;
        if (!toml_value_text(node, value))
        {
            result.diagnostics.push_back({
                .error = toml_config_error::unsupported_value,
                .path = std::move(path),
                .message =
                    "TOML value cannot be represented by the textual override mapper",
            });
            continue;
        }

        result.overrides.push_back({
            .path = std::move(path),
            .value = std::move(value),
        });
    }
}

inline void append_toml_parse_error(
    toml_config_parse_result& result,
    const std::string_view message)
{
    result.diagnostics.push_back({
        .error = toml_config_error::parse_error,
        .path = {},
        .message = std::string{message},
    });
}

inline void append_toml_parse_error(
    toml_config_parse_result& result,
    const toml::parse_error& error)
{
    append_toml_parse_error(result, error.description());
}

// The overrides of the TOML document that parse() returns, with its parse
// error as a diagnostic. toml++ reports the error by an exception or in the
// result, as it was built. A toml++ built as a shared library (Homebrew's,
// TOML_HEADER_ONLY=0) throws parse_error from the library, where macOS may not
// match it against the parse_error of this binary: the std::exception base
// still matches, and is reported as a parse error.
template<class Parse>
[[nodiscard]]
toml_config_parse_result parse_toml_overrides(Parse&& parse)
{
    toml_config_parse_result result{};
#if TOML_EXCEPTIONS
    std::optional<toml::table> table;
    try
    {
        table = std::forward<Parse>(parse)();
    }
    catch (const toml::parse_error& error)
    {
        append_toml_parse_error(result, error);
    }
    catch (const std::exception& error)
    {
        append_toml_parse_error(result, error.what());
    }
    if (table)
    {
        flatten_toml_table(*table, {}, result);
    }
#else
    const auto parsed = std::forward<Parse>(parse)();
    if (!parsed)
    {
        append_toml_parse_error(result, parsed.error());
        return result;
    }
    flatten_toml_table(parsed.table(), {}, result);
#endif
    return result;
}

} // namespace detail

/// The overrides of a TOML document given as text, with its errors.
///
/// `source_path` names the document in the parse error messages.
[[nodiscard]]
inline toml_config_parse_result parse_toml_text(
    const std::string_view text,
    const std::string_view source_path = {})
{
    return detail::parse_toml_overrides([&] { return toml::parse(text, source_path); });
}

/// The overrides of a TOML file, with its errors.
///
/// A file that cannot be read is reported as a parse error.
[[nodiscard]]
inline toml_config_parse_result load_toml_file(const std::filesystem::path& path)
{
    return detail::parse_toml_overrides([&] { return toml::parse_file(path.string()); });
}

} // namespace easylocal::config
