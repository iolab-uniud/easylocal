#pragma once

#include <easylocal/config/tree.hpp>
#include <easylocal/helpers/detail/delta_cost_layer.hpp>
#include <easylocal/utils/detail/meta.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

// Builder behind neighborhood<NHE>() | delta<C, D>().
namespace easylocal::detail
{

template<class BaseNHE, class Dependency, class Tuple>
struct base_neighborhood_constructible;

template<class BaseNHE, class Dependency, class... Args>
struct base_neighborhood_constructible<
    BaseNHE,
    Dependency,
    std::tuple<Args...>>
    : std::bool_constant<
          base_neighborhood_constructible_from_args<
              BaseNHE,
              Dependency,
              Args...>()>
{
};

template<class BaseNHE, class Dependency, class Tuple>
inline constexpr bool base_neighborhood_constructible_v =
    base_neighborhood_constructible<BaseNHE, Dependency, Tuple>::value;

template<class BaseNHE, class BaseArgsTuple, class... DeltaSpecs>
class neighborhood_recipe
{
public:
    using base_type = BaseNHE;
    using service_type = neighborhood_service_t<BaseNHE, DeltaSpecs...>;
    using delta_component_types = std::tuple<typename DeltaSpecs::component_type...>;

    static_assert(
        unique_types_v<typename DeltaSpecs::component_type...>,
        "a neighborhood recipe may attach at most one delta evaluator "
        "to each component type; the conflicting component type is shown in "
        "the template instantiation context");

    explicit neighborhood_recipe(BaseArgsTuple base_args)
        requires (sizeof...(DeltaSpecs) == 0)
        : base_args_{std::move(base_args)}
    {
    }

    neighborhood_recipe(
        BaseArgsTuple base_args,
        std::tuple<DeltaSpecs...> delta_specs)
        : base_args_{std::move(base_args)},
          delta_specs_{std::move(delta_specs)}
    {
    }

    template<class Component, class DeltaEvaluator, class... Args>
    [[nodiscard]]
    auto with_delta(Args&&... args) const &
    {
        static_assert(
            !type_in_pack_v<Component, typename DeltaSpecs::component_type...>,
            "a neighborhood recipe may attach at most one delta evaluator "
            "to each component type; the conflicting component type is shown in "
            "the template instantiation context");

        using spec_type = delta_spec<
            Component,
            DeltaEvaluator,
            std::decay_t<Args>...>;
        using result_type = neighborhood_recipe<
            BaseNHE,
            BaseArgsTuple,
            DeltaSpecs...,
            spec_type>;

        return result_type{
            base_args_,
            std::tuple_cat(
                delta_specs_,
                std::tuple{spec_type{std::forward<Args>(args)...}}),
        };
    }

    template<class Component, class DeltaEvaluator, class... Args>
    [[nodiscard]]
    auto with_delta(Args&&... args) &&
    {
        static_assert(
            !type_in_pack_v<Component, typename DeltaSpecs::component_type...>,
            "a neighborhood recipe may attach at most one delta evaluator "
            "to each component type; the conflicting component type is shown in "
            "the template instantiation context");

        using spec_type = delta_spec<
            Component,
            DeltaEvaluator,
            std::decay_t<Args>...>;
        using result_type = neighborhood_recipe<
            BaseNHE,
            BaseArgsTuple,
            DeltaSpecs...,
            spec_type>;

        return result_type{
            std::move(base_args_),
            std::tuple_cat(
                std::move(delta_specs_),
                std::tuple{spec_type{std::forward<Args>(args)...}}),
        };
    }

    template<class Component>
    [[nodiscard]]
    auto with_delta() const &
    {
        static_assert(
            !type_in_pack_v<Component, typename DeltaSpecs::component_type...>,
            "a neighborhood recipe may attach at most one delta evaluator "
            "to each component type; the conflicting component type is shown in "
            "the template instantiation context");

        using spec_type = colocated_delta_spec<Component>;
        using result_type = neighborhood_recipe<
            BaseNHE,
            BaseArgsTuple,
            DeltaSpecs...,
            spec_type>;

        return result_type{
            base_args_,
            std::tuple_cat(delta_specs_, std::tuple{spec_type{}}),
        };
    }

    template<class Component>
    [[nodiscard]]
    auto with_delta() &&
    {
        static_assert(
            !type_in_pack_v<Component, typename DeltaSpecs::component_type...>,
            "a neighborhood recipe may attach at most one delta evaluator "
            "to each component type; the conflicting component type is shown in "
            "the template instantiation context");

        using spec_type = colocated_delta_spec<Component>;
        using result_type = neighborhood_recipe<
            BaseNHE,
            BaseArgsTuple,
            DeltaSpecs...,
            spec_type>;

        return result_type{
            std::move(base_args_),
            std::tuple_cat(std::move(delta_specs_), std::tuple{spec_type{}}),
        };
    }

    template<class Dependency>
    static constexpr bool constructible_from =
        base_neighborhood_constructible_v<BaseNHE, Dependency, BaseArgsTuple>;

