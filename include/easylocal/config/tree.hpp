#pragma once

#include <easylocal/utils/detail/meta.hpp>
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

namespace detail
{

// The names of the nodes above a parameter block. They are expanded once,
// as values: clang-cl (Microsoft ABI) mis-substitutes a pack of fixed_strings
// of different sizes re-expanded into another template's argument list.
template<fixed_string... Nodes>
struct path_prefix
{
    static constexpr std::array<std::string_view, sizeof...(Nodes)> names{
        Nodes.view()...};
};

} // namespace detail

// The path of a parameter: the names of the nodes above it, then its own.
template<class Descriptor, class Prefix>
struct parameter_path
{
    static constexpr std::size_t size = Prefix::names.size() + 1;

    [[nodiscard]]
    static constexpr std::array<std::string_view, size> segments() noexcept
    {
        std::array<std::string_view, size> result{};
        for (std::size_t index = 0; index + 1 < size; ++index)
        {
            result[index] = Prefix::names[index];
        }
        result[size - 1] = Descriptor::name();
        return result;
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
consteval bool unique_node_names() noexcept
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
    using parameters_type = std::remove_const_t<Parameters>;

    explicit constexpr parameter_node(Parameters& parameters) noexcept
        : parameters_{std::addressof(parameters)}
    {
    }

    [[nodiscard]]
    static constexpr std::string_view name() noexcept
    {
        return Name.view();
    }

    [[nodiscard]]
    constexpr const Parameters& parameters() const noexcept
    {
        return *parameters_;
    }

    [[nodiscard]]
    constexpr validation_result configure(parameters_type parameters) const
        requires(!std::is_const_v<Parameters>)
    {
        const auto validation = parameters.validate();
        if (!validation)
        {
            return validation;
        }

        *parameters_ = std::move(parameters);
        return validation_result::success();
    }

private:
    Parameters* parameters_;
};

template<fixed_string Name, configurable_endpoint Endpoint>
class configurable_node
{
    static_assert(
        !Name.view().empty(),
        "configuration node name must not be empty");

public:
    using configuration_node_marker = named_configuration_node_marker;
    using parameters_type = configurable_parameters_t<Endpoint>;

    explicit constexpr configurable_node(Endpoint& endpoint) noexcept
        : endpoint_{std::addressof(endpoint)}
    {
    }

    [[nodiscard]]
    static constexpr std::string_view name() noexcept
    {
        return Name.view();
    }

    [[nodiscard]]
    constexpr const parameters_type& parameters() const noexcept
    {
        return endpoint_->parameters();
    }

    [[nodiscard]]
    constexpr validation_result configure(parameters_type parameters) const
    {
        return endpoint_->configure(std::move(parameters));
    }

private:
    Endpoint* endpoint_;
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
    static constexpr std::string_view name() noexcept
    {
        return Name.view();
    }

    [[nodiscard]]
    constexpr const std::tuple<Children...>& children() const noexcept
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
    using parameters_type = std::remove_const_t<Parameters>;

    explicit constexpr parameter_group_node(
        Parameters& parameters,
        Children... children) noexcept(
        (std::is_nothrow_move_constructible_v<Children> && ...))
        : parameters_{std::addressof(parameters)},
          children_{std::move(children)...}
    {
    }

    [[nodiscard]]
    static constexpr std::string_view name() noexcept
    {
        return Name.view();
    }

    [[nodiscard]]
    constexpr const Parameters& parameters() const noexcept
    {
        return *parameters_;
    }

    [[nodiscard]]
    constexpr validation_result configure(parameters_type parameters) const
        requires(!std::is_const_v<Parameters>)
    {
        const auto validation = parameters.validate();
        if (!validation)
        {
            return validation;
        }

        *parameters_ = std::move(parameters);
        return validation_result::success();
    }

