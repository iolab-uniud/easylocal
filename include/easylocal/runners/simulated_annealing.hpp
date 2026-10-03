#pragma once

// Simulated Annealing together with its policies: temperature schedules
// (runners::temperature) and the Metropolis acceptance criterion.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost/concepts.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::runners
{

// Temperature policies.

template<class Policy>
concept temperature_policy =
    requires(Policy& policy, const Policy& const_policy, const bool accepted)
    {
        { policy.reset() } -> std::same_as<void>;
        { const_policy.temperature() } -> std::convertible_to<double>;
        { policy.on_iteration(accepted) } -> std::same_as<void>;
        { const_policy.finished() } -> std::convertible_to<bool>;
    };

// A temperature policy that can estimate its initial temperature: before the
// run Simulated Annealing evaluates calibration_samples() random moves at the
// initial solution, without applying them, and passes their deltas to
// calibrate(), then calls reset(). The built-in policies are calibrating.
template<class Policy>
concept calibrating_temperature_policy = temperature_policy<Policy>
    && requires(
        Policy& policy,
        const Policy& const_policy,
        std::span<const double> deltas) {
           { const_policy.calibration_samples() } -> std::convertible_to<std::size_t>;
           { policy.calibrate(deltas) } -> std::same_as<void>;
       };

namespace detail
{

inline void validate_temperature_parameters(
    [[maybe_unused]] const double initial_temperature,
    [[maybe_unused]] const double final_temperature,
    [[maybe_unused]] const double cooling_rate)
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
inline std::size_t temperature_level_count(
    const double initial_temperature,
    const double final_temperature,
    const double cooling_rate)
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
constexpr std::size_t positive_quotient(
    const std::size_t numerator,
    const std::size_t denominator) noexcept
{
    assert(denominator != 0);
    return std::max(std::size_t{1}, numerator / denominator);
}

[[nodiscard]]
inline std::size_t accepted_limit(
    const std::size_t sample_limit,
    const double accepted_ratio)
{
    assert(std::isfinite(accepted_ratio));
    assert(accepted_ratio > 0.0);
    assert(accepted_ratio <= 1.0);

    return std::max(
        std::size_t{1},
        static_cast<std::size_t>(
            static_cast<double>(sample_limit) * accepted_ratio));
}

[[nodiscard]]
inline config::validation_result validate_cooling_schedule(
    const double initial_temperature,
    const double final_temperature,
    const double cooling_rate) noexcept
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
    if (!std::isfinite(cooling_rate) || cooling_rate <= 0.0 || cooling_rate >= 1.0)
    {
        return config::validation_result::failure(
            "cooling_rate must be finite and in the open interval (0, 1)");
    }
    return config::validation_result::success();
}

[[nodiscard]]
inline config::validation_result validate_calibration(
    const double initial_acceptance) noexcept
{
    if (!std::isfinite(initial_acceptance) || initial_acceptance <= 0.0
        || initial_acceptance >= 1.0)
    {
        return config::validation_result::failure(
            "initial_acceptance must be finite and in the open interval (0, 1)");
    }
    return config::validation_result::success();
}

// The initial temperature at which a worsening move of average size is
// accepted with probability initial_acceptance (Johnson et al., 1989), from
// the deltas of moves sampled at the initial solution; improving moves and
// infinite deltas (a hierarchical hard level) are ignored. Empty when no
// sampled move worsens the cost.
[[nodiscard]]
inline std::optional<double> estimate_temperature(
    const std::span<const double> deltas,
    const double initial_acceptance)
{
    double sum = 0.0;
    std::size_t count = 0;
    for (const auto delta : deltas)
    {
        if (delta > 0.0 && std::isfinite(delta))
        {
            sum += delta;
            ++count;
        }
    }
    if (count == 0)
        return std::nullopt;
    return -(sum / static_cast<double>(count)) / std::log(initial_acceptance);
}

// calibrate() for policies with an initial temperature: rebuild with the
// estimated one, kept above lowest so that the schedule stays valid.
template<class Policy, class Parameters>
void calibrate_initial_temperature(
    Policy& policy,
    Parameters parameters,
    const std::span<const double> deltas,
    const double lowest)
{
    const auto estimate = estimate_temperature(deltas, parameters.initial_acceptance);
    if (!estimate.has_value())
        return;
    parameters.initial_temperature = std::max(*estimate, lowest);
    policy = Policy{parameters};
}

} // namespace detail

namespace temperature
{

struct ClassicParameters
{
    double initial_temperature{10.0};
    double final_temperature{0.01};
    double cooling_rate{0.95};
    std::size_t samples_per_temperature{100};

