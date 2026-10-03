#pragma once

#include <easylocal/config/tree.hpp>

#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace easylocal::config
{

struct configuration_validation_diagnostic
{
    std::string path;
    std::string message;
};

struct configuration_validation_result
{
    std::vector<configuration_validation_diagnostic> diagnostics;

    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return diagnostics.empty();
    }
};

namespace detail
{

inline void append_configuration_path_segment(
    std::string& path,
    const std::string_view segment)
{
    if (!path.empty())
    {
        path += '.';
    }
    path.append(segment);
}

template<class Node>
void validate_configuration_node(
    const Node& node,
    const std::string& prefix,
    configuration_validation_result& result)
{
    std::string path = prefix;

    if constexpr (requires { std::remove_cvref_t<Node>::name(); })
    {
        append_configuration_path_segment(
            path,
            std::remove_cvref_t<Node>::name());
    }

    if constexpr (requires { node.parameters(); })
    {
        const auto validation = node.parameters().validate();
        if (!validation)
        {
            result.diagnostics.push_back({
                .path = path,
                .message = std::string{validation.message},
            });
        }
    }

    if constexpr (requires { node.children(); })
    {
        std::apply(
            [&](const auto&... children) {
                (validate_configuration_node(children, path, result), ...);
            },
            node.children());
    }
}

} // namespace detail

template<class... Children>
[[nodiscard]]
configuration_validation_result validate(const detail::root_node<Children...>& tree)
{
    configuration_validation_result result{};
    detail::validate_configuration_node(tree, {}, result);
    return result;
}

} // namespace easylocal::config
