#pragma once

#include <easylocal/utils/detail/meta.hpp>
#include <easylocal/config/tree.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::config
{

struct text_override
{
    std::string_view path;
    std::string_view value;
};

struct owned_text_override
{
    std::string path;
    std::string value;
};

[[nodiscard]]
inline auto override_views(const std::span<const owned_text_override> overrides)
    -> std::vector<text_override>
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

[[nodiscard]]
inline auto overlay_overrides(
    const std::span<const owned_text_override> lower_precedence,
    const std::span<const text_override> higher_precedence)
    -> std::vector<owned_text_override>
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

enum class override_error
{
    duplicate_path,
    unknown_parameter,
    read_only_parameter,
    parse_error,
    validation_error,
};

struct override_diagnostic
{
    override_error error;
    std::string path;
    std::string value;
    std::string message;
};

struct override_result
{
    std::size_t applied_parameter_blocks{};
    std::vector<override_diagnostic> diagnostics;

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

[[nodiscard]]
constexpr auto trim_ascii_space(std::string_view text) noexcept
    -> std::string_view
{
    while (!text.empty() &&
           (text.front() == ' ' || text.front() == '\t' ||
            text.front() == '\n' || text.front() == '\r'))
    {
        text.remove_prefix(1);
    }

    while (!text.empty() &&
           (text.back() == ' ' || text.back() == '\t' ||
            text.back() == '\n' || text.back() == '\r'))
    {
        text.remove_suffix(1);
    }

    return text;
}

template<class Value>
[[nodiscard]]
auto parse_text_value(const std::string_view text, Value& value)
    -> std::string_view
{
    using value_type = std::remove_cvref_t<Value>;

    if constexpr (std::same_as<value_type, std::filesystem::path>)
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
        const auto trimmed = trim_ascii_space(text);
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
    else if constexpr (
        std::integral<value_type> && !std::same_as<value_type, bool>)
    {
        const auto trimmed = trim_ascii_space(text);
        if (trimmed.empty())
        {
            return "expected integer";
        }

        value_type parsed{};
        const auto* const first = trimmed.data();
        const auto* const last = first + trimmed.size();
        const auto result = std::from_chars(first, last, parsed, 10);
        if (result.ec != std::errc{} || result.ptr != last)
        {
            return "expected integer";
        }

        value = parsed;
        return {};
    }
    else if constexpr (std::floating_point<value_type>)
    {
        const auto trimmed = trim_ascii_space(text);
        if (trimmed.empty())
        {
            return "expected floating-point value";
        }

        value_type parsed{};
        const auto* const first = trimmed.data();
        const auto* const last = first + trimmed.size();
        const auto result = std::from_chars(
            first,
            last,
            parsed,
            std::chars_format::general);
        if (result.ec != std::errc{} || result.ptr != last)
        {
            return "expected floating-point value";
        }

        value = parsed;
        return {};
    }
    else if constexpr (is_std_array_v<value_type>)
    {
        using element_type = typename is_std_array<value_type>::value_type;
        constexpr auto size = is_std_array<value_type>::size;

        auto body = trim_ascii_space(text);
        if (body.size() >= 2 && body.front() == '[' && body.back() == ']')
        {
            body.remove_prefix(1);
            body.remove_suffix(1);
        }

        value_type parsed{};
        std::size_t index = 0;

        while (true)
        {
            if (index == size)
            {
                if (!trim_ascii_space(body).empty())
                {
                    return "too many array elements";
                }
                break;
            }

            const auto comma = body.find(',');
            const auto token = comma == std::string_view::npos
                ? body
                : body.substr(0, comma);

            element_type element{};
            const auto error = parse_text_value(token, element);
            if (!error.empty())
            {
                return error;
            }
            parsed[index++] = std::move(element);

            if (comma == std::string_view::npos)
            {
                body = {};
                break;
            }
            body.remove_prefix(comma + 1);
        }

        if (index != size)
        {
            return "wrong number of array elements";
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

template<class Path>
[[nodiscard]]
constexpr auto path_equals(const std::string_view text) noexcept -> bool
{
    constexpr auto segments = Path::segments();
    auto remaining = text;

    for (std::size_t index = 0; index < segments.size(); ++index)
    {
        const auto segment = segments[index];
        if (!remaining.starts_with(segment))
        {
            return false;
        }
        remaining.remove_prefix(segment.size());

        if (index + 1 == segments.size())
        {
            return remaining.empty();
        }

        if (remaining.empty() || remaining.front() != '.')
        {
            return false;
        }
        remaining.remove_prefix(1);
    }

    return remaining.empty();
}

template<fixed_string... Segments>
[[nodiscard]]
auto joined_path() -> std::string
{
    std::string result;
    bool first = true;
    auto append = [&result, &first](const std::string_view segment) {
        if (!first)
        {
            result.push_back('.');
        }
        result.append(segment);
        first = false;
    };
    (append(Segments.view()), ...);
    return result;
}

struct override_context
{
    std::span<const text_override> overrides;
    std::span<std::size_t> match_counts;
    std::vector<override_diagnostic>& diagnostics;
};

template<class Node, class Parameters, fixed_string... Prefix>
void stage_parameter_block(
    const Node& node,
    Parameters& staged,
    override_context& context,
    bool& touched,
    bool& parse_failed)
{
    using prefix_type = path_prefix<Prefix...>;
    for_each_parameter(
        staged,
        [&](const auto descriptor, auto& value) {
            using descriptor_type = std::remove_cvref_t<decltype(descriptor)>;
            using path_type = parameter_path<descriptor_type, prefix_type>;

            for (std::size_t index = 0; index < context.overrides.size(); ++index)
            {
                const auto& candidate = context.overrides[index];
                if (!path_equals<path_type>(candidate.path))
                {
                    continue;
                }

                ++context.match_counts[index];
                touched = true;

                if constexpr (requires { node.configure(staged); })
                {
                    const auto error = parse_text_value(candidate.value, value);
                    if (!error.empty())
                    {
                        parse_failed = true;
                        context.diagnostics.push_back({
                            .error = override_error::parse_error,
                            .path = std::string{candidate.path},
                            .value = std::string{candidate.value},
                            .message = std::string{error},
                        });
                    }
                }
                else
                {
                    parse_failed = true;
                    context.diagnostics.push_back({
                        .error = override_error::read_only_parameter,
                        .path = std::string{candidate.path},
                        .value = std::string{candidate.value},
                        .message = "parameter is read-only",
                    });
                }
            }
        });
}

template<bool Commit, fixed_string... Path, class Node>
void process_parameter_block(
    const Node& node,
    override_context& context,
    std::size_t& applied_blocks)
{
    using parameters_type = typename Node::parameters_type;

    parameters_type staged = node.parameters();
    bool touched = false;
    bool parse_failed = false;

    stage_parameter_block<Node, parameters_type, Path...>(
        node,
        staged,
        context,
        touched,
        parse_failed);

    if (!touched || parse_failed)
    {
        return;
    }

    const auto validation = staged.validate();
    if (!validation)
    {
        if constexpr (!Commit)
        {
            context.diagnostics.push_back({
                .error = override_error::validation_error,
                .path = joined_path<Path...>(),
                .value = {},
                .message = std::string{validation.message},
            });
        }
        return;
    }

    if constexpr (Commit)
    {
        if constexpr (requires { node.configure(std::move(staged)); })
        {
            // The S26d endpoint contract requires configure() to accept every
            // block whose validate() succeeds. A failure here is therefore a
            // broken endpoint invariant, not a recoverable external-input error.
            const auto committed = node.configure(std::move(staged));
            assert(committed);
            if (!committed)
            {
                std::terminate();
            }
            ++applied_blocks;
        }
    }
}

template<bool Commit,
         fixed_string... Prefix,
         fixed_string Name,
         parameter_block Parameters>
void process_overrides(
    const parameter_node<Name, Parameters>& node,
    override_context& context,
    std::size_t& applied_blocks)
{
    process_parameter_block<Commit, Prefix..., Name>(
        node,
        context,
        applied_blocks);
}

template<bool Commit,
         fixed_string... Prefix,
         fixed_string Name,
         configurable_endpoint Endpoint>
void process_overrides(
    const configurable_node<Name, Endpoint>& node,
    override_context& context,
    std::size_t& applied_blocks)
{
    process_parameter_block<Commit, Prefix..., Name>(
        node,
        context,
        applied_blocks);
}

template<bool Commit,
         fixed_string... Prefix,
         fixed_string Name,
         named_configuration_node... Children>
void process_overrides(
    const group_node<Name, Children...>& node,
    override_context& context,
    std::size_t& applied_blocks)
{
    std::apply(
        [&](const auto&... children) {
            (process_overrides<Commit, Prefix..., Name>(
                 children,
                 context,
                 applied_blocks),
             ...);
        },
        node.children());
}

template<bool Commit,
         fixed_string... Prefix,
         fixed_string Name,
         parameter_block Parameters,
         named_configuration_node... Children>
void process_overrides(
    const parameter_group_node<Name, Parameters, Children...>& node,
    override_context& context,
    std::size_t& applied_blocks)
{
    process_parameter_block<Commit, Prefix..., Name>(
        node,
        context,
        applied_blocks);

    std::apply(
        [&](const auto&... children) {
            (process_overrides<Commit, Prefix..., Name>(
                 children,
                 context,
                 applied_blocks),
             ...);
        },
        node.children());
}

template<bool Commit, named_configuration_node... Children>
void process_overrides(
    const root_node<Children...>& tree,
    override_context& context,
    std::size_t& applied_blocks)
{
    std::apply(
        [&](const auto&... children) {
            (process_overrides<Commit>(children, context, applied_blocks), ...);
        },
        tree.children());
}

} // namespace detail

template<class... Children>
[[nodiscard]]
auto apply_overrides(
    const detail::root_node<Children...>& tree,
    const std::span<const text_override> overrides) -> override_result
{
    override_result result{};
    std::vector<std::size_t> match_counts(overrides.size(), 0);

    for (std::size_t first = 0; first < overrides.size(); ++first)
    {
        for (std::size_t second = first + 1; second < overrides.size(); ++second)
        {
            if (overrides[first].path == overrides[second].path)
            {
                result.diagnostics.push_back({
                    .error = override_error::duplicate_path,
                    .path = std::string{overrides[second].path},
                    .value = std::string{overrides[second].value},
                    .message = "duplicate override path",
                });
            }
        }
    }

    detail::override_context context{
        .overrides = overrides,
        .match_counts = match_counts,
        .diagnostics = result.diagnostics,
    };

    std::size_t ignored_applied_blocks = 0;
    detail::process_overrides<false>(tree, context, ignored_applied_blocks);

    for (std::size_t index = 0; index < overrides.size(); ++index)
    {
        if (match_counts[index] == 0)
        {
            result.diagnostics.push_back({
                .error = override_error::unknown_parameter,
                .path = std::string{overrides[index].path},
                .value = std::string{overrides[index].value},
                .message = "unknown configuration parameter",
            });
        }
    }

    if (!result.diagnostics.empty())
    {
        return result;
    }

    std::fill(match_counts.begin(), match_counts.end(), 0);
    detail::process_overrides<true>(tree, context, result.applied_parameter_blocks);
    return result;
}

template<class... Children, std::size_t Size>
[[nodiscard]]
auto apply_overrides(
    const detail::root_node<Children...>& tree,
    const std::array<text_override, Size>& overrides) -> override_result
{
    return apply_overrides(
        tree,
        std::span<const text_override>{overrides.data(), overrides.size()});
}

} // namespace easylocal::config
