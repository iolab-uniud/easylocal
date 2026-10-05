#pragma once

/// \file
/// Textual overrides (path = value) and the text form of parameter values:
/// overrides are read as booleans, numbers, strings, arrays and vectors, and
/// format_value writes them back in the same syntax.

#include <easylocal/config/parameters.hpp>
#include <easylocal/utils/detail/meta.hpp>
#include <easylocal/utils/detail/number_text.hpp>
#include <easylocal/utils/detail/text.hpp>
#include <easylocal/utils/limit.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::config
{

/// An override `path = value`, as views of a text owned elsewhere.
struct text_override
{
    /// The full path of the parameter.
    std::string_view path;
    /// The value, as text.
    std::string_view value;
};

/// An override `path = value` that owns its text.
struct owned_text_override
{
    /// The full path of the parameter.
    std::string path;
    /// The value, as text.
    std::string value;
};

/// Views of owned overrides, as parameter_set::apply() takes them; they refer
/// to the strings of `overrides`.
[[nodiscard]]
inline std::vector<text_override> override_views(
    const std::span<const owned_text_override> overrides)
{
    std::vector<text_override> result;
    result.reserve(overrides.size());

    for (const auto& candidate : overrides)
    {
        result.push_back({
            .path = candidate.path,
            .value = candidate.value,
        });
    }

    return result;
}

/// The overrides of `lower_precedence` whose path `higher_precedence` does not
/// set, followed by all those of `higher_precedence`.
///
/// For example, the overrides of a configuration file under those of the
/// command line.
[[nodiscard]]
inline std::vector<owned_text_override> overlay_overrides(
    const std::span<const owned_text_override> lower_precedence,
    const std::span<const text_override> higher_precedence)
{
    std::vector<owned_text_override> result;
    result.reserve(lower_precedence.size() + higher_precedence.size());

    for (const auto& candidate : lower_precedence)
    {
        const auto shadowed = std::ranges::any_of(
            higher_precedence,
            [&](const auto& higher) { return higher.path == candidate.path; });

        if (!shadowed)
        {
            result.push_back(candidate);
        }
    }

    for (const auto& candidate : higher_precedence)
    {
        result.push_back({
            .path = std::string{candidate.path},
            .value = std::string{candidate.value},
        });
    }

    return result;
}

/// An error of an override.
enum class override_error
{
    /// A path set by more than one override.
    duplicate_path,
    /// A path that names no parameter of the set.
    unknown_parameter,
    /// A parameter that cannot be changed.
    read_only_parameter,
    /// A value that cannot be read as the type of its parameter.
    parse_error,
    /// A block that the overrides make invalid; the path is the block's.
    validation_error,
};

/// An error of an override, with its path and value.
struct override_diagnostic
{
    /// The kind of error.
    override_error error;
    /// The path of the parameter, or of the block for a `validation_error`.
    std::string path;
    /// The value, as text; empty for a `validation_error`.
    std::string value;
    /// A description of the error.
    std::string message;
};

/// What parameter_set::apply() did: the number of blocks it changed, and the
/// errors.
///
/// It converts to true when there are no errors.
struct override_result
{
    /// The number of blocks changed; 0 when there are errors.
    std::size_t applied_parameter_blocks{};
    /// The errors; when there are any, nothing was changed.
    std::vector<override_diagnostic> diagnostics;

    /// Whether there are no diagnostics.
    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return diagnostics.empty();
    }
};

