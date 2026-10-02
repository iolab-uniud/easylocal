#pragma once

#include <easylocal/config/tree.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

// Cost expressions given meaning for a Solution type: each node knows its cost
// type and the cost components (leaves) below it. The leaves are flattened in
// depth-first order into the component tuple of the cost layer; a node
// evaluates its cost from that flat tuple of component values, starting at its
// own offset. Deltas therefore stay per component and never see the structure.
namespace easylocal::detail
{

template<class Component, class... StoredArgs>
class component_spec
{
public:
    using component_type = Component;

    explicit component_spec(StoredArgs... args)
        : args_{std::move(args)...}
    {
    }

    template<class Instance>
    [[nodiscard]]
    auto construct(const Instance& instance) const -> Component
    {
        return std::apply(
            [&](const auto&... args) -> Component {
                if constexpr (std::constructible_from<
                                  Component,
                                  const Instance&,
                                  const StoredArgs&...>)
                {
                    return Component{instance, args...};
                }
                else
                {
                    static_assert(
                        std::constructible_from<Component, const StoredArgs&...>,
                        "a cost component must be constructible either from the "
                        "bound Instance followed by its recipe arguments or from "
                        "its recipe arguments alone");
                    return Component{args...};
                }
            },
            args_);
    }

private:
    std::tuple<StoredArgs...> args_;
};

// component<C>() * w and w * component<C>() are cost::weighted(component, w).
template<class Component, class... StoredArgs, cost::arithmetic Weight>
[[nodiscard]]
auto operator*(component_spec<Component, StoredArgs...> spec, Weight weight)
    -> cost::weighted_term<component_spec<Component, StoredArgs...>, Weight>
{
    return cost::weighted(std::move(spec), weight);
}

template<cost::arithmetic Weight, class Component, class... StoredArgs>
[[nodiscard]]
auto operator*(Weight weight, component_spec<Component, StoredArgs...> spec)
    -> cost::weighted_term<component_spec<Component, StoredArgs...>, Weight>
{
    return cost::weighted(std::move(spec), weight);
}

template<class T>
struct is_component_spec : std::false_type
{
};

template<class Component, class... StoredArgs>
struct is_component_spec<component_spec<Component, StoredArgs...>>
    : std::true_type
{
};

template<class T>
inline constexpr bool is_cost_expression_v =
    is_component_spec<std::remove_cvref_t<T>>::value ||
    cost::is_expression_v<T>;

template<class Component, class Solution>
using component_value_t = std::remove_cvref_t<decltype(
    std::declval<const Component&>().evaluate(
        std::declval<const Solution&>()))>;

template<class... Tuples>
using tuple_cat_t = decltype(std::tuple_cat(std::declval<Tuples>()...));

// Configuration names of positional children: "0", "1", ...
[[nodiscard]]
consteval auto decimal_digits(std::size_t value) -> std::size_t
{
    return value < 10 ? 1 : 1 + decimal_digits(value / 10);
}

template<std::size_t Index>
struct index_text
{
    char value[decimal_digits(Index) + 1]{};

    consteval index_text()
    {
        auto rest = Index;
        for (auto position = decimal_digits(Index); position > 0; --position)
        {
            value[position - 1] = static_cast<char>('0' + rest % 10);
            rest /= 10;
        }
    }
};

template<std::size_t Index>
inline constexpr index_text<Index> index_text_v{};

template<std::size_t Index>
inline constexpr config::fixed_string index_name{index_text_v<Index>.value};

template<config::fixed_string Name, class Node>
[[nodiscard]]
constexpr auto node_configuration(Node& node)
{
    if constexpr (std::remove_const_t<Node>::configurable)
    {
        return std::tuple{node.template configuration<Name>()};
    }
    else
    {
        return std::tuple{};
    }
}

template<class Children>
[[nodiscard]]
constexpr auto indexed_configurations(Children& children)
{
    return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
        return std::tuple_cat(
            node_configuration<index_name<Indices>>(
                std::get<Indices>(children))...);
    }(std::make_index_sequence<
        std::tuple_size_v<std::remove_const_t<Children>>>{});
}

