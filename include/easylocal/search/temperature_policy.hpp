#pragma once

#include <easylocal/config/parameters.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>

namespace easylocal::search
{

template<class Policy>
concept temperature_policy =
    requires(Policy& policy, const Policy& const_policy, const bool accepted)
    {
        { policy.reset() } -> std::same_as<void>;
        { const_policy.temperature() } -> std::convertible_to<double>;
        { policy.on_iteration(accepted) } -> std::same_as<void>;
        { const_policy.finished() } -> std::convertible_to<bool>;
    };

namespace detail
{

inline void validate_temperature_parameters(
    const double initial_temperature,
    const double final_temperature,
    const double cooling_rate)
{
    assert(std::isfinite(initial_temperature));
    assert(std::isfinite(final_temperature));
    assert(std::isfinite(cooling_rate));
    assert(initial_temperature > 0.0);
    assert(final_temperature > 0.0);
    assert(final_temperature < initial_temperature);
    assert(cooling_rate > 0.0);
    assert(cooling_rate < 1.0);
}

[[nodiscard]]
inline auto temperature_level_count(
    const double initial_temperature,
    const double final_temperature,
    const double cooling_rate) -> std::size_t
{
    validate_temperature_parameters(
        initial_temperature,
        final_temperature,
        cooling_rate);

    const auto raw_levels =
        std::log(final_temperature / initial_temperature) /
        std::log(cooling_rate);

    return std::max(
        std::size_t{1},
        static_cast<std::size_t>(std::ceil(raw_levels)));
}

[[nodiscard]]
constexpr auto positive_quotient(
    const std::size_t numerator,
    const std::size_t denominator) noexcept -> std::size_t
{
    assert(denominator != 0);
    return std::max(std::size_t{1}, numerator / denominator);
}

[[nodiscard]]
inline auto accepted_limit(
    const std::size_t sample_limit,
    const double accepted_ratio) -> std::size_t
{
    assert(std::isfinite(accepted_ratio));
    assert(accepted_ratio > 0.0);
    assert(accepted_ratio <= 1.0);

    return std::max(
        std::size_t{1},
        static_cast<std::size_t>(
            static_cast<double>(sample_limit) * accepted_ratio));
}

} // namespace detail

namespace temperature
{

struct ClassicParameters
{
    double initial_temperature;
    double final_temperature;
    double cooling_rate;
    std::size_t samples_per_temperature;
};

class Classic
{
public:
    explicit Classic(
        const ClassicParameters parameters) noexcept
        : parameters_{parameters}
    {
        detail::validate_temperature_parameters(
            parameters_.initial_temperature,
            parameters_.final_temperature,
            parameters_.cooling_rate);
        assert(parameters_.samples_per_temperature >= 1);
        reset();
    }

    void reset() noexcept
    {
        temperature_ = parameters_.initial_temperature;
        sampled_ = 0;
    }

    [[nodiscard]]
    auto temperature() const noexcept -> double
    {
        return temperature_;
    }

    void on_iteration(const bool) noexcept
    {
        ++sampled_;
        if (sampled_ >= parameters_.samples_per_temperature)
        {
            temperature_ *= parameters_.cooling_rate;
            sampled_ = 0;
        }
    }

    [[nodiscard]]
    auto finished() const noexcept -> bool
    {
        return temperature_ <= parameters_.final_temperature;
    }

private:
    ClassicParameters parameters_;
    double temperature_{};
    std::size_t sampled_{};
};

struct FixedLengthParameters
{
    double initial_temperature;
    double final_temperature;
    double cooling_rate;
    std::size_t max_iterations;

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "initial_temperature",
                &FixedLengthParameters::initial_temperature>(
                    "Initial annealing temperature"),
            config::field<
                "final_temperature",
                &FixedLengthParameters::final_temperature>(
                    "Final annealing temperature"),
            config::field<
                "cooling_rate",
                &FixedLengthParameters::cooling_rate>(
                    "Multiplicative cooling factor"),
            config::field<
                "max_iterations",
                &FixedLengthParameters::max_iterations>(
                    "Maximum number of annealing iterations"));
    }

    [[nodiscard]]
    auto validate() const noexcept -> config::validation_result
    {
        if (!std::isfinite(initial_temperature) || initial_temperature <= 0.0)
        {
            return config::validation_result::failure(
                "initial_temperature must be finite and positive");
        }

        if (!std::isfinite(final_temperature) || final_temperature <= 0.0)
        {
            return config::validation_result::failure(
                "final_temperature must be finite and positive");
        }

        if (final_temperature >= initial_temperature)
        {
            return config::validation_result::failure(
                "final_temperature must be smaller than initial_temperature");
        }

        if (!std::isfinite(cooling_rate) ||
            cooling_rate <= 0.0 || cooling_rate >= 1.0)
        {
            return config::validation_result::failure(
                "cooling_rate must be finite and in the open interval (0, 1)");
        }

        if (max_iterations == 0)
        {
            return config::validation_result::failure(
                "max_iterations must be positive");
        }

        return config::validation_result::success();
    }
};

class FixedLength
{
public:
    explicit FixedLength(
        const FixedLengthParameters parameters) noexcept
        : parameters_{parameters},
          temperature_levels_{detail::temperature_level_count(
              parameters.initial_temperature,
              parameters.final_temperature,
              parameters.cooling_rate)},
          samples_per_temperature_{detail::positive_quotient(
              parameters.max_iterations,
              temperature_levels_)}
    {
        assert(parameters_.max_iterations >= 1);
        reset();
    }

    void reset() noexcept
    {
        temperature_ = parameters_.initial_temperature;
        iterations_ = 0;
        sampled_ = 0;
    }

    [[nodiscard]]
    auto temperature() const noexcept -> double
    {
        return temperature_;
    }