    // Moves sampled at the initial solution to estimate the initial
    // temperature; 0 keeps initial_temperature.
    std::size_t calibration_samples{0};
    // Acceptance probability of an average worsening move at the estimated
    // initial temperature.
    double initial_acceptance{0.5};
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"initial_temperature", &ClassicParameters::initial_temperature>(
                "Initial annealing temperature"),
            config::field<"final_temperature", &ClassicParameters::final_temperature>(
                "Final annealing temperature"),
            config::field<"cooling_rate", &ClassicParameters::cooling_rate>(
                "Multiplicative cooling factor"),
            config::field<
                "samples_per_temperature",
                &ClassicParameters::samples_per_temperature>(
                "Proposals evaluated at each temperature"),
            config::field<"calibration_samples", &ClassicParameters::calibration_samples>(
                "Moves sampled to estimate the initial temperature (0: none)"),
            config::field<"initial_acceptance", &ClassicParameters::initial_acceptance>(
                "Acceptance probability of an average worsening move at the "
                "estimated initial temperature"));
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        const auto calibration = detail::validate_calibration(initial_acceptance);
        if (!calibration)
            return calibration;
        const auto schedule = detail::validate_cooling_schedule(
            initial_temperature, final_temperature, cooling_rate);
        if (!schedule)
        {
            return schedule;
        }
        if (samples_per_temperature == 0)
        {
            return config::validation_result::failure(
                "samples_per_temperature must be positive");
        }
        return config::validation_result::success();
    }
};

class Classic
{
public:
    using parameters_type = ClassicParameters;

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

    [[nodiscard]]
    const ClassicParameters& parameters() const noexcept
    {
        return parameters_;
    }

    [[nodiscard]]
    std::size_t calibration_samples() const noexcept
    {
        return parameters_.calibration_samples;
    }

    void calibrate(const std::span<const double> deltas)
    {
        detail::calibrate_initial_temperature(
            *this,
            parameters_,
            deltas,
            parameters_.final_temperature / parameters_.cooling_rate);
    }

    void reset() noexcept
    {
        temperature_ = parameters_.initial_temperature;
        sampled_ = 0;
    }

    [[nodiscard]]
    double temperature() const noexcept
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
    bool finished() const noexcept
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
    double initial_temperature{10.0};
    double final_temperature{0.01};
    double cooling_rate{0.95};
    std::size_t max_iterations{100'000};

    // Moves sampled at the initial solution to estimate the initial
    // temperature; 0 keeps initial_temperature.
    std::size_t calibration_samples{0};
    // Acceptance probability of an average worsening move at the estimated
    // initial temperature.
    double initial_acceptance{0.5};
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "initial_temperature",
                &FixedLengthParameters::initial_temperature>(
                "Initial annealing temperature"),
            config::field<"final_temperature", &FixedLengthParameters::final_temperature>(
                "Final annealing temperature"),
            config::field<"cooling_rate", &FixedLengthParameters::cooling_rate>(
                "Multiplicative cooling factor"),
            config::field<"max_iterations", &FixedLengthParameters::max_iterations>(
                "Maximum number of annealing iterations"),
            config::field<
                "calibration_samples",
                &FixedLengthParameters::calibration_samples>(
                "Moves sampled to estimate the initial temperature (0: none)"),
            config::field<
                "initial_acceptance",
                &FixedLengthParameters::initial_acceptance>(
                "Acceptance probability of an average worsening move at the "
                "estimated initial temperature"));
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        const auto calibration = detail::validate_calibration(initial_acceptance);
        if (!calibration)
            return calibration;
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
    using parameters_type = FixedLengthParameters;

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

    [[nodiscard]]
    const FixedLengthParameters& parameters() const noexcept
    {
        return parameters_;
    }

    [[nodiscard]]
    std::size_t calibration_samples() const noexcept
    {
        return parameters_.calibration_samples;
    }

    void calibrate(const std::span<const double> deltas)
    {
        detail::calibrate_initial_temperature(
            *this,
            parameters_,
            deltas,
            parameters_.final_temperature / parameters_.cooling_rate);
    }

    void reset() noexcept
    {
        temperature_ = parameters_.initial_temperature;
        iterations_ = 0;
        sampled_ = 0;
    }

    [[nodiscard]]
    double temperature() const noexcept
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
    bool finished() const noexcept
    {
        return iterations_ >= parameters_.max_iterations;
    }

