#pragma once

#include <easylocal/config/parameters.hpp>
#include <easylocal/config/tree.hpp>
#include <easylocal/cost.hpp>

#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::aggregation
{

namespace detail
{

struct lexicographic_tag
{
};

template<class Tag, class... Values>
class ordered_cost
{
public:
    constexpr explicit ordered_cost(Values... values)
        : values_{std::move(values)...}
    {
    }

    template<std::size_t Index>
    [[nodiscard]]
    constexpr auto get() const noexcept
        -> const std::tuple_element_t<Index, std::tuple<Values...>>&
    {
        return std::get<Index>(values_);
    }

    auto operator<=>(const ordered_cost&) const = default;

private:
    std::tuple<Values...> values_;
};

} // namespace detail

template<class... Values>
using lexicographic_cost =
    detail::ordered_cost<detail::lexicographic_tag, Values...>;

template<class HardCost, class SoftCost>
class hierarchical_cost
{
public:
    using hard_cost_type = HardCost;
    using soft_cost_type = SoftCost;

    constexpr explicit hierarchical_cost(HardCost hard, SoftCost soft)
        : hard_{std::move(hard)},
          soft_{std::move(soft)}
    {
    }

    [[nodiscard]]
    constexpr auto hard() const noexcept -> const HardCost&
    {
        return hard_;
    }

    [[nodiscard]]
    constexpr auto soft() const noexcept -> const SoftCost&
    {
        return soft_;
    }

    auto operator<=>(const hierarchical_cost&) const = default;

    [[nodiscard]]
    friend constexpr auto delta(
        const hierarchical_cost& candidate,
        const hierarchical_cost& current) -> long double
        requires requires(const HardCost& lhs, const HardCost& rhs) {
            { lhs < rhs } -> std::convertible_to<bool>;
            { lhs == rhs } -> std::convertible_to<bool>;
        } && delta_cost<SoftCost>
    {
        if (candidate.hard() < current.hard())
        {
            return -std::numeric_limits<long double>::infinity();
        }
        if (current.hard() < candidate.hard())
        {
            return std::numeric_limits<long double>::infinity();
        }
        if (candidate.hard() == current.hard())
        {
            return static_cast<long double>(
                delta(candidate.soft(), current.soft()));
        }

        // A hierarchical cost requires a total ordering of the hard branch for
        // a meaningful numeric delta. Conservatively make an unordered hard
        // transition unacceptable to delta-based algorithms.
        return std::numeric_limits<long double>::infinity();
    }

    [[nodiscard]]
    friend constexpr auto operator-(
        const hierarchical_cost& candidate,
        const hierarchical_cost& current) -> long double
        requires delta_cost<hierarchical_cost>
    {
        return delta(candidate, current);
    }

private:
    HardCost hard_;
    SoftCost soft_;
};


template<class T>
struct is_hierarchical_cost : std::false_type
{
};

template<class HardCost, class SoftCost>
struct is_hierarchical_cost<hierarchical_cost<HardCost, SoftCost>>
    : std::true_type
{
};

template<class T>
inline constexpr bool is_hierarchical_cost_v =
    is_hierarchical_cost<std::remove_cvref_t<T>>::value;

template<class T>
concept hierarchical_cost_type = is_hierarchical_cost_v<T>;

struct lexicographic
{
    template<class... Values>
    [[nodiscard]]
    constexpr auto operator()(Values&&... values) const
    {
        return lexicographic_cost<std::remove_cvref_t<Values>...>{
            std::forward<Values>(values)...,
        };
    }
};

struct hierarchical
{
    template<class HardCost, class SoftCost>
    [[nodiscard]]
    constexpr auto operator()(HardCost&& hard, SoftCost&& soft) const
    {
        return hierarchical_cost<
            std::remove_cvref_t<HardCost>,
            std::remove_cvref_t<SoftCost>>{
            std::forward<HardCost>(hard),
            std::forward<SoftCost>(soft),
        };
    }
};

template<class Weight, std::size_t Size>
struct weighted_sum_parameters
{
    std::array<Weight, Size> weights{};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"weights", &weighted_sum_parameters::weights>(
                "Aggregation weights"));
    }

    [[nodiscard]]
    constexpr auto validate() const noexcept -> config::validation_result
    {
        return config::validation_result::success();
    }
};

template<class Weight, std::size_t Size>
class weighted_sum
{
public:
    static_assert(Size > 0, "weighted_sum requires at least one weight");

    using weight_type = Weight;
    using parameters_type = weighted_sum_parameters<Weight, Size>;

    constexpr explicit weighted_sum(parameters_type parameters) noexcept
        : parameters_{std::move(parameters)}
    {
    }

    template<class... Weights>
        requires (
            sizeof...(Weights) == Size &&
            (std::convertible_to<Weights, Weight> && ...))
    constexpr explicit weighted_sum(Weights&&... weights) noexcept
        : parameters_{
              .weights = {
                  static_cast<Weight>(std::forward<Weights>(weights))...,
              },
          }
    {
    }

