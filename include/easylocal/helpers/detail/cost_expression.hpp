#pragma once

// Cost expressions given meaning for a Solution type: each node knows its cost
// type and the cost components (leaves) below it. The leaves are flattened in
// depth-first order into the component tuple of the cost layer; a node
// evaluates its cost from that flat tuple of component values, starting at its
// own offset. Deltas therefore stay per component and never see the structure.

#include <easylocal/config/detail/parameterized.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::detail
{

// A configurable component or cost::apply function is configured under its
// static name(), which every one must have.
template<class T>
concept statically_named = requires {
    { T::name() } -> std::convertible_to<std::string_view>;
};

template<class T, bool Configurable>
consteval bool check_configurable_name()
{
    static_assert(
        !Configurable || statically_named<T>,
        "a cost component or cost::apply function whose parameters_type is a "
        "parameter block is configured under cost.<name>: give it "
        "`static std::string_view name()`");
    return true;
}

template<class Component, class... StoredArgs>
class component_spec
{
    using holder_type = config::detail::parameters_holder<Component>;

public:
    using component_type = Component;
    using parameters_type = typename holder_type::parameters_type;

    explicit component_spec(StoredArgs... args)
        : args_{std::move(args)...}
    {
    }

    // Throws std::invalid_argument when the parameters are not valid.
    component_spec(parameters_type parameters, StoredArgs... args)
        requires config::detail::parameterized<Component>
        : args_{std::move(args)...}, parameters_{std::move(parameters)}
    {
    }

    // The component's parameters, to read or change.
    template<class Self>
    [[nodiscard]]
    auto& parameters(this Self& self) noexcept
        requires config::detail::parameterized<Component>
    {
        return self.parameters_;
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
                                  decltype(args)...>)
                {
                    return Component(instance, args...);
                }
                else
                {
                    static_assert(
                        std::constructible_from<Component, decltype(args)...>,
                        "a cost component must be constructible either from the "
                        "bound Instance followed by its recipe arguments or from "
                        "its recipe arguments alone (its parameters first, when "
                        "it has a parameters_type)");
                    return Component(args...);
                }
            },
            parameters_.arguments(args_));
    }

private:
    std::tuple<StoredArgs...> args_;
    EASYLOCAL_NO_UNIQUE_ADDRESS holder_type parameters_;
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

// The function of a cost::apply that defines the order of its costs: a
// compare(a, b) returning a std::partial_ordering.
template<class Function, class Cost>
concept apply_function_orders =
    requires(const Function& function, const Cost& lhs, const Cost& rhs) {
        { function.compare(lhs, rhs) } -> std::convertible_to<std::partial_ordering>;
    };

// The function of a cost::apply with one of the relations that compare(a, b)
// replaced, which the cost layer no longer reads.
template<class Function, class Cost>
concept apply_function_has_relations =
    requires(const Function& function, const Cost& cost) {
        function.better(cost, cost);
    } || requires(const Function& function, const Cost& cost) {
        function.equivalent(cost, cost);
    } || requires(const Function& function, const Cost& cost) {
        function.better_or_equivalent(cost, cost);
    };

// A node that defines the order of its cost (compare(a, b)): it gives the cost
// semantics of the whole expression, so it may only be the root.
template<class Node>
concept ordering_cost_node =
    requires(const Node& node, const typename Node::cost_type& cost) {
        node.compare(cost, cost);
    };

template<class... Nodes>
inline constexpr bool no_ordering_child_v = !(ordering_cost_node<Nodes> || ...);

template<class Component, class Solution>
using component_value_t = std::remove_cvref_t<decltype(
    std::declval<const Component&>().evaluate(
        std::declval<const Solution&>()))>;

template<class... Tuples>
using tuple_cat_t = decltype(std::tuple_cat(std::declval<Tuples>()...));

// The parameters of a node, when it has any: those of its structure (the
// weights of a sum, a tolerance) under its path, path, and those of its
// configurable components and functions at the root of parameters, under
// their names.
template<class Node>
void add_node_parameters(
    config::parameter_set& parameters,
    const std::string& path,
    Node& node)
{
    if constexpr (std::remove_const_t<Node>::configurable)
        node.add_parameters(parameters, path);
}