    [[nodiscard]]
    std::size_t samples_per_temperature() const noexcept
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
    double initial_temperature{10.0};
    double final_temperature{0.01};
    double cooling_rate{0.95};
    std::size_t max_iterations{100'000};
    double accepted_ratio{0.1};

    // Moves sampled at the initial solution to estimate the initial
    // temperature; 0 keeps initial_temperature.
    std::size_t calibration_samples{0};
    // Acceptance probability of an average worsening move at the estimated
    // initial temperature.
    double initial_acceptance{0.5};
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"initial_temperature", &CutoffParameters::initial_temperature>(
                "Initial annealing temperature"),
            config::field<"final_temperature", &CutoffParameters::final_temperature>(
                "Final annealing temperature"),
            config::field<"cooling_rate", &CutoffParameters::cooling_rate>(
                "Multiplicative cooling factor"),
            config::field<"max_iterations", &CutoffParameters::max_iterations>(
                "Maximum number of annealing iterations"),
            config::field<"accepted_ratio", &CutoffParameters::accepted_ratio>(
                "Fraction of accepted proposals that triggers cooling"),
            config::field<"calibration_samples", &CutoffParameters::calibration_samples>(
                "Moves sampled to estimate the initial temperature (0: none)"),
            config::field<"initial_acceptance", &CutoffParameters::initial_acceptance>(
                "Acceptance probability of an average worsening move at the "
                "estimated initial temperature"));
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        const auto calibration = detail::validate_calibration(initial_acceptance);
        if (!calibration)
            return calibration;
        const auto schedule = detail::validate_cooling_schedule(
            initial_temperature, final_temperature, cooling_rate);
        if (!schedule)
        {
            return schedule;
        }
        if (max_iterations == 0)
        {
            return config::validation_result::failure(
                "max_iterations must be positive");
        }
        if (!std::isfinite(accepted_ratio) || accepted_ratio <= 0.0 || accepted_ratio > 1.0)
        {
            return config::validation_result::failure(
                "accepted_ratio must be finite and in the interval (0, 1]");
        }
        return config::validation_result::success();
    }
};

class Cutoff
{
public:
    using parameters_type = CutoffParameters;

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

    [[nodiscard]]
    const CutoffParameters& parameters() const noexcept
    {
        return parameters_;
    }

    [[nodiscard]]
    std::size_t calibration_samples() const noexcept
    {
        return parameters_.calibration_samples;
    }

    void calibrate(const std::span<const double> deltas)
    {
        detail::calibrate_initial_temperature(
            *this,
            parameters_,
            deltas,
            parameters_.final_temperature / parameters_.cooling_rate);
    }

    void reset() noexcept
    {
        temperature_ = parameters_.initial_temperature;
        iterations_ = 0;
        accepted_ = 0;
    }

    [[nodiscard]]
    double temperature() const noexcept
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
    bool finished() const noexcept
    {
        return iterations_ >= parameters_.max_iterations;
    }

    [[nodiscard]]
    std::size_t accepted_limit() const noexcept
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
    using parameters_type = HybridParameters;

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

    [[nodiscard]]
    const HybridParameters& parameters() const noexcept
    {
        return parameters_;
    }

    [[nodiscard]]
    std::size_t calibration_samples() const noexcept
    {
        return parameters_.calibration_samples;
    }

    void calibrate(const std::span<const double> deltas)
    {
        detail::calibrate_initial_temperature(
            *this,
            parameters_,
            deltas,
            parameters_.final_temperature / parameters_.cooling_rate);
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
    double temperature() const noexcept
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
    bool finished() const noexcept
    {
        return iterations_ >= parameters_.max_iterations;
    }

    [[nodiscard]]
    std::size_t sample_limit() const noexcept
    {
        return current_sample_limit_;
    }

    [[nodiscard]]
    std::size_t accepted_limit() const noexcept
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

struct FixedTemperatureParameters
{
    double temperature{1.0};
    std::size_t max_iterations{100'000};
    double accepted_ratio{1.0};

    // Moves sampled at the initial solution to estimate the temperature; 0
    // keeps temperature.
    std::size_t calibration_samples{0};
    // Acceptance probability of an average worsening move at the estimated
    // initial temperature.
    double initial_acceptance{0.5};
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"temperature", &FixedTemperatureParameters::temperature>(
                "Constant annealing temperature"),
            config::field<"max_iterations", &FixedTemperatureParameters::max_iterations>(
                "Maximum number of annealing iterations"),
            config::field<"accepted_ratio", &FixedTemperatureParameters::accepted_ratio>(
                "Fraction of max_iterations accepted proposals that ends the search"),
            config::field<
                "calibration_samples",
                &FixedTemperatureParameters::calibration_samples>(
                "Moves sampled to estimate the initial temperature (0: none)"),
            config::field<
                "initial_acceptance",
                &FixedTemperatureParameters::initial_acceptance>(
                "Acceptance probability of an average worsening move at the "
                "estimated initial temperature"));
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        const auto calibration = detail::validate_calibration(initial_acceptance);
        if (!calibration)
            return calibration;
        if (!std::isfinite(temperature) || temperature <= 0.0)
        {
            return config::validation_result::failure(
                "temperature must be finite and positive");
        }
        if (max_iterations == 0)
            return config::validation_result::failure("max_iterations must be positive");
        if (!std::isfinite(accepted_ratio) || accepted_ratio <= 0.0
            || accepted_ratio > 1.0)
        {
            return config::validation_result::failure(
                "accepted_ratio must be finite and in the interval (0, 1]");
        }
        return config::validation_result::success();
    }
};

// A constant temperature: the search ends after max_iterations proposals, or
// earlier once accepted_ratio * max_iterations of them have been accepted.
class FixedTemperature
{
public:
    using parameters_type = FixedTemperatureParameters;

