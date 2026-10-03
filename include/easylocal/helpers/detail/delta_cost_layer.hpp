#pragma once

#include <easylocal/config/tree.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/utils/detail/attributes.hpp>
#include <easylocal/utils/detail/meta.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

// NeighborhoodExplorer composition, the delta cost layer (symmetric to the
// SolutionManager cost layer): the user NeighborhoodExplorer plus the delta
// evaluator bound to each cost component (separate or co-located). Deltas are
// per component; the move cost is always recomputed by the cost expression.
namespace easylocal::detail
{

template<class Component, class DeltaEvaluator>
class delta_binding
{
public:
    using component_type = Component;
    using evaluator_type = DeltaEvaluator;

    explicit delta_binding(DeltaEvaluator evaluator)
        : evaluator_{std::move(evaluator)}
    {
    }

    [[nodiscard]]
    const DeltaEvaluator& evaluator() const noexcept
    {
        return evaluator_;
    }

    template<class Value, class Solution, class Move>
        requires requires(
            const Value& value,
            const DeltaEvaluator& evaluator,
            const Solution& solution,
            const Move& move) {
            {
                value + evaluator.delta_evaluate(solution, move)
            } -> std::same_as<Value>;
        }
    [[nodiscard]]
    Value apply(const Value& value, const Solution& solution, const Move& move) const
    {
        return value + evaluator_.delta_evaluate(solution, move);
    }

private:
    EASYLOCAL_NO_UNIQUE_ADDRESS DeltaEvaluator evaluator_;
};

template<class Component, class DeltaEvaluator, class... StoredArgs>
class delta_spec
{
public:
    using component_type = Component;
    using evaluator_type = DeltaEvaluator;
    using binding_type = delta_binding<Component, DeltaEvaluator>;

    explicit delta_spec(StoredArgs... args)
        : args_{std::move(args)...}
    {
    }

    template<class Dependency>
    [[nodiscard]]
    binding_type construct(Dependency& dependency) const
    {
        return std::apply(
            [&](const auto&... args) -> binding_type {
                using input_type = typename std::remove_cvref_t<Dependency>::input_type;
                if constexpr (std::constructible_from<
                                  DeltaEvaluator,
                                  const input_type&,
                                  const StoredArgs&...>)
                {
                    return binding_type{
                        DeltaEvaluator{dependency.input(), args...},
                    };
                }
                else
                {
                    static_assert(
                        std::constructible_from<
                            DeltaEvaluator,
                            const StoredArgs&...>,
                        "a delta evaluator must be constructible either from the "
                        "bound Instance followed by its recipe arguments or from "
                        "its recipe arguments alone");
                    return binding_type{DeltaEvaluator{args...}};
                }
            },
            args_);
    }

    [[nodiscard]]
    std::tuple<StoredArgs...>&& args() && noexcept
    {
        return std::move(args_);
    }

private:
    std::tuple<StoredArgs...> args_;
};

template<class Component>
class colocated_delta_binding
{
public:
    using component_type = Component;

    explicit colocated_delta_binding(const Component& component) noexcept
        : component_{component}
    {
    }

    template<class Value, class Solution, class Move>
        requires requires(
            const Value& value,
            const Component& component,
            const Solution& solution,
            const Move& move) {
            {
                value + component.delta_evaluate(solution, move)
            } -> std::same_as<Value>;
        }
    [[nodiscard]]
    Value apply(const Value& value, const Solution& solution, const Move& move) const
    {
        return value + component_.get().delta_evaluate(solution, move);
    }

private:
    std::reference_wrapper<const Component> component_;
};

template<class Component>
class colocated_delta_spec
{
public:
    using component_type = Component;
    using evaluator_type = Component;
    using binding_type = colocated_delta_binding<Component>;

    template<class Dependency>
    [[nodiscard]]
    binding_type construct(Dependency& dependency) const
    {
        return binding_type{dependency.template component<Component>()};
    }
};

template<class BaseNHE, class... DeltaSpecs>
class delta_cost_layer : public BaseNHE
{
public:
    using base_type = BaseNHE;
    using delta_bindings_type = std::tuple<typename DeltaSpecs::binding_type...>;

    static_assert(
        unique_types_v<typename DeltaSpecs::component_type...>,
        "a neighborhood recipe may attach at most one delta evaluator "
        "to each component type; the conflicting component type is shown in "
        "the template instantiation context");

    delta_cost_layer(
        BaseNHE base,
        typename DeltaSpecs::binding_type... bindings)
        : BaseNHE{std::move(base)},
          delta_bindings_{std::move(bindings)...}
    {
    }

    [[nodiscard]]
    const delta_bindings_type& delta_bindings() const noexcept
    {
        return delta_bindings_;
    }

private:
    EASYLOCAL_NO_UNIQUE_ADDRESS delta_bindings_type delta_bindings_;
};

template<class BaseNHE, class... DeltaSpecs>
using neighborhood_service_t = std::conditional_t<
    sizeof...(DeltaSpecs) == 0,
    BaseNHE,
    delta_cost_layer<BaseNHE, DeltaSpecs...>>;

template<class BaseNHE, class Dependency, class... Args>
consteval bool base_neighborhood_constructible_from_args()
{
    if constexpr (requires(Dependency& dependency) { dependency.base(); })
    {
        return std::constructible_from<
                   BaseNHE,
                   decltype(std::declval<Dependency&>().base()),
                   const Args&...> ||
               std::constructible_from<BaseNHE, Dependency&, const Args&...>;
    }
    else
    {
        return std::constructible_from<BaseNHE, Dependency&, const Args&...>;
    }
}

} // namespace easylocal::detail