// The parameters of positional children, their structure under their
// positions ("0", "1") below path.
template<class Children>
void add_indexed_parameters(
    config::parameter_set& parameters,
    const std::string& path,
    Children& children)
{
    [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
        (add_node_parameters(
             parameters,
             config::detail::join_path(path, std::to_string(Indices)),
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
        "cost::sum, cost::in_order, cost::objectives, cost::hard_soft, "
        "cost::apply and cost::approximately nodes");
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

    static_assert(
        !cost::detail::unsigned_cost_v<cost_type>,
        "a cost component cannot return an unsigned integer, whose "
        "differences wrap around: return a signed integer (int, long long) "
        "or a floating-point value");

    static constexpr std::size_t leaf_count = 1;
    static constexpr bool configurable = config::detail::parameterized<Component>;

    static_assert(check_configurable_name<Component, configurable>());

    explicit cost_node(spec_type spec)
        : spec_{std::move(spec)}
    {
    }

    [[nodiscard]]
    leaf_specs leaves() const
    {
        return leaf_specs{spec_};
    }

    // The component's parameters, under its name.
    template<class Self>
    void add_parameters(
        this Self& self,
        config::parameter_set& parameters,
        const std::string&)
        requires configurable
    {
        parameters.add(Component::name(), self.spec_.parameters());
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
                "Weights of the terms of the sum",
                easylocal::unlimited));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        if (const auto schema = config::check_schema(*this); !schema)
            return schema;
        // A negative weight may be intended; NaN and infinity are not.
        if constexpr (std::floating_point<Weight>)
        {
            for (const auto weight : weights)
                if (!(weight >= std::numeric_limits<Weight>::lowest()
                        && weight <= std::numeric_limits<Weight>::max()))
                    return config::validation_result::failure("weights must be finite");
        }
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
    static_assert(
        no_ordering_child_v<cost_node<typename sum_term<Terms>::child_type, Solution>...>,
        "the cost semantics are defined at the root of the cost expression: a "
        "cost::apply whose function defines compare(a, b), or "
        "cost::approximately, cannot be the child of another node");

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

    // The weights, under path, and the parameters of the terms under their
    // positions.
    template<class Self>
    void add_parameters(
        this Self& self,
        config::parameter_set& parameters,
        const std::string& path)
    {
        parameters.add(path, self.parameters_);
        add_indexed_parameters(parameters, path, self.children_);
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
    static_assert(
        no_ordering_child_v<cost_node<Children, Solution>...>,
        "the cost semantics are defined at the root of the cost expression: a "
        "cost::apply whose function defines compare(a, b), or "
        "cost::approximately, cannot be the child of another node");

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

    template<class Self>
    void add_parameters(
        this Self& self,
        config::parameter_set& parameters,
        const std::string& path)
    {
        add_indexed_parameters(parameters, path, self.children_);
    }

private:
    static constexpr auto offsets =
        child_offsets<cost_node<Children, Solution>...>;

    EASYLOCAL_NO_UNIQUE_ADDRESS children_type children_;
};

// A node whose cost is Cost<the children's costs...>, built from them in order:
// the base of in_order and objectives.
template<template<class...> class Cost, class Solution, class... Children>
class positional_cost_node
{
    using children_type = cost_children<Solution, Children...>;

public:
    using cost_type = Cost<typename cost_node<Children, Solution>::cost_type...>;
    using leaf_specs = typename children_type::leaf_specs;

    static constexpr std::size_t leaf_count = children_type::leaf_count;
    static constexpr bool configurable = children_type::configurable;

    explicit positional_cost_node(std::tuple<Children...> children)
        : children_{std::move(children)}
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
    template<class Self>
    void add_parameters(
        this Self& self,
        config::parameter_set& parameters,
        const std::string& path)
        requires configurable
    {
        self.children_.add_parameters(parameters, path);
    }

private:
    EASYLOCAL_NO_UNIQUE_ADDRESS children_type children_;
};

// Lexicographic: the children's costs, compared in order.
template<class... Children, class Solution>
class cost_node<cost::in_order_expression<Children...>, Solution>
    : public positional_cost_node<cost::lexicographic, Solution, Children...>
{
public:
    explicit cost_node(cost::in_order_expression<Children...> expression)
        : positional_cost_node<cost::lexicographic, Solution, Children...>{
              std::move(expression.children)}
    {
    }
};

// Pareto: the children's costs, compared by dominance.
template<class... Children, class Solution>
class cost_node<cost::objectives_expression<Children...>, Solution>
    : public positional_cost_node<cost::pareto, Solution, Children...>
{
public:
    explicit cost_node(cost::objectives_expression<Children...> expression)
        : positional_cost_node<cost::pareto, Solution, Children...>{
              std::move(expression.children)}
    {
    }
};

// Hierarchical: the hard branch has strict priority. Its components are the
// leading ones, so a stage on the hard cost can evaluate them alone.
template<class Hard, class Soft, class Solution>
class cost_node<cost::hard_soft_expression<Hard, Soft>, Solution>
{
    using hard_node = cost_node<Hard, Solution>;
    using soft_node = cost_node<Soft, Solution>;

    static_assert(
        no_ordering_child_v<hard_node, soft_node>,
        "the cost semantics are defined at the root of the cost expression: a "
        "cost::apply whose function defines compare(a, b), or "
        "cost::approximately, cannot be the child of another node");

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
    template<class Self>
    void add_parameters(
        this Self& self,
        config::parameter_set& parameters,
        const std::string& path)
        requires configurable
    {
        add_node_parameters(
            parameters,
            config::detail::join_path(path, "hard"),
            self.hard_);
        add_node_parameters(
            parameters,
            config::detail::join_path(path, "soft"),
            self.soft_);
    }

private:
    EASYLOCAL_NO_UNIQUE_ADDRESS hard_node hard_;
    EASYLOCAL_NO_UNIQUE_ADDRESS soft_node soft_;
};

// The function of a cost::apply, as it is called: without parameters, as it
// was given.
template<class Function>
class apply_function
{
public:
    apply_function(Function function, config::detail::no_parameters)
        : function_{std::move(function)}
    {
    }

    [[nodiscard]]
    const Function& function() const noexcept
    {
        return function_;
    }

private:
    EASYLOCAL_NO_UNIQUE_ADDRESS Function function_;
};

// A function with parameters: a configurable endpoint, which builds the
// function again from the parameters it is given.
template<config::detail::parameterized Function>
class apply_function<Function>
{
public:
    using parameters_type = typename Function::parameters_type;

    apply_function(Function function, parameters_type parameters)
        : function_{std::move(function)}, parameters_{std::move(parameters)}
    {
    }

    [[nodiscard]]
    const Function& function() const noexcept
    {
        return function_;
    }

    [[nodiscard]]
    const parameters_type& parameters() const noexcept
    {
        return parameters_;
    }

    [[nodiscard]]
    config::validation_result configure(parameters_type parameters)
    {
        const auto validation = parameters.validate();
        if (!validation)
            return validation;
        function_ = Function{parameters};
        parameters_ = std::move(parameters);
        return config::validation_result::success();
    }

private:
    Function function_;
    parameters_type parameters_;
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

    static_assert(
        !cost::detail::unsigned_cost_v<cost_type>,
        "the function of cost::apply cannot return an unsigned integer, whose "
        "differences wrap around: return a signed integer (int, long long) "
        "or a floating-point value");
    static_assert(
        !apply_function_has_relations<Function, cost_type>,
        "the function of cost::apply defines the cost semantics with one "
        "compare(a, b) returning std::partial_ordering, not with better, "
        "equivalent or better_or_equivalent");
    static_assert(
        !requires(const Function& function, const cost_type& cost) {
            function.compare(cost, cost);
        } || apply_function_orders<Function, cost_type>,
        "compare(a, b) of the function of cost::apply must return "
        "std::partial_ordering (or a comparison category that converts to it)");

    static constexpr bool parameterized_function =
        config::detail::parameterized<Function>;
    static constexpr std::size_t leaf_count = children_type::leaf_count;
    static constexpr bool configurable =
        parameterized_function || children_type::configurable;

    static_assert(config::detail::check_declared_parameters<Function>());
    static_assert(check_configurable_name<Function, parameterized_function>());

    explicit cost_node(cost::apply_expression<Function, Children...> expression)
        : function_{std::move(expression.function), std::move(expression.parameters)},
          children_{std::move(expression.children)}
    {
    }

    [[nodiscard]]
    const Function& function() const noexcept
    {
        return function_.function();
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
                return std::invoke(function_.function(), cost...);
            },
            values);
    }

    // The order of the costs, when the function defines it: at the root, it
    // defines better, equivalent and better_or_equivalent.
    [[nodiscard]]
    std::partial_ordering compare(const cost_type& lhs, const cost_type& rhs) const
        requires apply_function_orders<Function, cost_type>
    {
        return static_cast<std::partial_ordering>(function_.function().compare(lhs, rhs));
    }

    // The function's parameters, under its name, and the children's, under
    // their positions.
    template<class Self>
    void add_parameters(
        this Self& self,
        config::parameter_set& parameters,
        const std::string& path)
        requires configurable
    {
        if constexpr (parameterized_function)
            parameters.add(Function::name(), self.function_);
        if constexpr (children_type::configurable)
            self.children_.add_parameters(parameters, path);
    }

private:
    EASYLOCAL_NO_UNIQUE_ADDRESS apply_function<Function> function_;
    EASYLOCAL_NO_UNIQUE_ADDRESS children_type children_;
};