    explicit FixedTemperature(const FixedTemperatureParameters parameters) noexcept
        : parameters_{parameters},
          accepted_limit_{detail::accepted_limit(
              parameters.max_iterations,
              parameters.accepted_ratio)}
    {
        assert(parameters_.validate());
        reset();
    }

    [[nodiscard]]
    const FixedTemperatureParameters& parameters() const noexcept
    {
        return parameters_;
    }

    [[nodiscard]]
    std::size_t calibration_samples() const noexcept
    {
        return parameters_.calibration_samples;
    }

    void calibrate(const std::span<const double> deltas)
    {
        const auto estimate =
            detail::estimate_temperature(deltas, parameters_.initial_acceptance);
        if (estimate.has_value())
        {
            auto parameters = parameters_;
            parameters.temperature = *estimate;
            *this = FixedTemperature{parameters};
        }
    }

    void reset() noexcept
    {
        iterations_ = 0;
        accepted_ = 0;
    }

    [[nodiscard]]
    double temperature() const noexcept
    {
        return parameters_.temperature;
    }

    void on_iteration(const bool accepted) noexcept
    {
        assert(!finished());
        ++iterations_;
        accepted_ += accepted ? 1U : 0U;
    }

    [[nodiscard]]
    bool finished() const noexcept
    {
        return iterations_ >= parameters_.max_iterations || accepted_ >= accepted_limit_;
    }

    [[nodiscard]]
    std::size_t accepted_limit() const noexcept
    {
        return accepted_limit_;
    }

private:
    FixedTemperatureParameters parameters_;
    std::size_t accepted_limit_{};
    std::size_t iterations_{};
    std::size_t accepted_{};
};

struct TimeBasedParameters
{
    double initial_temperature{10.0};
    double final_temperature{0.01};
    double cooling_rate{0.95};
    // Seconds.
    double allowed_running_time{10.0};
    // Accepted proposals that cool early; 0 cools only on time.
    std::size_t accepted_per_temperature{0};

    // Moves sampled at the initial solution to estimate the initial
    // temperature; 0 keeps initial_temperature.
    std::size_t calibration_samples{0};
    // Acceptance probability of an average worsening move at the estimated
    // initial temperature.
    double initial_acceptance{0.5};
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "initial_temperature",
                &TimeBasedParameters::initial_temperature>(
                "Initial annealing temperature"),
            config::field<"final_temperature", &TimeBasedParameters::final_temperature>(
                "Final annealing temperature"),
            config::field<"cooling_rate", &TimeBasedParameters::cooling_rate>(
                "Multiplicative cooling factor"),
            config::field<
                "allowed_running_time",
                &TimeBasedParameters::allowed_running_time>(
                "Running time of the annealing, in seconds"),
            config::field<
                "accepted_per_temperature",
                &TimeBasedParameters::accepted_per_temperature>(
                "Accepted proposals that trigger cooling (0: cool only on time)"),
            config::field<
                "calibration_samples",
                &TimeBasedParameters::calibration_samples>(
                "Moves sampled to estimate the initial temperature (0: none)"),
            config::field<"initial_acceptance", &TimeBasedParameters::initial_acceptance>(
                "Acceptance probability of an average worsening move at the "
                "estimated initial temperature"));
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        const auto calibration = detail::validate_calibration(initial_acceptance);
        if (!calibration)
            return calibration;
        const auto schedule = detail::validate_cooling_schedule(
            initial_temperature,
            final_temperature,
            cooling_rate);
        if (!schedule)
            return schedule;
        if (!std::isfinite(allowed_running_time) || allowed_running_time <= 0.0)
        {
            return config::validation_result::failure(
                "allowed_running_time must be finite and positive");
        }
        return config::validation_result::success();
    }
};

// The cooling schedule spread over a running time instead of an iteration
// budget: the allowed time is divided evenly among the temperature levels,
// and the temperature cools when the time of its level is over or, with
// accepted_per_temperature, after that many acceptances; the time an early
// cooling saves is redistributed over the remaining levels. The annealing
// ends when the time is over or the final temperature is reached. The
// trajectory depends on the speed of the machine, so equal seeds no longer
// give equal runs. The clock is read once per proposal.
template<class Clock = std::chrono::steady_clock>
class BasicTimeBased
{
public:
    using parameters_type = TimeBasedParameters;

