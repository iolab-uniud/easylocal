#pragma once

// TOML configuration files (optional component, needs toml++): a TOML document
// read as the path = value overrides of a config::parameter_set, nested tables
// giving the dotted paths.

#include <easylocal/config/overrides.hpp>

#include <toml++/toml.hpp>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace easylocal::config
{

enum class toml_config_error
{
    parse_error,
    unsupported_value,
};

struct toml_config_diagnostic
{
    toml_config_error error;
    std::string path;
    std::string message;
};

struct toml_config_parse_result
{
    std::vector<owned_text_override> overrides;
    std::vector<toml_config_diagnostic> diagnostics;

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
        std::ostringstream stream;
        stream << std::setprecision(std::numeric_limits<double>::max_digits10)
               << *value;
        output = stream.str();
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

#if TOML_EXCEPTIONS
// Parse with toml++ in exception mode. A toml++ built as a shared library
// (Homebrew's, TOML_HEADER_ONLY=0) throws parse_error from the library, where
// macOS may not match it against the parse_error of this binary: the
// std::exception base still matches, and is reported as a parse error.
template<class Parse>
[[nodiscard]]
std::optional<toml::table> parse_toml_table(
    toml_config_parse_result& result,
    Parse&& parse)
{
    try
    {
        return std::forward<Parse>(parse)();
    }
    catch (const toml::parse_error& error)
    {
        append_toml_parse_error(result, error);
    }
    catch (const std::exception& error)
    {
        append_toml_parse_error(result, error.what());
    }
    return std::nullopt;
}
#endif

} // namespace detail

[[nodiscard]]
inline toml_config_parse_result parse_toml_text(
    const std::string_view text,
    const std::string_view source_path = {})
{
    toml_config_parse_result result{};

#if TOML_EXCEPTIONS
    const auto table = detail::parse_toml_table(
        result, [&] { return toml::parse(text, source_path); });
    if (table)
    {
        detail::flatten_toml_table(*table, {}, result);
    }
#else
    const auto parsed = toml::parse(text, source_path);
    if (!parsed)
    {
        detail::append_toml_parse_error(result, parsed.error());
        return result;
    }
    detail::flatten_toml_table(parsed.table(), {}, result);
#endif

    return result;
}

[[nodiscard]]
inline toml_config_parse_result load_toml_file(const std::filesystem::path& path)
{
#if TOML_EXCEPTIONS
    toml_config_parse_result result{};
    const auto table = detail::parse_toml_table(
        result, [&] { return toml::parse_file(path.string()); });
    if (table)
    {
        detail::flatten_toml_table(*table, {}, result);
    }
    return result;
#else
    const auto parsed = toml::parse_file(path.string());
    toml_config_parse_result result{};
    if (!parsed)
    {
        detail::append_toml_parse_error(result, parsed.error());
        return result;
    }
    detail::flatten_toml_table(parsed.table(), {}, result);
    return result;
#endif
}

} // namespace easylocal::config
