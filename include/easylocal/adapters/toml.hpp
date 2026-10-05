#pragma once

/// \file
/// TOML configuration files (optional component, needs toml++): a TOML document
/// read as the path = value overrides of a config::parameter_set, nested tables
/// giving the dotted paths.

#include <easylocal/config/overrides.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>
#include <utility>
#include <vector>

namespace easylocal::config
{

/// The kind of an error found while reading a TOML document.
enum class toml_config_error
{
    /// The text is not valid TOML.
    parse_error,
    /// A value that is not a string, a number, a boolean or an array of numbers,
    /// booleans and such arrays: a date, a time, or an array holding a string.
    unsupported_value,
};

/// An error found while reading a TOML document.
struct toml_config_diagnostic
{
    /// The kind of the error.
    toml_config_error error;
    /// The dotted path of the value, empty for a parse error.
    std::string path;
    /// The description of the error; for a parse error, after the place of
    /// the error, `file:line:column: ` (`line:column: ` without a file name).
    std::string message;
    /// The line of a parse error, from 1; 0 when it is not known.
    std::size_t line{};
    /// The column of a parse error, from 1; 0 when it is not known.
    std::size_t column{};
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

// The text of a scalar, by the type of its node: node.value<T>() converts
// between types (true reads as the integer 1), so each type is read as its own.
// A float keeps a decimal point, so that 3.0 does not set an integer.
[[nodiscard]]
inline bool toml_scalar_text(const toml::node& node, std::string& output)
{
    if (const auto* const text = node.as_string())
    {
        output = text->get();
        return true;
    }
    if (const auto* const boolean = node.as_boolean())
    {
        output = boolean->get() ? "true" : "false";
        return true;
    }
    if (const auto* const integer = node.as_integer())
    {
        output = std::to_string(integer->get());
        return true;
    }
    if (const auto* const real = node.as_floating_point())
    {
        output = format_value(real->get());
        if (output.find_first_not_of("+-0123456789") == std::string::npos)
            output += ".0";
        return true;
    }

    return false;
}

// The text of a value, or why it has none. An array is the text of a list,
// [a, b], its elements numbers, booleans or arrays of them: a string inside an
// array has no text yet, since a list's elements are not quoted.
[[nodiscard]]
inline std::string_view toml_value_text(const toml::node& node, std::string& output)
{
    const auto* const array = node.as_array();
    if (array == nullptr)
    {
        if (toml_scalar_text(node, output))
            return {};
        return "a TOML date or time is not a parameter value";
    }

    output = '[';
    bool first = true;
    for (const auto& element : *array)
    {
        if (element.is_string())
            return "an array cannot hold strings: its elements must be numbers, "
                   "booleans or arrays";
        std::string element_text;
        if (const auto error = toml_value_text(element, element_text); !error.empty())
            return error;
        if (!first)
            output += ", ";
        output += element_text;
        first = false;
    }
    output += ']';
    return {};
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
        if (const auto error = toml_value_text(node, value); !error.empty())
        {
            result.diagnostics.push_back({
                .error = toml_config_error::unsupported_value,
                .path = std::move(path),
                .message = std::string{error},
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

// A parse error with its place: file:line:column: description.
inline void append_toml_parse_error(
    toml_config_parse_result& result,
    const toml::parse_error& error)
{
    const auto& source = error.source();
    const auto line = static_cast<std::size_t>(source.begin.line);
    const auto column = static_cast<std::size_t>(source.begin.column);
    std::string place;
    if (source.path && !source.path->empty())
        place = *source.path + ':';
    if (line != 0)
        place += std::to_string(line) + ':' + std::to_string(column) + ':';
    result.diagnostics.push_back({
        .error = toml_config_error::parse_error,
        .path = {},
        .message = place.empty()
            ? std::string{error.description()}
            : place + ' ' + std::string{error.description()},
        .line = line,
        .column = column,
    });
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
