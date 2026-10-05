#pragma once

// Builder behind neighborhood<NHE>() | delta<C, D>().

#include <easylocal/config/detail/parameterized.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/helpers/detail/delta_cost_layer.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/utils/detail/attributes.hpp>
#include <easylocal/utils/detail/meta.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

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

// The contract of a NeighborhoodExplorer on Solution, checked member by member
// so that a mistake is named where it is made: when its recipe is written, if
// the explorer declares its solution_type, and when it is bound otherwise.
template<class NHE, class Solution>
consteval bool check_explorer_contract()
{
    static_assert(
        requires { typename NHE::move_type; },
        "a NeighborhoodExplorer declares its Move: `using move_type = <its Move>;`");
    if constexpr (requires { typename NHE::move_type; })
    {
        using move_type = typename NHE::move_type;
        static_assert(
            requires(
                const NHE& explorer,
                const Solution& solution,
                const move_type& move) {
                { explorer.is_valid(solution, move) } -> std::convertible_to<bool>;
            },
            "a NeighborhoodExplorer has `bool is_valid(const Solution&, const Move&) "
            "const`");
        constexpr bool const_make_move =
            requires(const NHE& explorer, Solution& solution, const move_type& move) {
                explorer.make_move(solution, move);
            };
        constexpr bool mutable_make_move =
            requires(NHE& explorer, Solution& solution, const move_type& move) {
                explorer.make_move(solution, move);
            };
        static_assert(
            const_make_move || !mutable_make_move,
            "the make_move of a NeighborhoodExplorer is const: `void "
            "make_move(Solution&, const Move&) const`");
        static_assert(
            const_make_move || mutable_make_move,
            "a NeighborhoodExplorer has `void make_move(Solution&, const Move&) const`");
        if constexpr (const_make_move)
        {
            static_assert(
                !requires(
                    const NHE& explorer,
                    Solution&& solution,
                    const move_type& move) {
                    explorer.make_move(std::move(solution), move);
                },
                "make_move must change the Solution it is given: take it as `Solution&`, "
                "`void make_move(Solution& solution, const Move&) const`; taken by "
                "value or by const reference, it changes a copy");
        }
    }
    return true;
}

// The same, when the explorer declares the Solution it explores.
template<class NHE>
consteval bool check_declared_explorer_contract()
{
    if constexpr (requires { typename NHE::solution_type; })
        return check_explorer_contract<NHE, typename NHE::solution_type>();
    else
        return true;
}

// The explorer, built from the SolutionManager it is bound to (or from its
// base, the user's own, under the cost layer) and its construction arguments.
template<class NHE, class Dependency, class... Args>
consteval bool constructible_from_base()
{
    if constexpr (requires(Dependency& dependency) { dependency.base(); })
    {
        return std::constructible_from<
            NHE,
            decltype(std::declval<Dependency&>().base()),
            const Args&...>;
    }
    else
        return false;
}

template<class NHE, class Dependency, class... Args>
[[nodiscard]]
NHE construct_explorer(Dependency& dependency, const Args&... args)
{
    if constexpr (constructible_from_base<NHE, Dependency, Args...>())
    {
        return NHE(dependency.base(), args...);
    }
    else
    {
        constexpr bool derived_without_constructors = sizeof...(Args) == 0
            && requires { typename NHE::solution_manager_type; }
            && std::derived_from<
                NHE,
                neighborhood_explorer_base<
                    typename NHE::solution_manager_type,
                    typename NHE::move_type>>;
        static_assert(
            std::constructible_from<NHE, Dependency&, const Args&...>
                || !derived_without_constructors,
            "a NeighborhoodExplorer derived from neighborhood_explorer_base "
            "must inherit the base constructors; did you forget "
            "`using neighborhood_explorer_base::neighborhood_explorer_base;`?");
        static_assert(
            std::constructible_from<NHE, Dependency&, const Args&...>
                || derived_without_constructors,
            "a NeighborhoodExplorer must be constructible from the configured "
            "SolutionManager followed by its recipe arguments (its parameters "
            "first, when it has a parameters_type)");
        return NHE(dependency, args...);
    }
}

template<class BaseNHE, class BaseArgsTuple, class... DeltaSpecs>
class neighborhood_recipe
{
public:
    using base_type = BaseNHE;
    using service_type = neighborhood_service_t<BaseNHE, DeltaSpecs...>;

    static_assert(check_declared_explorer_contract<BaseNHE>());