template<config::fixed_string Name, class Nodes>
[[nodiscard]]
constexpr auto configuration_group(Nodes nodes)
{
    return std::apply(
        [](auto... node) { return config::named<Name>(std::move(node)...); },
        std::move(nodes));
}

template<class Expression, class Solution>
class cost_node
{
    static_assert(
        is_cost_expression_v<Expression>,
        "a cost expression is built from component<C>(...) leaves and "
        "cost::sum, cost::in_order, cost::hard_soft and cost::apply nodes");
};

template<class Child, class Weight, class Solution>
class cost_node<cost::weighted_term<Child, Weight>, Solution>
{
    static_assert(
        !std::same_as<Child, Child>,
        "cost::weighted(child, weight) weighs a term of cost::sum and is only "
        "allowed directly inside it");
};

template<class... Children>
inline constexpr auto child_offsets = [] {
    std::array<std::size_t, sizeof...(Children)> offsets{};
    std::size_t offset = 0;
    std::size_t index = 0;
    ((offsets[index++] = offset, offset += Children::leaf_count), ...);
    return offsets;
}();

// Leaf: the value of a cost component is its cost.
template<class Component, class... StoredArgs, class Solution>
class cost_node<component_spec<Component, StoredArgs...>, Solution>
{
public:
    using spec_type = component_spec<Component, StoredArgs...>;
    using cost_type = component_value_t<Component, Solution>;
    using leaf_specs = std::tuple<spec_type>;

    static constexpr std::size_t leaf_count = 1;
    static constexpr bool configurable = false;

    explicit cost_node(spec_type spec)
        : spec_{std::move(spec)}
    {
    }

    [[nodiscard]]
    auto leaves() const -> leaf_specs
    {
        return leaf_specs{spec_};
    }

    template<std::size_t Offset, class Values>
    [[nodiscard]]
    auto evaluate(const Values& values) const -> cost_type
    {
        return std::get<Offset>(values);
    }

private:
    EASYLOCAL_NO_UNIQUE_ADDRESS spec_type spec_;
};

template<class Term>
struct sum_term
{
    using child_type = Term;
    static constexpr bool weighted = false;

    template<class Weight>
    [[nodiscard]]
    static constexpr auto weight(const Term&) -> Weight
    {
        return Weight{1};
    }

    [[nodiscard]]
    static auto child(Term term) -> Term
    {
        return term;
    }
};

template<class Child, class Weight>
struct sum_term<cost::weighted_term<Child, Weight>>
{
    using child_type = Child;
    using weight_type = Weight;
    static constexpr bool weighted = true;

    template<class Target>
    [[nodiscard]]
    static constexpr auto weight(
        const cost::weighted_term<Child, Weight>& term) -> Target
    {
        return static_cast<Target>(term.weight);
    }

    [[nodiscard]]
    static auto child(cost::weighted_term<Child, Weight> term) -> Child
    {
        return std::move(term.child);
    }
};

template<class Term, class ChildCost>
struct sum_term_weight
{
    using type = ChildCost;
};

template<class Child, class Weight, class ChildCost>
struct sum_term_weight<cost::weighted_term<Child, Weight>, ChildCost>
{
    using type = Weight;
};

template<class Weight, std::size_t Size>
struct sum_parameters
{
    std::array<Weight, Size> weights{};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"weights", &sum_parameters::weights>(
                "Weights of the terms of the sum"));
    }

    [[nodiscard]]
    constexpr auto validate() const noexcept -> config::validation_result
    {
        return config::validation_result::success();
    }
};

// Weighted sum: Σ wᵢ · costᵢ, computed in the common type of the costs and of
// the given weights.
template<class... Terms, class Solution>
class cost_node<cost::sum_expression<Terms...>, Solution>
{
    using children_type = std::tuple<
        cost_node<typename sum_term<Terms>::child_type, Solution>...>;

    template<class Term>
    using child_cost_t = typename cost_node<
        typename sum_term<Term>::child_type,
        Solution>::cost_type;