    void on_iteration(const bool) noexcept
    {
        assert(!finished());
        ++iterations_;
        ++sampled_;

        if (!finished() && sampled_ >= samples_per_temperature_)
        {
            temperature_ *= parameters_.cooling_rate;
            sampled_ = 0;
        }
    }

    [[nodiscard]]
    auto finished() const noexcept -> bool
    {
        return iterations_ >= parameters_.max_iterations;
    }

    [[nodiscard]]
    auto samples_per_temperature() const noexcept -> std::size_t
    {
        return samples_per_temperature_;
    }

private:
    FixedLengthParameters parameters_;
    std::size_t temperature_levels_{};
    std::size_t samples_per_temperature_{};
    double temperature_{};
    std::size_t iterations_{};
    std::size_t sampled_{};
};

struct CutoffParameters
{
    double initial_temperature;
    double final_temperature;
    double cooling_rate;
    std::size_t max_iterations;
    double accepted_ratio;
};

class Cutoff
{
public:
    explicit Cutoff(
        const CutoffParameters parameters) noexcept
        : parameters_{parameters},
          temperature_levels_{detail::temperature_level_count(
              parameters.initial_temperature,
              parameters.final_temperature,
              parameters.cooling_rate)},
          reference_sample_limit_{detail::positive_quotient(
              parameters.max_iterations,
              temperature_levels_)},
          accepted_limit_{detail::accepted_limit(
              reference_sample_limit_,
              parameters.accepted_ratio)}
    {
        assert(parameters_.max_iterations >= 1);
        reset();
    }

    void reset() noexcept
    {
        temperature_ = parameters_.initial_temperature;
        iterations_ = 0;
        accepted_ = 0;
    }

    [[nodiscard]]
    auto temperature() const noexcept -> double
    {
        return temperature_;
    }

    void on_iteration(const bool accepted) noexcept
    {
        assert(!finished());
        ++iterations_;
        accepted_ += accepted ? 1U : 0U;

        if (!finished() && accepted_ >= accepted_limit_)
        {
            temperature_ *= parameters_.cooling_rate;
            accepted_ = 0;
        }
    }

    [[nodiscard]]
    auto finished() const noexcept -> bool
    {
        return iterations_ >= parameters_.max_iterations;
    }

    [[nodiscard]]
    auto accepted_limit() const noexcept -> std::size_t
    {
        return accepted_limit_;
    }

private:
    CutoffParameters parameters_;
    std::size_t temperature_levels_{};
    std::size_t reference_sample_limit_{};
    std::size_t accepted_limit_{};
    double temperature_{};
    std::size_t iterations_{};
    std::size_t accepted_{};
};

using HybridParameters = CutoffParameters;

class Hybrid
{
public:
    explicit Hybrid(
        const HybridParameters parameters) noexcept
        : parameters_{parameters},
          temperature_levels_{detail::temperature_level_count(
              parameters.initial_temperature,
              parameters.final_temperature,
              parameters.cooling_rate)},
          initial_sample_limit_{detail::positive_quotient(
              parameters.max_iterations,
              temperature_levels_)},
          accepted_limit_{detail::accepted_limit(
              initial_sample_limit_,
              parameters.accepted_ratio)}
    {
        assert(parameters_.max_iterations >= 1);
        reset();
    }

    void reset() noexcept
    {
        temperature_ = parameters_.initial_temperature;
        iterations_ = 0;
        sampled_ = 0;
        accepted_ = 0;
        completed_levels_ = 0;
        current_sample_limit_ = initial_sample_limit_;
    }

    [[nodiscard]]
    auto temperature() const noexcept -> double
    {
        return temperature_;
    }

    void on_iteration(const bool accepted) noexcept
    {
        assert(!finished());
        ++iterations_;
        ++sampled_;
        accepted_ += accepted ? 1U : 0U;

        if (finished())
        {
            return;
        }

        const auto accepted_cutoff = accepted_ >= accepted_limit_;
        const auto sampled_limit = sampled_ >= current_sample_limit_;

        if (sampled_limit || accepted_cutoff)
        {
            const auto cutoff_saved_iterations =
                accepted_cutoff && !sampled_limit;

            temperature_ *= parameters_.cooling_rate;
            ++completed_levels_;

            if (cutoff_saved_iterations)
            {
                const auto remaining_iterations =
                    parameters_.max_iterations - iterations_;
                const auto remaining_levels =
                    temperature_levels_ > completed_levels_
                    ? temperature_levels_ - completed_levels_
                    : std::size_t{1};

                current_sample_limit_ = detail::positive_quotient(
                    remaining_iterations,
                    remaining_levels);
            }

            sampled_ = 0;
            accepted_ = 0;
        }
    }

    [[nodiscard]]
    auto finished() const noexcept -> bool
    {
        return iterations_ >= parameters_.max_iterations;
    }

    [[nodiscard]]
    auto sample_limit() const noexcept -> std::size_t
    {
        return current_sample_limit_;
    }

    [[nodiscard]]
    auto accepted_limit() const noexcept -> std::size_t
    {
        return accepted_limit_;
    }

private:
    HybridParameters parameters_;
    std::size_t temperature_levels_{};
    std::size_t initial_sample_limit_{};
    std::size_t accepted_limit_{};
    double temperature_{};
    std::size_t iterations_{};
    std::size_t sampled_{};
    std::size_t accepted_{};
    std::size_t completed_levels_{};
    std::size_t current_sample_limit_{};
};

} // namespace temperature

static_assert(temperature_policy<temperature::Classic>);
static_assert(temperature_policy<temperature::FixedLength>);
static_assert(temperature_policy<temperature::Cutoff>);
static_assert(temperature_policy<temperature::Hybrid>);

} // namespace easylocal::search
