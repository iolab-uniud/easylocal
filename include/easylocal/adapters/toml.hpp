#pragma once

/// \file
/// TOML configuration files (optional component, needs toml++): a TOML document
/// read as the path = value overrides of a config::parameter_set, nested tables
/// giving the dotted paths, with the errors of a configuration file.
///
/// load_toml_file is a config_file_reader: given to load_and_apply, or as
/// cli::options::read_config, it reads the file of `--config`.

#include <easylocal/config/file.hpp>
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
    config_file_parse_result& result)
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
            const auto& source = node.source();
            result.diagnostics.push_back({
                .error = config_file_error::unsupported_value,
                .line = static_cast<std::size_t>(source.begin.line),
                .text = std::move(path),
                .message = std::string{error},
                .column = static_cast<std::size_t>(source.begin.column),
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
    config_file_parse_result& result,
    const std::string_view source_path,
    const std::string_view message)
{
    result.diagnostics.push_back({
        .error = config_file_error::parse_error,
        .line = 0,
        .text = std::string{source_path},
        .message = std::string{message},
    });
}

// A parse error with its place: its line and column.
inline void append_toml_parse_error(
    config_file_parse_result& result,
    const std::string_view source_path,
    const toml::parse_error& error)
{
    const auto& source = error.source();
    result.diagnostics.push_back({
        .error = config_file_error::parse_error,
        .line = static_cast<std::size_t>(source.begin.line),
        .text = std::string{source_path},
        .message = std::string{error.description()},
        .column = static_cast<std::size_t>(source.begin.column),
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
config_file_parse_result parse_toml_overrides(
    const std::string_view source_path,
    Parse&& parse)
{
    config_file_parse_result result{};
#if TOML_EXCEPTIONS
    std::optional<toml::table> table;
    try
    {
        table = std::forward<Parse>(parse)();
    }
    catch (const toml::parse_error& error)
    {
        append_toml_parse_error(result, source_path, error);
    }
    catch (const std::exception& error)
    {
        append_toml_parse_error(result, source_path, error.what());
    }
    if (table)
    {
        flatten_toml_table(*table, {}, result);
    }
#else
    const auto parsed = std::forward<Parse>(parse)();
    if (!parsed)
    {
        append_toml_parse_error(result, source_path, parsed.error());
        return result;
    }
    flatten_toml_table(parsed.table(), {}, result);
#endif
    return result;
}

} // namespace detail

/// The overrides of a TOML document given as text, with its errors.
///
/// A value that cannot be read is an `unsupported_value`, with its path, and
/// the others are still read; a `parse_error`, with its line and column,
/// leaves no overrides. `source_path` names the document in the text of a
/// parse error.
[[nodiscard]]
inline config_file_parse_result parse_toml_text(
    const std::string_view text,
    const std::string_view source_path = {})
{
    return detail::parse_toml_overrides(source_path, [&] {
        return toml::parse(text, source_path);
    });
}

/// The overrides of a TOML file, with its errors, as parse_toml_text reads
/// them: a config_file_reader.
///
/// A file that cannot be read is reported as a parse error.
[[nodiscard]]
inline config_file_parse_result load_toml_file(const std::filesystem::path& path)
{
    // toml++ reads the name of a file as UTF-8.
    const auto name = easylocal::detail::utf8_text(path);
    return detail::parse_toml_overrides(name, [&] { return toml::parse_file(name); });
}

} // namespace easylocal::config