    [[nodiscard]]
    constexpr auto parameters() const noexcept -> const parameters_type&
    {
        return parameters_;
    }

    [[nodiscard]]
    constexpr auto configure(parameters_type parameters) noexcept
        -> config::validation_result
    {
        const auto validation = parameters.validate();
        if (!validation)
        {
            return validation;
        }
        parameters_ = std::move(parameters);
        return config::validation_result::success();
    }

    [[nodiscard]]
    constexpr auto configuration() noexcept
    {
        return config::endpoint<"cost">(*this);
    }

    [[nodiscard]]
    constexpr auto configuration() const noexcept
    {
        return config::endpoint<"cost">(*this);
    }

    template<class... Values>
        requires (sizeof...(Values) == Size)
    [[nodiscard]]
    constexpr auto operator()(Values&&... values) const
    {
        return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
            auto tuple = std::forward_as_tuple(std::forward<Values>(values)...);
            return (... +
                (parameters_.weights[Indices] *
                 std::get<Indices>(std::move(tuple))));
        }(std::make_index_sequence<Size>{});
    }

private:
    parameters_type parameters_;
};

template<class... Weights>
weighted_sum(Weights...)
    -> weighted_sum<std::common_type_t<std::remove_cvref_t<Weights>...>,
                    sizeof...(Weights)>;

template<class Weight, std::size_t Size>
weighted_sum(weighted_sum_parameters<Weight, Size>)
    -> weighted_sum<Weight, Size>;

template<class Weight, std::size_t SoftSize>
struct weighted_sum_with_hard_penalty_parameters
{
    Weight hard_multiplier{};
    std::array<Weight, SoftSize> soft_weights{};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "hard_multiplier",
                &weighted_sum_with_hard_penalty_parameters::hard_multiplier>(
                    "Multiplier applied to the hard cost"),
            config::field<
                "soft_weights",
                &weighted_sum_with_hard_penalty_parameters::soft_weights>(
                    "Weights applied to soft cost components"));
    }

    [[nodiscard]]
    constexpr auto validate() const noexcept -> config::validation_result
    {
        return config::validation_result::success();
    }
};

template<class Weight, std::size_t SoftSize>
class weighted_sum_with_hard_penalty
{
public:
    using weight_type = Weight;
    using parameters_type =
        weighted_sum_with_hard_penalty_parameters<Weight, SoftSize>;

    constexpr explicit weighted_sum_with_hard_penalty(
        parameters_type parameters) noexcept
        : parameters_{std::move(parameters)}
    {
    }

    constexpr weighted_sum_with_hard_penalty(
        Weight hard_multiplier,
        std::array<Weight, SoftSize> soft_weights) noexcept
        : parameters_{
              .hard_multiplier = std::move(hard_multiplier),
              .soft_weights = std::move(soft_weights),
          }
    {
    }

    [[nodiscard]]
    constexpr auto parameters() const noexcept -> const parameters_type&
    {
        return parameters_;
    }

    [[nodiscard]]
    constexpr auto configure(parameters_type parameters) noexcept
        -> config::validation_result
    {
        const auto validation = parameters.validate();
        if (!validation)
        {
            return validation;
        }
        parameters_ = std::move(parameters);
        return config::validation_result::success();
    }

    [[nodiscard]]
    constexpr auto configuration() noexcept
    {
        return config::endpoint<"cost">(*this);
    }

    [[nodiscard]]
    constexpr auto configuration() const noexcept
    {
        return config::endpoint<"cost">(*this);
    }

    template<class Hard, class... Soft>
        requires (sizeof...(Soft) == SoftSize)
    [[nodiscard]]
    constexpr auto operator()(Hard&& hard, Soft&&... soft) const
    {
        const auto soft_part = [&]<std::size_t... Indices>(
                                   std::index_sequence<Indices...>) {
            if constexpr (SoftSize == 0)
            {
                return Weight{};
            }
            else
            {
                auto tuple =
                    std::forward_as_tuple(std::forward<Soft>(soft)...);
                return (... +
                    (parameters_.soft_weights[Indices] *
                     std::get<Indices>(std::move(tuple))));
            }
        }(std::make_index_sequence<SoftSize>{});

        return parameters_.hard_multiplier * std::forward<Hard>(hard) +
               soft_part;
    }

private:
    parameters_type parameters_;
};

template<class Weight, std::size_t SoftSize>
weighted_sum_with_hard_penalty(
    weighted_sum_with_hard_penalty_parameters<Weight, SoftSize>)
    -> weighted_sum_with_hard_penalty<Weight, SoftSize>;

template<class Weight, std::size_t SoftSize>
weighted_sum_with_hard_penalty(Weight, std::array<Weight, SoftSize>)
    -> weighted_sum_with_hard_penalty<Weight, SoftSize>;

} // namespace easylocal::aggregation