    static_assert(
        (cost::arithmetic<child_cost_t<Terms>> && ...),
        "cost::sum adds arithmetic costs; turn a domain value into a number "
        "with cost::apply(f, component<C>())");

public:
    using cost_type = std::common_type_t<
        child_cost_t<Terms>...,
        typename sum_term_weight<Terms, child_cost_t<Terms>>::type...>;
    using parameters_type = sum_parameters<cost_type, sizeof...(Terms)>;
    using leaf_specs = tuple_cat_t<
        typename cost_node<typename sum_term<Terms>::child_type, Solution>::
            leaf_specs...>;

    static constexpr std::size_t leaf_count =
        (cost_node<typename sum_term<Terms>::child_type, Solution>::leaf_count +
         ...);
    static constexpr bool configurable = true;

    explicit cost_node(cost::sum_expression<Terms...> expression)
        : parameters_{std::apply(
              [](const auto&... term) {
                  return parameters_type{
                      .weights = {
                          sum_term<std::remove_cvref_t<decltype(term)>>::
                              template weight<cost_type>(term)...,
                      },
                  };
              },
              expression.terms)},
          children_{std::apply(
              [](auto&... term) {
                  return children_type{make_child(std::move(term))...};
              },
              expression.terms)}
    {
    }

    [[nodiscard]]
    auto parameters() const noexcept -> const parameters_type&
    {
        return parameters_;
    }

    [[nodiscard]]
    auto leaves() const -> leaf_specs
    {
        return std::apply(
            [](const auto&... child) { return std::tuple_cat(child.leaves()...); },
            children_);
    }

    template<std::size_t Offset, class Values>
    [[nodiscard]]
    auto evaluate(const Values& values) const -> cost_type
    {
        return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
            return static_cast<cost_type>(
                (cost_type{} + ... +
                 static_cast<cost_type>(
                     parameters_.weights[Indices] *
                     static_cast<cost_type>(
                         std::get<Indices>(children_).template evaluate<
                             Offset + offsets[Indices]>(values)))));
        }(std::index_sequence_for<Terms...>{});
    }

    template<config::fixed_string Name>
    [[nodiscard]]
    auto configuration()
    {
        return configuration_of<Name>(*this);
    }

    template<config::fixed_string Name>
    [[nodiscard]]
    auto configuration() const
    {
        return configuration_of<Name>(*this);
    }

private:
    static constexpr auto offsets = child_offsets<
        cost_node<typename sum_term<Terms>::child_type, Solution>...>;

    template<class Term>
    [[nodiscard]]
    static auto make_child(Term term)
        -> cost_node<typename sum_term<Term>::child_type, Solution>
    {
        return cost_node<typename sum_term<Term>::child_type, Solution>{
            sum_term<Term>::child(std::move(term))};
    }

    template<config::fixed_string Name, class Self>
    [[nodiscard]]
    static auto configuration_of(Self& self)
    {
        return std::apply(
            [&](auto... child) {
                return config::named<Name>(self.parameters_, std::move(child)...);
            },
            indexed_configurations(self.children_));
    }

    parameters_type parameters_;
    EASYLOCAL_NO_UNIQUE_ADDRESS children_type children_;
};

// Positional children of in_order and apply.
template<class Solution, class... Children>
class cost_children
{
public:
    using children_type = std::tuple<cost_node<Children, Solution>...>;
    using leaf_specs = tuple_cat_t<
        typename cost_node<Children, Solution>::leaf_specs...>;

    static constexpr std::size_t leaf_count =
        (cost_node<Children, Solution>::leaf_count + ...);
    static constexpr bool configurable =
        (cost_node<Children, Solution>::configurable || ...);

    explicit cost_children(std::tuple<Children...> children)
        : children_{std::apply(
              [](auto&... child) {
                  return children_type{
                      cost_node<std::remove_cvref_t<decltype(child)>, Solution>{
                          std::move(child)}...,
                  };
              },
              children)}
    {
    }

    [[nodiscard]]
    auto leaves() const -> leaf_specs
    {
        return std::apply(
            [](const auto&... child) { return std::tuple_cat(child.leaves()...); },
            children_);
    }

