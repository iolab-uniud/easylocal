#pragma once

// Builder behind neighborhood<NHE>() | delta<C, D>().

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/helpers/detail/delta_cost_layer.hpp>
#include <easylocal/utils/detail/attributes.hpp>
#include <easylocal/utils/detail/meta.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal
{

// A NeighborhoodExplorer with parameters: its parameters_type is a parameter
// block, and it is constructed from the SolutionManager, the parameters and
// its other recipe arguments. Its recipe holds the parameters, gives them to
// the explorer when a runner is bound, and exposes them as configuration
// (under "neighborhood" in a runner or an app).
template<class NHE>
concept parameterized_neighborhood = requires {
    typename NHE::parameters_type;
} && config::parameter_block<typename NHE::parameters_type>;

} // namespace easylocal

namespace easylocal::detail
{

struct no_neighborhood_parameters
{
};

template<class NHE>
struct neighborhood_parameters_storage
{
    using type = no_neighborhood_parameters;
};

template<parameterized_neighborhood NHE>
struct neighborhood_parameters_storage<NHE>
{
    using type = typename NHE::parameters_type;
};

template<class NHE>
using neighborhood_parameters_storage_t =
    typename neighborhood_parameters_storage<NHE>::type;

// The arguments an explorer is constructed with after its SolutionManager:
// its parameters, if it has any, then the recipe arguments.
template<class NHE, class BaseArgsTuple>
struct neighborhood_construction_args
{
    using type = BaseArgsTuple;
};

template<parameterized_neighborhood NHE, class... Args>
struct neighborhood_construction_args<NHE, std::tuple<Args...>>
{
    using type = std::tuple<typename NHE::parameters_type, Args...>;
};

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

    static_assert(
        unique_types_v<typename DeltaSpecs::component_type...>,
        "a neighborhood recipe may attach at most one delta evaluator "
        "to each component type; the conflicting component type is shown in "
        "the template instantiation context");

    using parameters_storage_type = neighborhood_parameters_storage_t<BaseNHE>;

    explicit neighborhood_recipe(
        BaseArgsTuple base_args,
        parameters_storage_type parameters = {})
        requires(sizeof...(DeltaSpecs) == 0)
        : base_args_{std::move(base_args)}, parameters_{std::move(parameters)}
    {
        assert_valid_parameters();
    }

    neighborhood_recipe(
        BaseArgsTuple base_args,
        std::tuple<DeltaSpecs...> delta_specs,
        parameters_storage_type parameters = {})
        : base_args_{std::move(base_args)},
          delta_specs_{std::move(delta_specs)},
          parameters_{std::move(parameters)}
    {
        assert_valid_parameters();
    }

    // The explorer's parameters, to read or change.
    [[nodiscard]]
    parameters_storage_type& parameters() noexcept
        requires parameterized_neighborhood<BaseNHE>
    {
        return parameters_;
    }

    [[nodiscard]]
    const parameters_storage_type& parameters() const noexcept
        requires parameterized_neighborhood<BaseNHE>
    {
        return parameters_;
    }

    [[nodiscard]]
    config::validation_result configure(parameters_storage_type parameters)
        requires parameterized_neighborhood<BaseNHE>
    {
        const auto validation = parameters.validate();
        if (!validation)
            return validation;
        parameters_ = std::move(parameters);
        return config::validation_result::success();
    }

    // The explorer's parameters, at the root: a runner puts them under
    // "neighborhood".
    [[nodiscard]]
    config::parameter_set configuration()
        requires parameterized_neighborhood<BaseNHE>
    {
        config::parameter_set parameters;
        parameters.add(*this);
        return parameters;
    }

    [[nodiscard]]
    config::parameter_set configuration() const
        requires parameterized_neighborhood<BaseNHE>
    {
        config::parameter_set parameters;
        parameters.add(*this);
        return parameters;
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
                std::tuple<spec_type>{spec_type{std::forward<Args>(args)...}}),
            parameters_,
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
                std::tuple<spec_type>{spec_type{std::forward<Args>(args)...}}),
            std::move(parameters_),
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
            std::tuple_cat(delta_specs_, std::tuple<spec_type>{spec_type{}}),
            parameters_,
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
            std::tuple_cat(std::move(delta_specs_), std::tuple<spec_type>{spec_type{}}),
            std::move(parameters_),
        };
    }

    template<class Dependency>
    static constexpr bool constructible_from = base_neighborhood_constructible_v<
        BaseNHE,
        Dependency,
        typename neighborhood_construction_args<BaseNHE, BaseArgsTuple>::type>;

    template<class Dependency>
    [[nodiscard]]
    service_type construct(Dependency& dependency) const
    {
        const auto construction_args = [this] {
            const auto references = std::apply(
                [](const auto&... args) {
                    return std::tuple<const decltype(args)&...>{args...};
                },
                base_args_);
            if constexpr (parameterized_neighborhood<BaseNHE>)
                return std::tuple_cat(
                    std::tuple<const typename BaseNHE::parameters_type&>{parameters_},
                    references);
            else
                return references;
        }();
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
            construction_args);

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
    void assert_valid_parameters() const
    {
        if constexpr (parameterized_neighborhood<BaseNHE>)
            assert(
                parameters_.validate() && "the neighborhood's parameters must be valid");
    }

    BaseArgsTuple base_args_;
    std::tuple<DeltaSpecs...> delta_specs_;
    EASYLOCAL_NO_UNIQUE_ADDRESS parameters_storage_type parameters_{};
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