    template<class Dependency>
    [[nodiscard]]
    auto construct(Dependency& dependency) const -> service_type
    {
        auto base = std::apply(
            [&](const auto&... args) {
                if constexpr (requires { dependency.base(); })
                {
                    if constexpr (std::constructible_from<
                                      BaseNHE,
                                      decltype(dependency.base()),
                                      const decltype(args)&...>)
                    {
                        return BaseNHE{dependency.base(), args...};
                    }
                    else
                    {
                        if constexpr (sizeof...(args) == 0)
                        {
                            if constexpr (requires { typename BaseNHE::solution_manager_type; })
                            {
                                if constexpr (std::same_as<
                                    std::remove_cvref_t<decltype(dependency.base())>,
                                    typename BaseNHE::solution_manager_type>)
                                {
                                    static_assert(
                                        std::constructible_from<
                                            BaseNHE,
                                            decltype(dependency.base())>,
                                        "a NeighborhoodExplorer derived from neighborhood_explorer_base "
                                        "must inherit the base constructors; did you forget "
                                        "`using neighborhood_explorer_base::neighborhood_explorer_base;`?");
                                }
                                else
                                {
                                    static_assert(
                                        std::constructible_from<
                                            BaseNHE,
                                            Dependency&,
                                            const decltype(args)&...>,
                                        "a NeighborhoodExplorer must be constructible from "
                                        "the configured SolutionManager (or its base) followed "
                                        "by its recipe arguments");
                                }
                            }
                            else
                            {
                                static_assert(
                                    std::constructible_from<
                                        BaseNHE,
                                        Dependency&,
                                        const decltype(args)&...>,
                                    "a NeighborhoodExplorer must be constructible from "
                                    "the configured SolutionManager (or its base) followed "
                                    "by its recipe arguments");
                            }
                        }
                        else
                        {
                            static_assert(
                                std::constructible_from<
                                    BaseNHE,
                                    Dependency&,
                                    const decltype(args)&...>,
                                "a NeighborhoodExplorer must be constructible from "
                                "the configured SolutionManager (or its base) followed "
                                "by its recipe arguments");
                        }
                        return BaseNHE{dependency, args...};
                    }
                }
                else
                {
                    if constexpr (sizeof...(args) == 0)
                    {
                        if constexpr (requires { typename BaseNHE::solution_manager_type; })
                        {
                            if constexpr (std::same_as<
                                std::remove_cvref_t<Dependency>,
                                typename BaseNHE::solution_manager_type>)
                            {
                                static_assert(
                                    std::constructible_from<BaseNHE, Dependency&>,
                                    "a NeighborhoodExplorer derived from neighborhood_explorer_base "
                                    "must inherit the base constructors; did you forget "
                                    "`using neighborhood_explorer_base::neighborhood_explorer_base;`?");
                            }
                            else
                            {
                                static_assert(
                                    std::constructible_from<
                                        BaseNHE,
                                        Dependency&,
                                        const decltype(args)&...>,
                                    "a NeighborhoodExplorer must be constructible from "
                                    "the configured SolutionManager followed by its recipe "
                                    "arguments");
                            }
                        }
                        else
                        {
                            static_assert(
                                std::constructible_from<
                                    BaseNHE,
                                    Dependency&,
                                    const decltype(args)&...>,
                                "a NeighborhoodExplorer must be constructible from "
                                "the configured SolutionManager followed by its recipe "
                                "arguments");
                        }
                    }
                    else
                    {
                        static_assert(
                            std::constructible_from<
                                BaseNHE,
                                Dependency&,
                                const decltype(args)&...>,
                            "a NeighborhoodExplorer must be constructible from "
                            "the configured SolutionManager followed by its recipe "
                            "arguments");
                    }
                    return BaseNHE{dependency, args...};
                }
            },
            base_args_);

        if constexpr (sizeof...(DeltaSpecs) == 0)
        {
            return base;
        }
        else
        {
            return std::apply(
                [&](const auto&... specs) {
                    return service_type{
                        std::move(base),
                        specs.construct(dependency)...,
                    };
                },
                delta_specs_);
        }
    }

private:
    BaseArgsTuple base_args_;
    std::tuple<DeltaSpecs...> delta_specs_;
};

template<class... DeltaSpecs>
inline constexpr bool unique_delta_component_specs_v = unique_types_v<
    typename DeltaSpecs::component_type...>;

template<
    class BaseNHE,
    class BaseArgsTuple,
    class... DeltaSpecs,
    class Component,
    class DeltaEvaluator,
    class... Args>
[[nodiscard]]
auto operator|(
    neighborhood_recipe<
        BaseNHE,
        BaseArgsTuple,
        DeltaSpecs...> recipe,
    delta_spec<Component, DeltaEvaluator, Args...> spec)
{
    return std::apply(
        [&]<class... StoredArgs>(StoredArgs&&... args) {
            return std::move(recipe)
                .template with_delta<Component, DeltaEvaluator>(
                    std::forward<StoredArgs>(args)...);
        },
        std::move(spec).args());
}

template<
    class BaseNHE,
    class BaseArgsTuple,
    class... DeltaSpecs,
    class Component>
[[nodiscard]]
auto operator|(
    neighborhood_recipe<
        BaseNHE,
        BaseArgsTuple,
        DeltaSpecs...> recipe,
    colocated_delta_spec<Component>)
{
    return std::move(recipe).template with_delta<Component>();
}

} // namespace easylocal::detail