    explicit BasicTimeBased(const TimeBasedParameters parameters) noexcept
        : parameters_{parameters},
          temperature_levels_{detail::temperature_level_count(
              parameters.initial_temperature,
              parameters.final_temperature,
              parameters.cooling_rate)},
          running_time_{std::chrono::duration_cast<duration>(
              std::chrono::duration<double>{parameters.allowed_running_time})}
    {
        assert(parameters_.validate());
        reset();
    }

    [[nodiscard]]
    const TimeBasedParameters& parameters() const noexcept
    {
        return parameters_;
    }

    [[nodiscard]]
    std::size_t calibration_samples() const noexcept
    {
        return parameters_.calibration_samples;
    }

    void calibrate(const std::span<const double> deltas)
    {
        detail::calibrate_initial_temperature(
            *this,
            parameters_,
            deltas,
            parameters_.final_temperature / parameters_.cooling_rate);
    }

    // Starts the clock.
    void reset() noexcept
    {
        temperature_ = parameters_.initial_temperature;
        start_ = Clock::now();
        level_start_ = start_;
        level_time_ =
            running_time_ / static_cast<typename duration::rep>(temperature_levels_);
        completed_levels_ = 0;
        accepted_ = 0;
        timed_out_ = false;
    }

    [[nodiscard]]
    double temperature() const noexcept
    {
        return temperature_;
    }

    void on_iteration(const bool accepted) noexcept
    {
        assert(!finished());
        const auto now = Clock::now();
        if (now - start_ >= running_time_)
        {
            timed_out_ = true;
            return;
        }

        accepted_ += accepted ? 1U : 0U;
        const auto level_elapsed = now - level_start_;
        const auto time_over = level_elapsed >= level_time_;
        const auto accepted_cutoff = parameters_.accepted_per_temperature != 0
            && accepted_ >= parameters_.accepted_per_temperature;
        if (!time_over && !accepted_cutoff)
            return;

        temperature_ *= parameters_.cooling_rate;
        ++completed_levels_;
        if (!time_over && completed_levels_ < temperature_levels_)
        {
            const auto remaining_levels = temperature_levels_ - completed_levels_;
            level_time_ = (running_time_ - (now - start_))
                / static_cast<typename duration::rep>(remaining_levels);
        }
        level_start_ = now;
        accepted_ = 0;
    }

    [[nodiscard]]
    bool finished() const noexcept
    {
        return timed_out_ || temperature_ <= parameters_.final_temperature;
    }

    [[nodiscard]]
    typename Clock::duration level_time() const noexcept
    {
        return level_time_;
    }

private:
    using duration = typename Clock::duration;

    TimeBasedParameters parameters_;
    std::size_t temperature_levels_{};
    duration running_time_{};
    double temperature_{};
    typename Clock::time_point start_{};
    typename Clock::time_point level_start_{};
    duration level_time_{};
    std::size_t completed_levels_{};
    std::size_t accepted_{};
    bool timed_out_{};
};

using TimeBased = BasicTimeBased<>;

} // namespace temperature

namespace detail
{

// The budget of a schedule that Reheating divides among its descents: an
// iteration budget or a running time.
template<class Parameters>
concept iteration_budget = requires(Parameters parameters) {
    { parameters.max_iterations } -> std::convertible_to<std::size_t>;
};

template<class Parameters>
concept time_budget = requires(Parameters parameters) {
    { parameters.allowed_running_time } -> std::convertible_to<double>;
};

template<class Parameters>
concept final_temperature_schedule = requires(Parameters parameters) {
    { parameters.final_temperature } -> std::convertible_to<double>;
};

// A schedule Reheating can restart: its parameters have an initial temperature.
template<class Policy>
concept reheatable_policy = temperature_policy<Policy>
    && std::constructible_from<Policy, typename Policy::parameters_type>
    && requires(typename Policy::parameters_type parameters) {
           { parameters.initial_temperature } -> std::convertible_to<double>;
           { parameters.validate() } -> std::convertible_to<config::validation_result>;
       };

} // namespace detail