    // A recipe attaches at most one delta cost component to each component,
    // to an explorer the delta layer can derive from.
    template<class Component>
    static consteval bool check_new_delta()
    {
        static_assert(
            !std::is_final_v<BaseNHE>,
            "a NeighborhoodExplorer that takes a delta cost component cannot be "
            "final: its delta layer derives from it");
        static_assert(
            !type_in_pack_v<Component, typename DeltaSpecs::component_type...>,
            "a neighborhood recipe may attach at most one delta cost component "
            "to each component type; the conflicting component type is shown in "
            "the template instantiation context");
        return true;
    }

    using parameters_holder_type = config::detail::parameters_holder<BaseNHE>;
    using parameters_storage_type = typename parameters_holder_type::parameters_type;

    // Throws std::invalid_argument when the explorer's parameters are not
    // valid.
    explicit neighborhood_recipe(
        BaseArgsTuple base_args,
        parameters_storage_type parameters = {})
        requires(sizeof...(DeltaSpecs) == 0)
        : base_args_{std::move(base_args)}, parameters_{std::move(parameters)}
    {
    }

    neighborhood_recipe(
        BaseArgsTuple base_args,
        std::tuple<DeltaSpecs...> delta_specs,
        parameters_storage_type parameters = {})
        : base_args_{std::move(base_args)},
          delta_specs_{std::move(delta_specs)},
          parameters_{std::move(parameters)}
    {
    }

    // The explorer's parameters, to read or change.
    template<class Self>
    [[nodiscard]]
    auto& parameters(this Self& self) noexcept
        requires config::detail::parameterized<BaseNHE>
    {
        return self.parameters_.parameters();
    }

    // The explorer's parameters, at the root: a runner puts them under
    // "neighborhood".
    [[nodiscard]]
    config::parameter_set configuration() &
        requires config::detail::parameterized<BaseNHE>
    {
        config::parameter_set parameters;
        parameters.add(parameters_);
        return parameters;
    }

    [[nodiscard]]
    config::parameter_set configuration() const&
        requires config::detail::parameterized<BaseNHE>
    {
        config::parameter_set parameters;
        parameters.add(parameters_);
        return parameters;
    }

    // A temporary has no configuration: the set would refer to it after it is
    // gone. Configure the object that will run.
    config::parameter_set configuration() const&& = delete;

    template<class Component, class DeltaEvaluator, class... Args>
    [[nodiscard]]
    auto with_delta(Args&&... args) const &
    {
        static_assert(check_new_delta<Component>());

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
            parameters_.parameters(),
        };
    }

    template<class Component, class DeltaEvaluator, class... Args>
    [[nodiscard]]
    auto with_delta(Args&&... args) &&
    {
        static_assert(check_new_delta<Component>());

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
            std::move(parameters_.parameters()),
        };
    }

    template<class Component>
    [[nodiscard]]
    auto with_delta() const &
    {
        static_assert(check_new_delta<Component>());

        using spec_type = colocated_delta_spec<Component>;
        using result_type = neighborhood_recipe<
            BaseNHE,
            BaseArgsTuple,
            DeltaSpecs...,
            spec_type>;

        return result_type{
            base_args_,
            std::tuple_cat(delta_specs_, std::tuple<spec_type>{spec_type{}}),
            parameters_.parameters(),
        };
    }

    template<class Component>
    [[nodiscard]]
    auto with_delta() &&
    {
        static_assert(check_new_delta<Component>());

        using spec_type = colocated_delta_spec<Component>;
        using result_type = neighborhood_recipe<
            BaseNHE,
            BaseArgsTuple,
            DeltaSpecs...,
            spec_type>;

        return result_type{
            std::move(base_args_),
            std::tuple_cat(std::move(delta_specs_), std::tuple<spec_type>{spec_type{}}),
            std::move(parameters_.parameters()),
        };
    }

    template<class Dependency>
    static constexpr bool constructible_from = base_neighborhood_constructible_v<
        BaseNHE,
        Dependency,
        config::detail::construction_arguments_t<BaseNHE, BaseArgsTuple>>;

    template<class Dependency>
    [[nodiscard]]
    service_type construct(Dependency& dependency) const
    {
        static_assert(check_explorer_contract<
            BaseNHE,
            typename std::remove_cvref_t<Dependency>::solution_type>());
        auto base = std::apply(
            [&](const auto&... args) {
                return construct_explorer<BaseNHE>(dependency, args...);
            },
            parameters_.arguments(base_args_));

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
    EASYLOCAL_NO_UNIQUE_ADDRESS parameters_holder_type parameters_;
};

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