namespace detail
{

template<class T>
struct is_std_array : std::false_type
{
};

template<class Value, std::size_t Size>
struct is_std_array<std::array<Value, Size>> : std::true_type
{
    using value_type = Value;
    static constexpr std::size_t size = Size;
};

template<class T>
inline constexpr bool is_std_array_v = is_std_array<T>::value;

template<class T>
struct is_std_vector : std::false_type
{
};

template<class Value, class Allocator>
struct is_std_vector<std::vector<Value, Allocator>> : std::true_type
{
    using value_type = Value;
};

template<class T>
inline constexpr bool is_std_vector_v = is_std_vector<T>::value;

// The elements of a list, "[a, b]" or "a, b": split at the commas outside
// nested brackets, so that "[[1, 2], [3, 4]]" has two. An empty element, as a
// trailing comma leaves, is an error; "[]" has none.
[[nodiscard]]
inline std::expected<std::vector<std::string_view>, std::string_view> list_elements(
    const std::string_view text)
{
    auto body = easylocal::detail::trim_space(text);
    if (body.size() >= 2 && body.front() == '[' && body.back() == ']')
    {
        body.remove_prefix(1);
        body.remove_suffix(1);
    }
    auto elements = easylocal::detail::split_list(body);
    for (const auto element : elements)
        if (element.empty())
            return std::unexpected{std::string_view{"empty list element"}};
    return elements;
}

template<class Value>
[[nodiscard]]
std::string_view parse_text_value(const std::string_view text, Value& value)
{
    using value_type = std::remove_cvref_t<Value>;

    if constexpr (std::same_as<value_type, easylocal::limit>)
    {
        // "unlimited", or a count.
        if (easylocal::detail::trim_space(text) == "unlimited")
        {
            value = easylocal::unlimited;
            return {};
        }
        std::size_t count{};
        if (const auto error = parse_text_value(text, count); !error.empty())
            return "expected a count or 'unlimited'";
        value = count;
        return {};
    }
    else if constexpr (std::same_as<value_type, std::filesystem::path>)
    {
        value = std::filesystem::path{std::string{text}};
        return {};
    }
    else if constexpr (std::same_as<value_type, std::string>)
    {
        value = std::string{text};
        return {};
    }
    else if constexpr (std::same_as<value_type, bool>)
    {
        const auto trimmed = easylocal::detail::trim_space(text);
        if (trimmed == "true")
        {
            value = true;
            return {};
        }
        if (trimmed == "false")
        {
            value = false;
            return {};
        }
        return "expected 'true' or 'false'";
    }
    else if constexpr (std::integral<value_type>)
    {
        const auto parsed = easylocal::detail::parse_number<value_type>(text);
        if (!parsed)
            return "expected integer";
        value = *parsed;
        return {};
    }
    else if constexpr (std::floating_point<value_type>)
    {
        const auto parsed = easylocal::detail::parse_number<value_type>(text);
        if (!parsed)
            return "expected floating-point value";
        value = *parsed;
        return {};
    }
    else if constexpr (is_std_array_v<value_type>)
    {
        constexpr auto size = is_std_array<value_type>::size;

        const auto elements = list_elements(text);
        if (!elements)
            return elements.error();
        if (elements->size() != size)
            return "wrong number of array elements";
        value_type parsed{};
        for (std::size_t index = 0; index < size; ++index)
        {
            const auto error = parse_text_value((*elements)[index], parsed[index]);
            if (!error.empty())
                return error;
        }
        value = std::move(parsed);
        return {};
    }
    else if constexpr (is_std_vector_v<value_type>)
    {
        using element_type = typename is_std_vector<value_type>::value_type;

        const auto elements = list_elements(text);
        if (!elements)
            return elements.error();
        value_type parsed;
        parsed.reserve(elements->size());
        for (const auto element_text : *elements)
        {
            element_type element{};
            const auto error = parse_text_value(element_text, element);
            if (!error.empty())
                return error;
            parsed.push_back(std::move(element));
        }
        value = std::move(parsed);
        return {};
    }
    else
    {
        static_assert(
            easylocal::detail::always_false_v<value_type>,
            "parameter type has no built-in textual parser");
    }
}

} // namespace detail

/// A parameter value as text, in the syntax overrides are read in: true/false,
/// numbers, "unlimited" for an unlimited limit, [a, b] for arrays and vectors.
template<class Value>
[[nodiscard]]
std::string format_value(const Value& value)
{
    using value_type = std::remove_cvref_t<Value>;

    if constexpr (std::same_as<value_type, easylocal::limit>)
    {
        return value.is_unlimited()
            ? std::string{"unlimited"}
            : format_value(static_cast<std::size_t>(value));
    }
    else if constexpr (std::same_as<value_type, bool>)
    {
        return value ? "true" : "false";
    }
    else if constexpr (std::same_as<value_type, std::filesystem::path>)
    {
        return value.string();
    }
    else if constexpr (std::same_as<value_type, std::string>)
    {
        return value;
    }
    else if constexpr (detail::is_std_array_v<value_type>
        || detail::is_std_vector_v<value_type>)
    {
        std::string result{"["};
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            if (index != 0)
            {
                result += ", ";
            }
            result += format_value(value[index]);
        }
        result += ']';
        return result;
    }
    else if constexpr (std::integral<value_type> || std::floating_point<value_type>)
    {
        return easylocal::detail::number_text(value);
    }
    else
    {
        static_assert(
            easylocal::detail::always_false_v<value_type>,
            "parameter type has no built-in text formatter");
    }
}

} // namespace easylocal::config