// The order of costs within a tolerance, which cost::approximately gives to the
// hard projection of its cost.
class tolerance_order
{
public:
    explicit tolerance_order(const cost::tolerance within) noexcept : within_{within} {}

    template<class Cost>
    [[nodiscard]]
    std::partial_ordering compare(const Cost& lhs, const Cost& rhs) const
    {
        return cost::approximate_compare(lhs, rhs, within_);
    }

private:
    cost::tolerance within_;
};

// The number of hard components of Node, for a node that passes them through:
// none unless Node has a hard projection.
template<class Node>
struct hard_leaves_of
{
};

template<class Node>
    requires requires { Node::hard_leaf_count; }
struct hard_leaves_of<Node>
{
    static constexpr std::size_t hard_leaf_count = Node::hard_leaf_count;
};

// The cost of the child, ordered within a tolerance: the root, which defines
// the cost semantics, over a structure it leaves visible (the hard components
// of a hard_soft child).
template<class Child, class Solution>
class cost_node<cost::approximately_expression<Child>, Solution>
    : public hard_leaves_of<cost_node<Child, Solution>>
{
    using child_node = cost_node<Child, Solution>;

    static_assert(
        no_ordering_child_v<child_node>,
        "the cost semantics are defined at the root of the cost expression: a "
        "cost::apply whose function defines compare(a, b), or "
        "cost::approximately, cannot be the child of another node");

public:
    using cost_type = typename child_node::cost_type;
    using leaf_specs = typename child_node::leaf_specs;

    static_assert(
        cost::approximately_comparable<cost_type>,
        "cost::approximately compares numbers, lexicographic, hierarchical and "
        "pareto costs of them, or costs with <=> or <");

    static constexpr std::size_t leaf_count = child_node::leaf_count;
    static constexpr bool configurable = true;

    explicit cost_node(cost::approximately_expression<Child> expression)
        : child_{std::move(expression.child)}, within_{expression.within}
    {
    }

    [[nodiscard]]
    leaf_specs leaves() const
    {
        return child_.leaves();
    }

    template<std::size_t Offset, class Values>
    [[nodiscard]]
    cost_type evaluate(const Values& values) const
    {
        return child_.template evaluate<Offset>(values);
    }

    [[nodiscard]]
    std::partial_ordering compare(const cost_type& lhs, const cost_type& rhs) const
    {
        return cost::approximate_compare(lhs, rhs, within_);
    }

    // The tolerance.
    [[nodiscard]]
    const cost::tolerance& tolerance() const noexcept
    {
        return within_;
    }

    // The hard cost of a hard_soft child, from its hard components, which stay
    // the leading ones.
    template<class HardValues>
    [[nodiscard]]
    auto hard_cost(const HardValues& values) const
        requires requires(const child_node& child) { child.hard_cost(values); }
    {
        return child_.hard_cost(values);
    }

    // The order of the hard costs, for the hard projection of the cost: within
    // the same tolerance.
    [[nodiscard]]
    tolerance_order hard_semantics() const noexcept
        requires cost::hierarchical_type<cost_type>
    {
        return tolerance_order{within_};
    }

    // The tolerance, under "tolerance", and the parameters of the child, as
    // they are without it.
    template<class Self>
    void add_parameters(
        this Self& self,
        config::parameter_set& parameters,
        const std::string& path)
    {
        parameters.add(config::detail::join_path(path, "tolerance"), self.within_);
        add_node_parameters(parameters, path, self.child_);
    }

private:
    EASYLOCAL_NO_UNIQUE_ADDRESS child_node child_;
    cost::tolerance within_;
};

} // namespace easylocal::detail
