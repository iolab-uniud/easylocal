#pragma once

#include <easylocal/config/parameters.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::config
{

template<class Descriptor, fixed_string... Nodes>
struct parameter_path
{
    static constexpr std::size_t size = sizeof...(Nodes) + 1;

    [[nodiscard]]
    static constexpr auto segments() noexcept
        -> std::array<std::string_view, size>
    {
        return {Nodes.view()..., Descriptor::name()};
    }
};

namespace detail
{

struct named_configuration_node_marker
{
};

template<class T>
concept named_configuration_node =
    requires
    {
        typename std::remove_cvref_t<T>::configuration_node_marker;
        requires std::same_as<
            typename std::remove_cvref_t<T>::configuration_node_marker,
            named_configuration_node_marker>;
        {
            std::remove_cvref_t<T>::name()
        } -> std::same_as<std::string_view>;
    };

template<class... Nodes>
[[nodiscard]]
consteval auto unique_node_names() noexcept -> bool
{
    if constexpr (sizeof...(Nodes) <= 1)
    {
        return true;
    }
    else
    {
        return []<class First, class... Rest>(std::type_identity<First>,
                                              std::type_identity<Rest>...) {
            return ((First::name() != Rest::name()) && ...) &&
                   unique_node_names<Rest...>();
        }(std::type_identity<Nodes>{}...);
    }
}

template<fixed_string Name, parameter_block Parameters>
class parameter_node
{
    static_assert(
        !Name.view().empty(),
        "configuration node name must not be empty");

public:
    using configuration_node_marker = named_configuration_node_marker;

    explicit constexpr parameter_node(Parameters& parameters) noexcept
        : parameters_{std::addressof(parameters)}
    {
    }

    [[nodiscard]]
    static constexpr auto name() noexcept -> std::string_view
    {
        return Name.view();
    }

    [[nodiscard]]
    constexpr auto parameters() const noexcept -> const Parameters&
    {
        return *parameters_;
    }

private:
    Parameters* parameters_;
};

template<fixed_string Name, named_configuration_node... Children>
class group_node
{
    static_assert(
        !Name.view().empty(),
        "configuration node name must not be empty");
    static_assert(sizeof...(Children) >= 1);
    static_assert(
        unique_node_names<Children...>(),
        "configuration siblings must have unique names");

public:
    using configuration_node_marker = named_configuration_node_marker;

    explicit constexpr group_node(Children... children) noexcept(
        (std::is_nothrow_move_constructible_v<Children> && ...))
        : children_{std::move(children)...}
    {
    }

    [[nodiscard]]
    static constexpr auto name() noexcept -> std::string_view
    {
        return Name.view();
    }

    [[nodiscard]]
    constexpr auto children() const noexcept -> const std::tuple<Children...>&
    {
        return children_;
    }

private:
    std::tuple<Children...> children_;
};

template<fixed_string Name,
         parameter_block Parameters,
         named_configuration_node... Children>
class parameter_group_node
{
    static_assert(
        !Name.view().empty(),
        "configuration node name must not be empty");
    static_assert(sizeof...(Children) >= 1);
    static_assert(
        unique_node_names<Children...>(),
        "configuration siblings must have unique names");

public:
    using configuration_node_marker = named_configuration_node_marker;

    explicit constexpr parameter_group_node(
        Parameters& parameters,
        Children... children) noexcept(
        (std::is_nothrow_move_constructible_v<Children> && ...))
        : parameters_{std::addressof(parameters)},
          children_{std::move(children)...}
    {
    }

    [[nodiscard]]
    static constexpr auto name() noexcept -> std::string_view
    {
        return Name.view();
    }

    [[nodiscard]]
    constexpr auto parameters() const noexcept -> const Parameters&
    {
        return *parameters_;
    }

    [[nodiscard]]
    constexpr auto children() const noexcept -> const std::tuple<Children...>&
    {
        return children_;
    }

private:
    Parameters* parameters_;
    std::tuple<Children...> children_;
};

template<named_configuration_node... Children>
class root_node
{
    static_assert(sizeof...(Children) >= 1);
    static_assert(
        unique_node_names<Children...>(),
        "configuration siblings must have unique names");

public:
    explicit constexpr root_node(Children... children) noexcept(
        (std::is_nothrow_move_constructible_v<Children> && ...))
        : children_{std::move(children)...}
    {
    }

    [[nodiscard]]
    constexpr auto children() const noexcept -> const std::tuple<Children...>&
    {
        return children_;
    }

private:
    std::tuple<Children...> children_;
};

template<fixed_string... Prefix,
         fixed_string Name,
         parameter_block Parameters,
         class Function>
constexpr void visit_parameters(
    const parameter_node<Name, Parameters>& node,
    Function& function)
{
    for_each_parameter(
        node.parameters(),
        [&function](const auto descriptor, const auto& value) {
            using descriptor_type = std::remove_cvref_t<decltype(descriptor)>;
            using path_type = config::parameter_path<
                descriptor_type,
                Prefix...,
                Name>;
            std::invoke(function, path_type{}, descriptor, value);
        });
}

template<fixed_string... Prefix,
         fixed_string Name,
         named_configuration_node... Children,
         class Function>
constexpr void visit_parameters(
    const group_node<Name, Children...>& node,
    Function& function)
{
    std::apply(
        [&function](const auto&... children) {
            (visit_parameters<Prefix..., Name>(children, function), ...);
        },
        node.children());
}

template<fixed_string... Prefix,
         fixed_string Name,
         parameter_block Parameters,
         named_configuration_node... Children,
         class Function>
constexpr void visit_parameters(
    const parameter_group_node<Name, Parameters, Children...>& node,
    Function& function)
{
    for_each_parameter(
        node.parameters(),
        [&function](const auto descriptor, const auto& value) {
            using descriptor_type = std::remove_cvref_t<decltype(descriptor)>;
            using path_type = config::parameter_path<
                descriptor_type,
                Prefix...,
                Name>;
            std::invoke(function, path_type{}, descriptor, value);
        });

    std::apply(
        [&function](const auto&... children) {
            (visit_parameters<Prefix..., Name>(children, function), ...);
        },
        node.children());
}

template<named_configuration_node... Children, class Function>
constexpr void visit_parameters(
    const root_node<Children...>& tree,
    Function& function)
{
    std::apply(
        [&function](const auto&... children) {
            (visit_parameters(children, function), ...);
        },
        tree.children());
}

} // namespace detail

template<fixed_string Name, parameter_block Parameters>
[[nodiscard]]
constexpr auto named(Parameters& parameters) noexcept
    -> detail::parameter_node<Name, Parameters>
{
    return detail::parameter_node<Name, Parameters>{parameters};
}

template<fixed_string Name, detail::named_configuration_node... Children>
    requires(sizeof...(Children) >= 1)
[[nodiscard]]
constexpr auto named(Children... children) noexcept(
    (std::is_nothrow_move_constructible_v<Children> && ...))
    -> detail::group_node<Name, Children...>
{
    return detail::group_node<Name, Children...>{std::move(children)...};
}

template<fixed_string Name,
         parameter_block Parameters,
         detail::named_configuration_node... Children>
    requires(sizeof...(Children) >= 1)
[[nodiscard]]
constexpr auto named(Parameters& parameters, Children... children) noexcept(
    (std::is_nothrow_move_constructible_v<Children> && ...))
    -> detail::parameter_group_node<Name, Parameters, Children...>
{
    return detail::parameter_group_node<Name, Parameters, Children...>{
        parameters,
        std::move(children)...,
    };
}

template<class T>
concept configuration_provider =
    requires(const T& value)
    {
        { value.configuration() };
        requires detail::named_configuration_node<
            decltype(value.configuration())>;
    };

namespace detail
{

template<class T>
[[nodiscard]]
constexpr auto configuration_nodes(const T& value)
{
    if constexpr (configuration_provider<T>)
    {
        return std::tuple{value.configuration()};
    }
    else
    {
        return std::tuple{};
    }
}

} // namespace detail

template<detail::named_configuration_node... Children>
    requires(sizeof...(Children) >= 1)
[[nodiscard]]
constexpr auto root(Children... children) noexcept(
    (std::is_nothrow_move_constructible_v<Children> && ...))
    -> detail::root_node<Children...>
{
    return detail::root_node<Children...>{std::move(children)...};
}

template<class... Children, class Function>
constexpr void for_each_config_parameter(
    const detail::root_node<Children...>& tree,
    Function&& function)
{
    auto&& visitor = function;
    detail::visit_parameters(tree, visitor);
}

} // namespace easylocal::config