    [[nodiscard]]
    constexpr const std::tuple<Children...>& children() const noexcept
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
    constexpr const std::tuple<Children...>& children() const noexcept
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
    using prefix_type = path_prefix<Prefix..., Name>;
    for_each_parameter(
        node.parameters(),
        [&function](const auto descriptor, const auto& value) {
            using descriptor_type = std::remove_cvref_t<decltype(descriptor)>;
            using path_type =
                config::parameter_path<descriptor_type, prefix_type>;
            std::invoke(function, path_type{}, descriptor, value);
        });
}

template<fixed_string... Prefix,
         fixed_string Name,
         configurable_endpoint Endpoint,
         class Function>
constexpr void visit_parameters(
    const configurable_node<Name, Endpoint>& node,
    Function& function)
{
    using prefix_type = path_prefix<Prefix..., Name>;
    for_each_parameter(
        node.parameters(),
        [&function](const auto descriptor, const auto& value) {
            using descriptor_type = std::remove_cvref_t<decltype(descriptor)>;
            using path_type =
                config::parameter_path<descriptor_type, prefix_type>;
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
    using prefix_type = path_prefix<Prefix..., Name>;
    for_each_parameter(
        node.parameters(),
        [&function](const auto descriptor, const auto& value) {
            using descriptor_type = std::remove_cvref_t<decltype(descriptor)>;
            using path_type =
                config::parameter_path<descriptor_type, prefix_type>;
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
constexpr detail::parameter_node<Name, Parameters> named(Parameters& parameters) noexcept
{
    return detail::parameter_node<Name, Parameters>{parameters};
}

template<fixed_string Name, configurable_endpoint Endpoint>
[[nodiscard]]
constexpr detail::configurable_node<Name, Endpoint> endpoint(Endpoint& value) noexcept
{
    return detail::configurable_node<Name, Endpoint>{value};
}

template<fixed_string Name, configurable_endpoint Endpoint>
[[nodiscard]]
constexpr auto endpoint(const Endpoint& value) noexcept
{
    return config::named<Name>(value.parameters());
}

template<fixed_string Name, detail::named_configuration_node... Children>
    requires(sizeof...(Children) >= 1)
[[nodiscard]]
constexpr detail::group_node<Name, Children...> named(Children... children) noexcept(
    (std::is_nothrow_move_constructible_v<Children> && ...))
{
    return detail::group_node<Name, Children...>{std::move(children)...};
}

template<
    fixed_string Name,
    parameter_block Parameters,
    detail::named_configuration_node... Children>
    requires(sizeof...(Children) >= 1)
[[nodiscard]]
constexpr detail::parameter_group_node<Name, Parameters, Children...>
named(Parameters& parameters, Children... children) noexcept(
    (std::is_nothrow_move_constructible_v<Children> && ...))
{
    return detail::parameter_group_node<Name, Parameters, Children...>{
        parameters,
        std::move(children)...,
    };
}

template<class T>
concept configuration_provider =
    requires(T& value)
    {
        { value.configuration() };
        requires detail::named_configuration_node<
            decltype(value.configuration())>;
    };

template<class T>
[[nodiscard]]
constexpr auto configuration_nodes(T& value)
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

namespace detail
{

template<fixed_string Name, std::size_t Index = 0, class Tuple>
[[nodiscard]]
constexpr decltype(auto) named_child(const Tuple& children)
{
    using tuple_type = std::remove_cvref_t<Tuple>;

    if constexpr (Index == std::tuple_size_v<tuple_type>)
    {
        static_assert(
            easylocal::detail::always_false_v<tuple_type>,
            "configuration path segment does not name a child node");
    }
    else
    {
        using child_type = std::tuple_element_t<Index, tuple_type>;

        if constexpr (child_type::name() == Name.view())
        {
            return (std::get<Index>(children));
        }
        else
        {
            return named_child<Name, Index + 1>(children);
        }
    }
}

template<fixed_string First, fixed_string... Rest, class Node>
[[nodiscard]]
constexpr decltype(auto) config_node_at(const Node& node)
{
    const auto& child = named_child<First>(node.children());

    if constexpr (sizeof...(Rest) == 0)
    {
        return (child);
    }
    else if constexpr (requires { child.children(); })
    {
        return config_node_at<Rest...>(child);
    }
    else
    {
        static_assert(
            easylocal::detail::always_false_v<std::remove_cvref_t<decltype(child)>>,
            "configuration path continues through a leaf node");
    }
}

} // namespace detail

template<detail::named_configuration_node... Children>
    requires(sizeof...(Children) >= 1)
[[nodiscard]]
constexpr detail::root_node<Children...> root(Children... children) noexcept(
    (std::is_nothrow_move_constructible_v<Children> && ...))
{
    return detail::root_node<Children...>{std::move(children)...};
}

template<fixed_string First, fixed_string... Rest, class... Children>
[[nodiscard]]
constexpr decltype(auto) at(
    const detail::root_node<Children...>& tree)
{
    return detail::config_node_at<First, Rest...>(tree);
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