namespace temperature
{

template<class DescentParameters>
struct ReheatingParameters
{
    // The schedule of the descents; the reheats restart it from a lower
    // initial temperature.
    DescentParameters descent{};
    std::size_t max_reheats{3};
    // The temperature a reheat restarts from, as a factor of the descent's
    // initial_temperature.
    double reheat_ratio{0.5};
    // The share of the descent's budget (max_iterations or
    // allowed_running_time) spent by the first descent; the reheats divide
    // the rest evenly. Ignored by a schedule without a budget.
    double first_descent_share{0.5};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::group<"descent", &ReheatingParameters::descent>(
                "The schedule of each descent"),
            config::field<"max_reheats", &ReheatingParameters::max_reheats>(
                "Number of reheats after the first descent"),
            config::field<"reheat_ratio", &ReheatingParameters::reheat_ratio>(
                "Restart temperature of a reheat, as a factor of the initial one"),
            config::field<
                "first_descent_share",
                &ReheatingParameters::first_descent_share>(
                "Share of the budget spent by the first descent"));
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        const auto schedule = descent.validate();
        if (!schedule)
            return schedule;
        if (max_reheats == 0)
            return config::validation_result::success();
        if (!std::isfinite(reheat_ratio) || reheat_ratio <= 0.0)
            return config::validation_result::failure("reheat_ratio must be positive");
        if constexpr (detail::final_temperature_schedule<DescentParameters>)
        {
            if (descent.initial_temperature * reheat_ratio <= descent.final_temperature)
            {
                return config::validation_result::failure(
                    "reheat_ratio must keep the reheat temperature above "
                    "final_temperature");
            }
        }
        if constexpr (detail::iteration_budget<DescentParameters>
            || detail::time_budget<DescentParameters>)
        {
            if (!std::isfinite(first_descent_share) || first_descent_share <= 0.0
                || first_descent_share >= 1.0)
            {
                return config::validation_result::failure(
                    "first_descent_share must be in the open interval (0, 1) when "
                    "there are reheats");
            }
        }
        return config::validation_result::success();
    }
};

// Reheats any schedule with an initial temperature: a first descent, then up
// to max_reheats descents restarting from reheat_ratio times the initial
// temperature. When the schedule has a budget, max_iterations or
// allowed_running_time, the first descent spends first_descent_share of it
// and the reheats divide the rest evenly; otherwise each descent runs the
// whole schedule. It calibrates when the schedule does, and the reheat
// temperature stays above the final one. Reheating<Hybrid> is EasyLocal 3's
// annealing with reheating.
template<detail::reheatable_policy Descent>
class Reheating
{
public:
    using descent_parameters_type = typename Descent::parameters_type;
    using parameters_type = ReheatingParameters<descent_parameters_type>;

    explicit Reheating(const parameters_type parameters)
        : parameters_{parameters}, descent_{first_descent(parameters)}
    {
        assert(parameters_.validate());
    }

    [[nodiscard]]
    const parameters_type& parameters() const noexcept
    {
        return parameters_;
    }

    [[nodiscard]]
    std::size_t calibration_samples() const noexcept
        requires calibrating_temperature_policy<Descent>
    {
        return descent_.calibration_samples();
    }

    // The schedule's estimate of the initial temperature, kept high enough
    // for the reheats.
    void calibrate(const std::span<const double> deltas)
        requires calibrating_temperature_policy<Descent>
    {
        Descent probe{first_descent(parameters_)};
        probe.calibrate(deltas);
        parameters_.descent.initial_temperature = std::max(
            probe.parameters().initial_temperature,
            lowest_initial_temperature());
        reset();
    }

    void reset()
    {
        descent_ = Descent{first_descent(parameters_)};
        reheats_ = 0;
    }

    [[nodiscard]]
    double temperature() const noexcept
    {
        return descent_.temperature();
    }

    void on_iteration(const bool accepted)
    {
        assert(!finished());
        descent_.on_iteration(accepted);
        if (descent_.finished() && reheats_ < parameters_.max_reheats)
        {
            descent_ = Descent{reheat_descent(parameters_)};
            ++reheats_;
        }
    }

    [[nodiscard]]
    bool finished() const noexcept
    {
        return reheats_ >= parameters_.max_reheats && descent_.finished();
    }

    [[nodiscard]]
    std::size_t reheats() const noexcept
    {
        return reheats_;
    }

    // The descent under way.
    [[nodiscard]]
    const Descent& descent() const noexcept
    {
        return descent_;
    }

private:
    // The lowest initial temperature that keeps the reheat temperature above
    // the final one, with at least one cooling level.
    [[nodiscard]]
    double lowest_initial_temperature() const noexcept
    {
        if constexpr (detail::final_temperature_schedule<descent_parameters_type>)
        {
            const auto& descent = parameters_.descent;
            const auto reheat_factor = parameters_.max_reheats == 0
                ? 1.0
                : std::min(1.0, parameters_.reheat_ratio);
            double cooling = 1.0;
            if constexpr (requires { descent.cooling_rate; })
                cooling = descent.cooling_rate;
            return descent.final_temperature / (cooling * reheat_factor);
        }
        else
        {
            return 0.0;
        }
    }

    [[nodiscard]]
    static descent_parameters_type first_descent(const parameters_type& parameters)
    {
        auto descent = parameters.descent;
        if (parameters.max_reheats == 0)
            return descent;
        if constexpr (detail::iteration_budget<descent_parameters_type>)
        {
            descent.max_iterations = std::max(
                std::size_t{1},
                static_cast<std::size_t>(std::ceil(
                    static_cast<double>(parameters.descent.max_iterations)
                    * parameters.first_descent_share)));
        }
        else if constexpr (detail::time_budget<descent_parameters_type>)
        {
            descent.allowed_running_time *= parameters.first_descent_share;
        }
        return descent;
    }

