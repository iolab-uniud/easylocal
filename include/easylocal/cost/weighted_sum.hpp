#pragma once

#include <easylocal/config/parameters.hpp>
#include <easylocal/config/tree.hpp>

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

// Weighted-sum aggregators: configurable function objects mapping component
// values to a scalar cost.
namespace easylocal::cost
{

inline constexpr auto default_hard_multiplier = 10'000;

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
    Weight hard_multiplier{static_cast<Weight>(default_hard_multiplier)};
    std::array<Weight, SoftSize> soft_weights = [] {
        std::array<Weight, SoftSize> result{};
        result.fill(Weight{1});
        return result;
    }();

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

    constexpr weighted_sum_with_hard_penalty() noexcept = default;

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

} // namespace easylocal::cost
