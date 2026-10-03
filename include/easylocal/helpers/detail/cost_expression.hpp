#pragma once

// Cost expressions given meaning for a Solution type: each node knows its cost
// type and the cost components (leaves) below it. The leaves are flattened in
// depth-first order into the component tuple of the cost layer; a node
// evaluates its cost from that flat tuple of component values, starting at its
// own offset. Deltas therefore stay per component and never see the structure.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

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
    Component construct(const Instance& instance) const
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
cost::weighted_term<component_spec<Component, StoredArgs...>, Weight> operator*(
    component_spec<Component, StoredArgs...> spec,
    Weight weight)
{
    return cost::weighted(std::move(spec), weight);
}

template<cost::arithmetic Weight, class Component, class... StoredArgs>
[[nodiscard]]
cost::weighted_term<component_spec<Component, StoredArgs...>, Weight> operator*(
    Weight weight,
    component_spec<Component, StoredArgs...> spec)
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

// The parameters of a child node, under prefix, when it has any.
template<class Node>
void add_node_configuration(
    config::parameter_set& parameters,
    const std::string_view prefix,
    Node& node)
{
    if constexpr (std::remove_const_t<Node>::configurable)
    {
        parameters.add(prefix, node.configuration());
    }
}

// The parameters of positional children, under their positions ("0", "1").
template<class Children>
void add_indexed_configurations(config::parameter_set& parameters, Children& children)
{
    [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
        (add_node_configuration(
             parameters,
             std::to_string(Indices),
             std::get<Indices>(children)),
            ...);
    }(std::make_index_sequence<std::tuple_size_v<std::remove_const_t<Children>>>{});
}

template<class Expression, class Solution>
class cost_node
{
    static_assert(
        is_cost_expression_v<Expression>,
        "a cost expression is built from component<C>(...) leaves and "
        "cost::sum, cost::in_order, cost::objectives, cost::hard_soft and "
        "cost::apply nodes");
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
    leaf_specs leaves() const
    {
        return leaf_specs{spec_};
    }

    template<std::size_t Offset, class Values>
    [[nodiscard]]
    cost_type evaluate(const Values& values) const
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
    static constexpr Weight weight(const Term&)
    {
        return Weight{1};
    }

    [[nodiscard]]
    static Term child(Term term)
    {
        return term;
    }
};

template<class Child, class Weight>
struct sum_term<cost::weighted_term<Child, Weight>>
{
    using child_type = Child;
    static constexpr bool weighted = true;

    template<class Target>
    [[nodiscard]]
    static constexpr Target weight(const cost::weighted_term<Child, Weight>& term)
    {
        return static_cast<Target>(term.weight);
    }

    [[nodiscard]]
    static Child child(cost::weighted_term<Child, Weight> term)
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
    constexpr config::validation_result validate() const noexcept
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
    const parameters_type& parameters() const noexcept
    {
        return parameters_;
    }

    [[nodiscard]]
    leaf_specs leaves() const
    {
        return std::apply(
            [](const auto&... child) { return std::tuple_cat(child.leaves()...); },
            children_);
    }

    template<std::size_t Offset, class Values>
    [[nodiscard]]
    cost_type evaluate(const Values& values) const
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

    // The weights, and the parameters of the terms under their positions.
    [[nodiscard]]
    config::parameter_set configuration()
    {
        config::parameter_set parameters;
        parameters.add(parameters_);
        add_indexed_configurations(parameters, children_);
        return parameters;
    }

    [[nodiscard]]
    config::parameter_set configuration() const
    {
        config::parameter_set parameters;
        parameters.add(parameters_);
        add_indexed_configurations(parameters, children_);
        return parameters;
    }

private:
    static constexpr auto offsets = child_offsets<
        cost_node<typename sum_term<Terms>::child_type, Solution>...>;

    template<class Term>
    [[nodiscard]]
    static cost_node<typename sum_term<Term>::child_type, Solution> make_child(Term term)
    {
        return cost_node<typename sum_term<Term>::child_type, Solution>{
            sum_term<Term>::child(std::move(term))};
    }

    parameters_type parameters_;
    EASYLOCAL_NO_UNIQUE_ADDRESS children_type children_;
};

// Positional children of in_order, objectives and apply.
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
    leaf_specs leaves() const
    {
        return std::apply(
            [](const auto&... child) { return std::tuple_cat(child.leaves()...); },
            children_);
    }

    template<std::size_t Offset, class Function, class Values>
    [[nodiscard]]
    decltype(auto) evaluate_with(const Function& function, const Values& values) const
    {
        return [&]<std::size_t... Indices>(std::index_sequence<Indices...>)
            -> decltype(auto) {
            return function(
                std::get<Indices>(children_).template evaluate<
                    Offset + offsets[Indices]>(values)...);
        }(std::index_sequence_for<Children...>{});
    }

    void add_configurations(config::parameter_set& parameters)
    {
        add_indexed_configurations(parameters, children_);
    }

    void add_configurations(config::parameter_set& parameters) const
    {
        add_indexed_configurations(parameters, children_);
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
    leaf_specs leaves() const
    {
        return children_.leaves();
    }

    template<std::size_t Offset, class Values>
    [[nodiscard]]
    cost_type evaluate(const Values& values) const
    {
        return children_.template evaluate_with<Offset>(
            [](auto... cost) { return cost_type{std::move(cost)...}; },
            values);
    }

    // The parameters of the children, under their positions.
    [[nodiscard]]
    config::parameter_set configuration()
        requires configurable
    {
        config::parameter_set parameters;
        children_.add_configurations(parameters);
        return parameters;
    }

    [[nodiscard]]
    config::parameter_set configuration() const
        requires configurable
    {
        config::parameter_set parameters;
        children_.add_configurations(parameters);
        return parameters;
    }

private:
    EASYLOCAL_NO_UNIQUE_ADDRESS children_type children_;
};

