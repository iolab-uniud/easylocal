#pragma once

// Builder behind solution_manager<SM>() | <cost expression>: the user
// SolutionManager with its constructor arguments, and the one cost expression
// whose leaves are the cost components.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/helpers/detail/cost_expression.hpp>
#include <easylocal/helpers/detail/cost_layer.hpp>
#include <easylocal/utils/detail/meta.hpp>

#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::detail
{

template<class BaseSM, class Instance, class Tuple>
struct base_solution_manager_constructible;

template<class BaseSM, class Instance, class... Args>
struct base_solution_manager_constructible<BaseSM, Instance, std::tuple<Args...>>
    : std::bool_constant<
          std::constructible_from<BaseSM, const Instance&, const Args&...>>
{
};

template<class BaseSM, class Instance, class Tuple>
inline constexpr bool base_solution_manager_constructible_v =
    base_solution_manager_constructible<BaseSM, Instance, Tuple>::value;

template<class BaseSM, class BaseArgsTuple>
[[nodiscard]]
BaseSM construct_base_solution_manager(
    const typename BaseSM::input_type& instance,
    const BaseArgsTuple& base_args)
{
    static_assert(
        base_solution_manager_constructible_v<
            BaseSM,
            typename BaseSM::input_type,
            BaseArgsTuple>,
        "a SolutionManager derived from solution_manager_base must inherit "
        "the base constructors; did you forget `using solution_manager_base::solution_manager_base;`?");

    return std::apply(
        [&](const auto&... args) { return BaseSM{instance, args...}; },
        base_args);
}

template<class BaseSM, class Leaves>
struct leaf_cost_layer;

template<class BaseSM, class... ComponentSpecs>
struct leaf_cost_layer<BaseSM, std::tuple<ComponentSpecs...>>
{
    using type = cost_layer<BaseSM, ComponentSpecs...>;
};

template<class BaseSM, class BaseArgsTuple, class Expression>
class solution_manager_with_cost_recipe
{
public:
    using base_type = BaseSM;
    using expression_type =
        cost_node<Expression, typename BaseSM::solution_type>;
    using component_service_type = typename leaf_cost_layer<
        BaseSM,
        typename expression_type::leaf_specs>::type;
    using component_types = typename component_service_type::component_types;
    using service_type =
        cost_layer_with_expression<component_service_type, expression_type>;

    solution_manager_with_cost_recipe(
        BaseArgsTuple base_args,
        Expression expression)
        : base_args_{std::move(base_args)},
          expression_{std::move(expression)}
    {
    }

    template<class Dependency>
    static constexpr bool constructible_from =
        std::same_as<
            std::remove_cvref_t<Dependency>,
            typename BaseSM::input_type>;

    [[nodiscard]]
    service_type construct(const typename BaseSM::input_type& instance) const
    {
        auto components = std::apply(
            [&](const auto&... specs) {
                return component_service_type{
                    construct_base_solution_manager<BaseSM>(instance, base_args_),
                    specs...,
                };
            },
            expression_.leaves());

        return service_type{std::move(components), expression_};
    }

    // The configurable parameters of the expression (the weights of its
    // sums, the parameters of its functions); a runner puts them under "cost".
    [[nodiscard]]
    config::parameter_set configuration()
        requires expression_type::configurable
    {
        return expression_.configuration();
    }

    [[nodiscard]]
    config::parameter_set configuration() const
        requires expression_type::configurable
    {
        return expression_.configuration();
    }

private:
    BaseArgsTuple base_args_;
    expression_type expression_;
};

template<class BaseSM, class BaseArgsTuple>
class solution_manager_recipe
{
public:
    static_assert(
        base_solution_manager<BaseSM>,
        "a SolutionManager must expose input_type, solution_type, "
        "input() -> const input_type&, and is_valid(solution)");

    using base_type = BaseSM;
    using service_type = BaseSM;

    explicit solution_manager_recipe(BaseArgsTuple base_args)
        : base_args_{std::move(base_args)}
    {
    }

    template<class Expression>
        requires is_cost_expression_v<Expression>
    [[nodiscard]]
    auto with_cost(Expression expression) const &
    {
        return solution_manager_with_cost_recipe<
            BaseSM,
            BaseArgsTuple,
            Expression>{base_args_, std::move(expression)};
    }

    template<class Expression>
        requires is_cost_expression_v<Expression>
    [[nodiscard]]
    auto with_cost(Expression expression) &&
    {
        return solution_manager_with_cost_recipe<
            BaseSM,
            BaseArgsTuple,
            Expression>{std::move(base_args_), std::move(expression)};
    }

    template<class Dependency>
    static constexpr bool constructible_from =
        std::same_as<
            std::remove_cvref_t<Dependency>,
            typename BaseSM::input_type>;

    [[nodiscard]]
    service_type construct(const typename BaseSM::input_type& instance) const
    {
        return construct_base_solution_manager<BaseSM>(instance, base_args_);
    }

private:
    BaseArgsTuple base_args_;
};

template<class BaseSM, class BaseArgsTuple, class Expression>
    requires is_cost_expression_v<Expression>
[[nodiscard]]
auto operator|(
    solution_manager_recipe<BaseSM, BaseArgsTuple> recipe,
    Expression expression)
{
    return std::move(recipe).with_cost(std::move(expression));
}

template<class BaseSM, class BaseArgsTuple, class CurrentExpression, class Expression>
    requires is_cost_expression_v<Expression>
auto operator|(
    solution_manager_with_cost_recipe<BaseSM, BaseArgsTuple, CurrentExpression>,
    Expression)
{
    static_assert(
        !std::same_as<Expression, Expression>,
        "a SolutionManager recipe has one cost expression: combine several "
        "components with cost::sum, cost::in_order, cost::objectives, "
        "cost::hard_soft or cost::apply");
}

} // namespace easylocal::detail