    [[nodiscard]]
    static descent_parameters_type reheat_descent(const parameters_type& parameters)
    {
        auto descent = parameters.descent;
        descent.initial_temperature *= parameters.reheat_ratio;
        if constexpr (detail::iteration_budget<descent_parameters_type>)
        {
            const auto first = first_descent(parameters).max_iterations;
            const auto total = parameters.descent.max_iterations;
            descent.max_iterations = detail::positive_quotient(
                total > first ? total - first : 0,
                parameters.max_reheats);
        }
        else if constexpr (detail::time_budget<descent_parameters_type>)
        {
            descent.allowed_running_time = parameters.descent.allowed_running_time
                * (1.0 - parameters.first_descent_share)
                / static_cast<double>(parameters.max_reheats);
        }
        return descent;
    }

    parameters_type parameters_;
    Descent descent_;
    std::size_t reheats_{};
};

} // namespace temperature

static_assert(temperature_policy<temperature::Classic>);
static_assert(temperature_policy<temperature::FixedLength>);
static_assert(temperature_policy<temperature::Cutoff>);
static_assert(temperature_policy<temperature::Hybrid>);
static_assert(temperature_policy<temperature::FixedTemperature>);
static_assert(temperature_policy<temperature::TimeBased>);
static_assert(temperature_policy<temperature::Reheating<temperature::Hybrid>>);
static_assert(temperature_policy<temperature::Reheating<temperature::Classic>>);
static_assert(temperature_policy<temperature::Reheating<temperature::TimeBased>>);
static_assert(calibrating_temperature_policy<temperature::Classic>);
static_assert(calibrating_temperature_policy<temperature::FixedLength>);
static_assert(calibrating_temperature_policy<temperature::Cutoff>);
static_assert(calibrating_temperature_policy<temperature::Hybrid>);
static_assert(calibrating_temperature_policy<temperature::FixedTemperature>);
static_assert(calibrating_temperature_policy<temperature::TimeBased>);
static_assert(
    calibrating_temperature_policy<temperature::Reheating<temperature::Hybrid>>);
static_assert(
    calibrating_temperature_policy<temperature::Reheating<temperature::FixedLength>>);
static_assert(!detail::reheatable_policy<temperature::FixedTemperature>);

// Acceptance policies.

namespace detail
{

template<class Cost>
concept metropolis_cost = cost::has_delta<Cost>;

} // namespace detail

class MetropolisAcceptance
{
public:
    template<detail::metropolis_cost Cost, std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    bool accept(
        const Cost& candidate,
        const Cost& current,
        const double temperature,
        RNG& rng) const
    {
        assert(std::isfinite(temperature));
        assert(temperature > 0.0);

        using cost::delta;
        const auto difference =
            static_cast<long double>(delta(candidate, current));
        assert(!std::isnan(difference));

        if (difference <= 0.0L)
        {
            return true;
        }
        if (std::isinf(difference))
        {
            return false;
        }

        // The probability is compared with a double, so exp in double: the
        // long double exp costs several times more and dominated cheap moves.
        const auto probability = std::exp(static_cast<double>(
            -difference / static_cast<long double>(temperature)));
        std::uniform_real_distribution<double> draw{0.0, 1.0};
        return draw(rng) < probability;
    }
};

// Algorithm.

// The parameters of Simulated Annealing: its temperature policy's, as the
// group "temperature" (paths temperature.*).
template<class TemperatureParameters>
struct SimulatedAnnealingParameters
{
    TemperatureParameters temperature{};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::group<"temperature", &SimulatedAnnealingParameters::temperature>(
                "The temperature schedule"));
    }

    // The schedule is validated as a group.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return config::validation_result::success();
    }
};

namespace detail
{

// Exposes the temperature policy's parameters_type, when it has one.
template<class Policy>
struct policy_parameters
{
};

template<class Policy>
    requires requires { typename Policy::parameters_type; }
struct policy_parameters<Policy>
{
    using parameters_type =
        runners::SimulatedAnnealingParameters<typename Policy::parameters_type>;
};
template<class Acceptance, class Cost, class RNG>
concept acceptance_policy_for =
    std::uniform_random_bit_generator<RNG> &&
    requires(
        const Acceptance& acceptance,
        const Cost candidate,
        const Cost current,
        const double temperature,
        RNG& rng)
    {
        {
            acceptance.accept(candidate, current, temperature, rng)
        } -> std::convertible_to<bool>;
    };

template<class Context, class Acceptance, class RNG>
concept simulated_annealing_context =
    random_move_context<Context, RNG> &&
    strict_improvement_context<Context> &&
    acceptance_policy_for<Acceptance, typename Context::cost_type, RNG>;

template<class Context, class Acceptance, class RNG>
consteval bool validate_simulated_annealing_acceptance()
{
    static_assert(
        acceptance_policy_for<Acceptance, typename Context::cost_type, RNG>,
        "SimulatedAnnealing acceptance policy cannot consume this cost_type; "
        "MetropolisAcceptance requires candidate_cost - current_cost to be "
        "convertible to a numeric delta");
    return true;
}

} // namespace detail

