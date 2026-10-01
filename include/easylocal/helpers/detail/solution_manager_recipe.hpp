#pragma once

#include <easylocal/config/tree.hpp>
#include <easylocal/helpers/detail/cost_layer.hpp>
#include <easylocal/utils/detail/meta.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

// Builder behind solution_manager<SM>() | component<C>() | aggregator(A).
namespace easylocal::detail
{

template<class SMSpec, class AggregatorSpec>
class solution_manager_with_aggregator_recipe
{
public:
    using inner_spec_type = SMSpec;
    using aggregator_spec_type = AggregatorSpec;
    using aggregator_type = typename AggregatorSpec::aggregator_type;
    using service_type = cost_layer_with_aggregator<
        typename SMSpec::component_service_type,
        aggregator_type>;

    solution_manager_with_aggregator_recipe(
        SMSpec inner,
        AggregatorSpec aggregator)
        : inner_{std::move(inner)}, aggregator_{std::move(aggregator)}
    {
    }

    template<class Dependency>
    static constexpr bool constructible_from =
        SMSpec::template constructible_from<Dependency>;

    template<class Dependency>
    [[nodiscard]]
    auto construct(Dependency& dependency) const -> service_type
    {
        return service_type{
            inner_.construct_components(dependency),
            aggregator_.get()};
    }

    [[nodiscard]]
    auto configuration()
        requires config::configuration_provider<aggregator_type>
    {
        return aggregator_.get().configuration();
    }

    [[nodiscard]]
    auto configuration() const
        requires config::configuration_provider<const aggregator_type>
    {
        return aggregator_.get().configuration();
    }

private:
    SMSpec inner_;
    AggregatorSpec aggregator_;
};

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

template<class BaseSM, class BaseArgsTuple, class... ComponentSpecs>
class solution_manager_recipe
{
public:
    static_assert(
        base_solution_manager<BaseSM>,
        "a SolutionManager must expose input_type, solution_type, "
        "input() -> const input_type&, and is_valid(solution)");

    using base_type = BaseSM;
    using component_service_type = cost_layer<
        BaseSM,
        ComponentSpecs...>;
    using component_types = std::tuple<typename ComponentSpecs::component_type...>;
    using component_values_type = typename component_service_type::component_values_type;
    using implicit_aggregator_traits_type =
        implicit_aggregator_traits<component_values_type>;
    static constexpr bool has_implicit_aggregator =
        sizeof...(ComponentSpecs) > 0 &&
        implicit_aggregator_traits_type::available;
    using implicit_aggregator_type = std::conditional_t<
        has_implicit_aggregator,
        typename implicit_aggregator_traits_type::type,
        no_implicit_aggregator>;
    using service_type = std::conditional_t<
        sizeof...(ComponentSpecs) == 0,
        BaseSM,
        std::conditional_t<
            has_implicit_aggregator,
            cost_layer_with_aggregator<
                component_service_type,
                implicit_aggregator_type>,
            component_service_type>>;

    static_assert(
        unique_types_v<typename ComponentSpecs::component_type...>,
        "a SolutionManager recipe may contain each component type at most once; "
        "the conflicting component type is shown in the template instantiation "
        "context");

    explicit solution_manager_recipe(BaseArgsTuple base_args)
        requires (sizeof...(ComponentSpecs) == 0)
        : base_args_{std::move(base_args)},
          implicit_aggregator_{make_implicit_aggregator()}
    {
    }

    solution_manager_recipe(
        BaseArgsTuple base_args,
        std::tuple<ComponentSpecs...> component_specs)
        : base_args_{std::move(base_args)},
          component_specs_{std::move(component_specs)},
          implicit_aggregator_{make_implicit_aggregator()}
    {
    }

    template<class Component, class... Args>
    [[nodiscard]]
    auto with_component(Args&&... args) const &
    {
        static_assert(
            !type_in_pack_v<Component, typename ComponentSpecs::component_type...>,
            "a SolutionManager recipe may contain each component type at most once; "
            "the conflicting component type is shown in the template instantiation "
            "context");

        using spec_type = component_spec<Component, std::decay_t<Args>...>;
        using result_type = solution_manager_recipe<
            BaseSM,
            BaseArgsTuple,
            ComponentSpecs...,
            spec_type>;

        return result_type{
            base_args_,
            std::tuple_cat(
                component_specs_,
                std::tuple{spec_type{std::forward<Args>(args)...}}),
        };
    }

    template<class Component, class... Args>
    [[nodiscard]]
    auto with_component(Args&&... args) &&
    {
        static_assert(
            !type_in_pack_v<Component, typename ComponentSpecs::component_type...>,
            "a SolutionManager recipe may contain each component type at most once; "
            "the conflicting component type is shown in the template instantiation "
            "context");

        using spec_type = component_spec<Component, std::decay_t<Args>...>;
        using result_type = solution_manager_recipe<
            BaseSM,
            BaseArgsTuple,
            ComponentSpecs...,
            spec_type>;

        return result_type{
            std::move(base_args_),
            std::tuple_cat(
                std::move(component_specs_),
                std::tuple{spec_type{std::forward<Args>(args)...}}),
        };
    }