// Pareto: the children's costs, compared by dominance.
template<class... Children, class Solution>
class cost_node<cost::objectives_expression<Children...>, Solution>
{
    using children_type = cost_children<Solution, Children...>;

public:
    using cost_type = cost::pareto<typename cost_node<Children, Solution>::cost_type...>;
    using leaf_specs = typename children_type::leaf_specs;

    static constexpr std::size_t leaf_count = children_type::leaf_count;
    static constexpr bool configurable = children_type::configurable;

    explicit cost_node(cost::objectives_expression<Children...> expression)
        : children_{std::move(expression.children)}
    {
    }

    [[nodiscard]]
    leaf_specs leaves() const
    {
        return children_.leaves();
    }

    template<std::size_t Offset, class Values>
    [[nodiscard]]
    cost_type evaluate(const Values& values) const
    {
        return children_.template evaluate_with<Offset>(
            [](auto... cost) { return cost_type{std::move(cost)...}; },
            values);
    }

    // The parameters of the children, under their positions.
    [[nodiscard]]
    config::parameter_set configuration()
        requires configurable
    {
        config::parameter_set parameters;
        children_.add_configurations(parameters);
        return parameters;
    }

    [[nodiscard]]
    config::parameter_set configuration() const
        requires configurable
    {
        config::parameter_set parameters;
        children_.add_configurations(parameters);
        return parameters;
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
    leaf_specs leaves() const
    {
        return std::tuple_cat(hard_.leaves(), soft_.leaves());
    }

    template<std::size_t Offset, class Values>
    [[nodiscard]]
    cost_type evaluate(const Values& values) const
    {
        return cost_type{
            hard_.template evaluate<Offset>(values),
            soft_.template evaluate<Offset + hard_leaf_count>(values),
        };
    }

    // The hard cost from the values of the hard components only.
    template<class HardValues>
    [[nodiscard]]
    hard_cost_type hard_cost(const HardValues& values) const
    {
        return hard_.template evaluate<0>(values);
    }

    // The parameters of the branches, under "hard" and "soft".
    [[nodiscard]]
    config::parameter_set configuration()
        requires configurable
    {
        config::parameter_set parameters;
        add_node_configuration(parameters, "hard", hard_);
        add_node_configuration(parameters, "soft", soft_);
        return parameters;
    }

    [[nodiscard]]
    config::parameter_set configuration() const
        requires configurable
    {
        config::parameter_set parameters;
        add_node_configuration(parameters, "hard", hard_);
        add_node_configuration(parameters, "soft", soft_);
        return parameters;
    }

private:
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
    const Function& function() const noexcept
    {
        return function_;
    }

    [[nodiscard]]
    leaf_specs leaves() const
    {
        return children_.leaves();
    }

    template<std::size_t Offset, class Values>
    [[nodiscard]]
    cost_type evaluate(const Values& values) const
    {
        return children_.template evaluate_with<Offset>(
            [&](const auto&... cost) -> cost_type {
                return std::invoke(function_, cost...);
            },
            values);
    }

    [[nodiscard]]
    bool better(const cost_type& candidate, const cost_type& reference) const
        requires requires(const Function& function) {
            { function.better(candidate, reference) } -> std::convertible_to<bool>;
        }
    {
        return static_cast<bool>(function_.better(candidate, reference));
    }

    [[nodiscard]]
    bool equivalent(const cost_type& lhs, const cost_type& rhs) const
        requires requires(const Function& function) {
            { function.equivalent(lhs, rhs) } -> std::convertible_to<bool>;
        }
    {
        return static_cast<bool>(function_.equivalent(lhs, rhs));
    }

    [[nodiscard]]
    bool better_or_equivalent(const cost_type& candidate, const cost_type& reference)
        const
        requires requires(const Function& function) {
            {
                function.better_or_equivalent(candidate, reference)
            } -> std::convertible_to<bool>;
        }
    {
        return static_cast<bool>(
            function_.better_or_equivalent(candidate, reference));
    }

    // The function's parameters, at the root, and the children's, under their
    // positions.
    [[nodiscard]]
    config::parameter_set configuration()
        requires configurable
    {
        config::parameter_set parameters;
        config::add_configuration(parameters, {}, function_);
        children_.add_configurations(parameters);
        return parameters;
    }

    [[nodiscard]]
    config::parameter_set configuration() const
        requires configurable
    {
        config::parameter_set parameters;
        config::add_configuration(parameters, {}, function_);
        children_.add_configurations(parameters);
        return parameters;
    }

private:
    EASYLOCAL_NO_UNIQUE_ADDRESS Function function_;
    EASYLOCAL_NO_UNIQUE_ADDRESS children_type children_;
};

} // namespace easylocal::detail