    template<std::size_t Offset, class Function, class Values>
    [[nodiscard]]
    auto evaluate_with(const Function& function, const Values& values) const
        -> decltype(auto)
    {
        return [&]<std::size_t... Indices>(std::index_sequence<Indices...>)
            -> decltype(auto) {
            return function(
                std::get<Indices>(children_).template evaluate<
                    Offset + offsets[Indices]>(values)...);
        }(std::index_sequence_for<Children...>{});
    }

    [[nodiscard]]
    auto configurations()
    {
        return indexed_configurations(children_);
    }

    [[nodiscard]]
    auto configurations() const
    {
        return indexed_configurations(children_);
    }

private:
    static constexpr auto offsets =
        child_offsets<cost_node<Children, Solution>...>;

    EASYLOCAL_NO_UNIQUE_ADDRESS children_type children_;
};

// Lexicographic: the children's costs, compared in order.
template<class... Children, class Solution>
class cost_node<cost::in_order_expression<Children...>, Solution>
{
    using children_type = cost_children<Solution, Children...>;

public:
    using cost_type = cost::lexicographic<
        typename cost_node<Children, Solution>::cost_type...>;
    using leaf_specs = typename children_type::leaf_specs;

    static constexpr std::size_t leaf_count = children_type::leaf_count;
    static constexpr bool configurable = children_type::configurable;

    explicit cost_node(cost::in_order_expression<Children...> expression)
        : children_{std::move(expression.children)}
    {
    }

    [[nodiscard]]
    auto leaves() const -> leaf_specs
    {
        return children_.leaves();
    }

    template<std::size_t Offset, class Values>
    [[nodiscard]]
    auto evaluate(const Values& values) const -> cost_type
    {
        return children_.template evaluate_with<Offset>(
            [](auto... cost) { return cost_type{std::move(cost)...}; },
            values);
    }

    template<config::fixed_string Name>
    [[nodiscard]]
    auto configuration()
        requires configurable
    {
        return configuration_group<Name>(children_.configurations());
    }

    template<config::fixed_string Name>
    [[nodiscard]]
    auto configuration() const
        requires configurable
    {
        return configuration_group<Name>(children_.configurations());
    }

private:
    EASYLOCAL_NO_UNIQUE_ADDRESS children_type children_;
};

// Hierarchical: the hard branch has strict priority. Its components are the
// leading ones, so TwoStage can evaluate them alone.
template<class Hard, class Soft, class Solution>
class cost_node<cost::hard_soft_expression<Hard, Soft>, Solution>
{
    using hard_node = cost_node<Hard, Solution>;
    using soft_node = cost_node<Soft, Solution>;

public:
    using hard_cost_type = typename hard_node::cost_type;
    using cost_type = cost::hierarchical<
        hard_cost_type,
        typename soft_node::cost_type>;
    using leaf_specs = tuple_cat_t<
        typename hard_node::leaf_specs,
        typename soft_node::leaf_specs>;

    static constexpr std::size_t hard_leaf_count = hard_node::leaf_count;
    static constexpr std::size_t leaf_count =
        hard_node::leaf_count + soft_node::leaf_count;
    static constexpr bool configurable =
        hard_node::configurable || soft_node::configurable;

    explicit cost_node(cost::hard_soft_expression<Hard, Soft> expression)
        : hard_{std::move(expression.hard)},
          soft_{std::move(expression.soft)}
    {
    }

    [[nodiscard]]
    auto leaves() const -> leaf_specs
    {
        return std::tuple_cat(hard_.leaves(), soft_.leaves());
    }

    template<std::size_t Offset, class Values>
    [[nodiscard]]
    auto evaluate(const Values& values) const -> cost_type
    {
        return cost_type{
            hard_.template evaluate<Offset>(values),
            soft_.template evaluate<Offset + hard_leaf_count>(values),
        };
    }

    // The hard cost from the values of the hard components only.
    template<class HardValues>
    [[nodiscard]]
    auto hard_cost(const HardValues& values) const -> hard_cost_type
    {
        return hard_.template evaluate<0>(values);
    }