    template<class Aggregator>
    [[nodiscard]]
    auto with_aggregator(Aggregator aggregator) const &
    {
        using spec_type = aggregator_spec<std::remove_cvref_t<Aggregator>>;
        return solution_manager_with_aggregator_recipe<
            solution_manager_recipe,
            spec_type>{
                *this,
                spec_type{std::move(aggregator)},
            };
    }

    template<class Aggregator>
    [[nodiscard]]
    auto with_aggregator(Aggregator aggregator) &&
    {
        using spec_type = aggregator_spec<std::remove_cvref_t<Aggregator>>;
        return solution_manager_with_aggregator_recipe<
            solution_manager_recipe,
            spec_type>{
                std::move(*this),
                spec_type{std::move(aggregator)},
            };
    }

    [[nodiscard]]
    auto configuration()
        requires (
            has_implicit_aggregator &&
            config::configuration_provider<implicit_aggregator_type>)
    {
        return implicit_aggregator_.configuration();
    }

    [[nodiscard]]
    auto configuration() const
        requires (
            has_implicit_aggregator &&
            config::configuration_provider<const implicit_aggregator_type>)
    {
        return implicit_aggregator_.configuration();
    }

    template<class Dependency>
    static constexpr bool constructible_from =
        std::same_as<
            std::remove_cvref_t<Dependency>,
            typename BaseSM::input_type>;

    [[nodiscard]]
    auto construct_components(const typename BaseSM::input_type& instance) const
        -> component_service_type
    {
        static_assert(
            sizeof...(ComponentSpecs) > 0,
            "an explicit aggregator requires at least one cost component");
        static_assert(
            base_solution_manager_constructible_v<
                BaseSM,
                typename BaseSM::input_type,
                BaseArgsTuple>,
            "a SolutionManager derived from solution_manager_base must inherit "
            "the base constructors; did you forget `using solution_manager_base::solution_manager_base;`?");

        auto base = std::apply(
            [&](const auto&... args) {
                return BaseSM{instance, args...};
            },
            base_args_);

        return std::apply(
            [&](const auto&... specs) {
                return component_service_type{std::move(base), specs...};
            },
            component_specs_);
    }

    [[nodiscard]]
    auto construct(const typename BaseSM::input_type& instance) const
        -> service_type
    {
        static_assert(
            base_solution_manager_constructible_v<
                BaseSM,
                typename BaseSM::input_type,
                BaseArgsTuple>,
            "a SolutionManager derived from solution_manager_base must inherit "
            "the base constructors; did you forget `using solution_manager_base::solution_manager_base;`?");

        auto base = std::apply(
            [&](const auto&... args) {
                return BaseSM{instance, args...};
            },
            base_args_);

        if constexpr (sizeof...(ComponentSpecs) == 0)
        {
            return base;
        }
        else
        {
            auto components = std::apply(
                [&](const auto&... specs) {
                    return component_service_type{std::move(base), specs...};
                },
                component_specs_);

            if constexpr (has_implicit_aggregator)
            {
                if constexpr (implicit_aggregator_traits_type::warns)
                {
                    warn_implicit_aggregator<implicit_aggregator_type>();
                }
                return service_type{
                    std::move(components),
                    implicit_aggregator_};
            }
            else
            {
                return components;
            }
        }
    }

private:
    [[nodiscard]]
    static constexpr auto make_implicit_aggregator()
        -> implicit_aggregator_type
    {
        if constexpr (has_implicit_aggregator)
        {
            return implicit_aggregator_traits_type::make();
        }
        else
        {
            return {};
        }
    }

    BaseArgsTuple base_args_;
    std::tuple<ComponentSpecs...> component_specs_;
    [[no_unique_address]] implicit_aggregator_type implicit_aggregator_;
};

template<class... ComponentSpecs>
inline constexpr bool unique_component_specs_v = unique_types_v<
    typename ComponentSpecs::component_type...>;

template<
    class BaseSM,
    class BaseArgsTuple,
    class... ComponentSpecs,
    class Component,
    class... Args>
[[nodiscard]]
auto operator|(
    solution_manager_recipe<
        BaseSM,
        BaseArgsTuple,
        ComponentSpecs...> recipe,
    component_spec<Component, Args...> spec)
{
    return std::apply(
        [&]<class... StoredArgs>(StoredArgs&&... args) {
            return std::move(recipe).template with_component<Component>(
                std::forward<StoredArgs>(args)...);
        },
        std::move(spec).args());
}

template<
    class BaseSM,
    class BaseArgsTuple,
    class... ComponentSpecs,
    class Aggregator>
[[nodiscard]]
auto operator|(
    solution_manager_recipe<
        BaseSM,
        BaseArgsTuple,
        ComponentSpecs...> recipe,
    aggregator_spec<Aggregator> spec)
{
    return std::move(recipe).with_aggregator(std::move(spec).get());
}

} // namespace easylocal::detail