template<
    temperature_policy TemperaturePolicy = temperature::Classic,
    class Acceptance = MetropolisAcceptance>
class SimulatedAnnealing
    : public detail::policy_parameters<TemperaturePolicy>
{
public:
    explicit SimulatedAnnealing(
        TemperaturePolicy temperature_policy,
        Acceptance acceptance = {})
        : temperature_policy_{std::move(temperature_policy)},
          acceptance_{std::move(acceptance)}
    {
    }

    // From its parameters, {.temperature = {...}}: the form a Runner and an
    // app hold, and build it from.
    template<class Policy = TemperaturePolicy>
        requires requires { typename Policy::parameters_type; }
    explicit SimulatedAnnealing(
        const SimulatedAnnealingParameters<typename Policy::parameters_type>& parameters,
        Acceptance acceptance = {})
        : temperature_policy_{parameters.temperature}, acceptance_{std::move(acceptance)}
    {
    }

    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::random_move_context<typename Run::context_type, RNG> &&
                 detail::strict_improvement_context<typename Run::context_type> &&
                 (!detail::acceptance_policy_for<
                     Acceptance, typename Run::cost_type, RNG>)
    [[nodiscard]]
    auto run(Run&, typename Run::solution_type, RNG&) const
    {
        static_assert(
            detail::validate_simulated_annealing_acceptance<
                typename Run::context_type, Acceptance, RNG>());
    }

    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::simulated_annealing_context<
            typename Run::context_type, Acceptance, RNG>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
        auto temperature = temperature_policy_;
        auto current = run.start(solution);
        if constexpr (calibrating_temperature_policy<TemperaturePolicy>)
            calibrate(run, temperature, solution, current, rng);
        temperature.reset();

        auto best_solution = solution;
        auto best_cost = current.cost();

        while (!temperature.finished() && !run.should_stop())
        {
            auto move = run.random_move(solution, rng);
            if (!move.has_value())
            {
                break;
            }

            run.next_iteration();
            auto candidate = run.evaluate_move(solution, current, *move);

            const auto accepted = static_cast<bool>(acceptance_.accept(
                candidate.cost(),
                current.cost(),
                temperature.temperature(),
                rng));

            if (accepted)
            {
                run.commit(solution, current, std::move(candidate), *move);

                if (run.better(current.cost(), best_cost))
                {
                    const auto previous_best = best_cost;
                    best_solution = solution;
                    best_cost = current.cost();
                    run.incumbent_updated(previous_best, best_cost);
                }
            }

            temperature.on_iteration(accepted);
        }

        return run.finish(std::move(best_solution), std::move(best_cost));
    }

private:
    // The sampled moves count as evaluations, not as iterations.
    template<class Run, class Policy, class RNG>
    static void calibrate(
        Run& run,
        Policy& temperature,
        const typename Run::solution_type& solution,
        const typename Run::evaluation_type& current,
        RNG& rng)
    {
        const auto samples = static_cast<std::size_t>(temperature.calibration_samples());
        if (samples == 0)
            return;
        if constexpr (cost::has_delta<typename Run::cost_type>)
        {
            std::vector<double> deltas;
            deltas.reserve(samples);
            for (std::size_t sample = 0; sample < samples && !run.should_stop(); ++sample)
            {
                const auto move = run.random_move(solution, rng);
                if (!move.has_value())
                    break;
                const auto candidate = run.evaluate_move(solution, current, *move);
                using cost::delta;
                deltas.push_back(
                    static_cast<double>(delta(candidate.cost(), current.cost())));
            }
            temperature.calibrate(deltas);
        }
        else
        {
            assert(false && "temperature calibration requires cost::delta");
        }
    }

    EASYLOCAL_NO_UNIQUE_ADDRESS TemperaturePolicy temperature_policy_;
    EASYLOCAL_NO_UNIQUE_ADDRESS Acceptance acceptance_;
};

template<temperature_policy TemperaturePolicy>
SimulatedAnnealing(TemperaturePolicy)
    -> SimulatedAnnealing<TemperaturePolicy, MetropolisAcceptance>;

template<temperature_policy TemperaturePolicy, class Acceptance>
SimulatedAnnealing(TemperaturePolicy, Acceptance)
    -> SimulatedAnnealing<TemperaturePolicy, Acceptance>;

} // namespace easylocal::runners