    template<config::fixed_string Name>
    [[nodiscard]]
    auto configuration()
        requires configurable
    {
        return configuration_of<Name>(*this);
    }

    template<config::fixed_string Name>
    [[nodiscard]]
    auto configuration() const
        requires configurable
    {
        return configuration_of<Name>(*this);
    }

private:
    template<config::fixed_string Name, class Self>
    [[nodiscard]]
    static auto configuration_of(Self& self)
    {
        return configuration_group<Name>(std::tuple_cat(
            node_configuration<"hard">(self.hard_),
            node_configuration<"soft">(self.soft_)));
    }

    EASYLOCAL_NO_UNIQUE_ADDRESS hard_node hard_;
    EASYLOCAL_NO_UNIQUE_ADDRESS soft_node soft_;
};

// User function over the children's costs. At the root, it may also define
// the cost semantics.
template<class Function, class... Children, class Solution>
class cost_node<cost::apply_expression<Function, Children...>, Solution>
{
    using children_type = cost_children<Solution, Children...>;

    static_assert(
        std::invocable<
            const Function&,
            const typename cost_node<Children, Solution>::cost_type&...>,
        "the function of cost::apply must be callable with the costs of its "
        "children, in order");

public:
    using cost_type = std::remove_cvref_t<std::invoke_result_t<
        const Function&,
        const typename cost_node<Children, Solution>::cost_type&...>>;
    using leaf_specs = typename children_type::leaf_specs;

    static constexpr std::size_t leaf_count = children_type::leaf_count;
    static constexpr bool configurable =
        config::configuration_provider<Function> ||
        children_type::configurable;

    explicit cost_node(cost::apply_expression<Function, Children...> expression)
        : function_{std::move(expression.function)},
          children_{std::move(expression.children)}
    {
    }

    [[nodiscard]]
    auto function() const noexcept -> const Function&
    {
        return function_;
    }

    [[nodiscard]]
    auto leaves() const -> leaf_specs
    {
        return children_.leaves();
    }

    template<std::size_t Offset, class Values>
    [[nodiscard]]
    auto evaluate(const Values& values) const -> cost_type
    {
        return children_.template evaluate_with<Offset>(
            [&](const auto&... cost) -> cost_type {
                return std::invoke(function_, cost...);
            },
            values);
    }

    [[nodiscard]]
    auto better(const cost_type& candidate, const cost_type& reference) const
        -> bool
        requires requires(const Function& function) {
            { function.better(candidate, reference) } -> std::convertible_to<bool>;
        }
    {
        return static_cast<bool>(function_.better(candidate, reference));
    }

    [[nodiscard]]
    auto equivalent(const cost_type& lhs, const cost_type& rhs) const -> bool
        requires requires(const Function& function) {
            { function.equivalent(lhs, rhs) } -> std::convertible_to<bool>;
        }
    {
        return static_cast<bool>(function_.equivalent(lhs, rhs));
    }

    [[nodiscard]]
    auto better_or_equivalent(
        const cost_type& candidate,
        const cost_type& reference) const -> bool
        requires requires(const Function& function) {
            {
                function.better_or_equivalent(candidate, reference)
            } -> std::convertible_to<bool>;
        }
    {
        return static_cast<bool>(
            function_.better_or_equivalent(candidate, reference));
    }

    template<config::fixed_string Name>
    [[nodiscard]]
    auto configuration()
        requires configurable
    {
        return configuration_of<Name>(*this);
    }

    template<config::fixed_string Name>
    [[nodiscard]]
    auto configuration() const
        requires configurable
    {
        return configuration_of<Name>(*this);
    }

private:
    template<config::fixed_string Name, class Self>
    [[nodiscard]]
    static auto configuration_of(Self& self)
    {
        return configuration_group<Name>(std::tuple_cat(
            config::configuration_nodes(self.function_),
            self.children_.configurations()));
    }

    EASYLOCAL_NO_UNIQUE_ADDRESS Function function_;
    EASYLOCAL_NO_UNIQUE_ADDRESS children_type children_;
};

} // namespace easylocal::detail
