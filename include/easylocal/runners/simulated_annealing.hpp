#pragma once

/// \file
/// Simulated Annealing together with its policies: temperature schedules
/// (runners::temperature) and the Metropolis acceptance criterion.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost/concepts.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/detail/throttled_clock.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/trace/events.hpp>
#include <easylocal/trace/tracer.hpp>
#include <easylocal/utils/detail/attributes.hpp>
#include <easylocal/utils/limit.hpp>

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
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::runners
{

// Temperature policies.

/// A temperature schedule of Simulated Annealing.
///
/// reset() starts it again, temperature() is the current temperature,
/// on_iteration(accepted) records a proposal and whether it was accepted (the
/// schedule cools on its own terms), and finished() ends the annealing. The
/// built-in schedules are in easylocal::runners::temperature.
template<class Policy>
concept temperature_policy =
    requires(Policy& policy, const Policy& const_policy, const bool accepted)
    {
        { policy.reset() } -> std::same_as<void>;
        { const_policy.temperature() } -> std::convertible_to<double>;
        { policy.on_iteration(accepted) } -> std::same_as<void>;
        { const_policy.finished() } -> std::convertible_to<bool>;
    };

/// A temperature policy that can estimate its initial temperature: before the
/// run Simulated Annealing evaluates calibration_samples() random moves at the
/// initial solution, without applying them, and passes their deltas to
/// calibrate(), then calls reset().
///
/// The built-in policies are calibrating.
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

[[nodiscard]]
constexpr std::size_t positive_quotient(
    const std::size_t numerator,
    const std::size_t denominator) noexcept
{
    assert(denominator != 0);
    return (std::max)(std::size_t{1}, numerator / denominator);
}

[[nodiscard]]
inline std::size_t accepted_limit(
    const std::size_t sample_limit,
    const double accepted_ratio)
{
    assert(std::isfinite(accepted_ratio));
    assert(accepted_ratio > 0.0);
    assert(accepted_ratio <= 1.0);

    return (std::max)(std::size_t{1},
        static_cast<std::size_t>(static_cast<double>(sample_limit) * accepted_ratio));
}

// The number of temperature levels of a cooling schedule: the coolings from
// initial_temperature to final_temperature. A count within rounding of an
// integer is that integer, so that 1 -> 0.001 by 0.1 is three levels.
[[nodiscard]]
inline std::size_t temperature_level_count(
    const double initial_temperature,
    const double final_temperature,
    const double cooling_rate)
{
    // The schemas of the schedules check these (cooling_fields, cooling_rule).
    assert(final_temperature > 0.0 && final_temperature < initial_temperature);
    assert(cooling_rate > 0.0 && cooling_rate < 1.0);

    const auto raw_levels =
        std::log(final_temperature / initial_temperature) / std::log(cooling_rate);

    constexpr double rounding = 1e-9;
    return (std::max)(std::size_t{1},
        static_cast<std::size_t>(
            std::ceil(raw_levels - rounding * (std::max)(1.0, raw_levels))));
}

// The fields of a cooling schedule, shared by the blocks that have one: its
// initial and final temperatures and its cooling rate.
template<class Block>
[[nodiscard]]
consteval auto cooling_fields()
{
    return config::fields(
        config::field<"initial_temperature", &Block::initial_temperature>(
            "Initial annealing temperature",
            config::range(0.0, easylocal::unlimited).open_low()),
        config::field<"final_temperature", &Block::final_temperature>(
            "Final annealing temperature",
            config::range(0.0, easylocal::unlimited).open_low()),
        config::field<"cooling_rate", &Block::cooling_rate>(
            "Multiplicative cooling factor",
            config::range(0.0, 1.0).open()));
}

// The rule between the temperatures of a cooling schedule.
[[nodiscard]]
consteval auto cooling_rule()
{
    return config::require(
        config::value<"final_temperature"> < config::value<"initial_temperature">,
        "final_temperature must be smaller than initial_temperature");
}

// The fields of the calibration of the initial temperature, shared by every
// built-in schedule.
template<class Block>
[[nodiscard]]
consteval auto calibration_fields()
{
    return config::fields(
        config::field<"calibration_samples", &Block::calibration_samples>(
            "Moves sampled to estimate the initial temperature (0: none)",
            config::range(0, easylocal::unlimited)),
        config::field<"initial_acceptance", &Block::initial_acceptance>(
            "Acceptance probability of an average worsening move at the "
            "estimated initial temperature",
            config::range(0.0, 1.0).open())
            .only_if(config::value<"calibration_samples"> > 0));
}

// What the schemas cannot say: their domains have no upper bound, and let an
// infinite value through. The message is a literal, which a validation result
// refers to.
[[nodiscard]]
constexpr config::validation_result validate_finite(
    const double value,
    const std::string_view message) noexcept
{
    if (!std::isfinite(value))
        return config::validation_result::failure(message);
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
// estimated one, kept above final_temperature / cooling_rate so that the
// schedule stays valid.
template<class Policy, class Parameters>
void calibrate_initial_temperature(
    Policy& policy,
    Parameters parameters,
    const std::span<const double> deltas)
{
    const auto estimate = estimate_temperature(deltas, parameters.initial_acceptance);
    if (!estimate.has_value())
        return;
    parameters.initial_temperature =
        (std::max)(*estimate, parameters.final_temperature / parameters.cooling_rate);
    policy = Policy{parameters};
}

} // namespace detail

namespace temperature
{

/// The parameters of the Classic schedule.
struct ClassicParameters
{
    /// The temperature the annealing starts from.
    double initial_temperature{10.0};
    /// The temperature the cooling stops at.
    double final_temperature{0.01};
    /// The factor that multiplies the temperature at each cooling.
    double cooling_rate{0.95};
    /// Proposals evaluated at each temperature.
    std::size_t samples_per_temperature{100};

    /// Moves sampled at the initial solution to estimate the initial
    /// temperature; 0 keeps initial_temperature.
    std::size_t calibration_samples{0};
    /// Acceptance probability of an average worsening move at the estimated
    /// initial temperature.
    double initial_acceptance{0.5};
    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return std::tuple_cat(
            detail::cooling_fields<ClassicParameters>(),
            config::fields(
                config::field<
                    "samples_per_temperature",
                    &ClassicParameters::samples_per_temperature>(
                    "Proposals evaluated at each temperature",
                    config::range(1, easylocal::unlimited))),
            detail::calibration_fields<ClassicParameters>(),
            config::fields(detail::cooling_rule()));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        if (const auto schema = config::check_schema(*this); !schema)
            return schema;
        if (const auto finite = detail::validate_finite(
                initial_temperature,
                "initial_temperature must be finite");
            !finite)
            return finite;
        return config::validation_result::success();
    }
};

/// The classic geometric schedule: samples_per_temperature proposals at each
/// temperature, which is then multiplied by cooling_rate.
///
/// The annealing ends when the temperature reaches final_temperature: after
/// the levels from initial_temperature to it, counted once, so that rounding
/// does not add one.
class Classic
{
public:
    /// The parameter block of the policy.
    using parameters_type = ClassicParameters;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit Classic(const ClassicParameters& parameters)
        : parameters_{config::require_valid(parameters)},
          temperature_levels_{detail::temperature_level_count(
              parameters.initial_temperature,
              parameters.final_temperature,
              parameters.cooling_rate)}
    {
        reset();
    }

    /// The parameters.
    [[nodiscard]]
    const ClassicParameters& parameters() const noexcept
    {
        return parameters_;
    }

    /// The moves sampled to estimate the initial temperature; 0 skips the
    /// calibration.
    [[nodiscard]]
    std::size_t calibration_samples() const noexcept
    {
        return parameters_.calibration_samples;
    }

    /// Rebuilds the schedule from the initial temperature at which a worsening
    /// move of average delta is accepted with probability initial_acceptance.
    ///
    /// The estimate is kept at least final_temperature / cooling_rate; without
    /// worsening deltas nothing changes.
    void calibrate(const std::span<const double> deltas)
    {
        detail::calibrate_initial_temperature(*this, parameters_, deltas);
    }

    /// Restarts from initial_temperature, at the start of a level.
    void reset() noexcept
    {
        temperature_ = parameters_.initial_temperature;
        sampled_ = 0;
        completed_levels_ = 0;
    }

    /// The current temperature.
    [[nodiscard]]
    double temperature() const noexcept
    {
        return temperature_;
    }

    /// Counts the proposal, and multiplies the temperature by cooling_rate
    /// after samples_per_temperature of them.
    void on_iteration(const bool) noexcept
    {
        ++sampled_;
        if (sampled_ >= parameters_.samples_per_temperature)
        {
            temperature_ *= parameters_.cooling_rate;
            sampled_ = 0;
            ++completed_levels_;
        }
    }

    /// Whether the temperature has reached final_temperature: every level has
    /// been completed.
    [[nodiscard]]
    bool finished() const noexcept
    {
        return completed_levels_ >= temperature_levels_;
    }

private:
    ClassicParameters parameters_;
    std::size_t temperature_levels_{};
    double temperature_{};
    std::size_t sampled_{};
    std::size_t completed_levels_{};
};

/// The parameters of the FixedLength schedule.
struct FixedLengthParameters
{
    /// The temperature the annealing starts from.
    double initial_temperature{10.0};
    /// The temperature the cooling stops at.
    double final_temperature{0.01};
    /// The factor that multiplies the temperature at each cooling.
    double cooling_rate{0.95};
    /// Proposals in all, spread over the temperature levels.
    std::size_t allowed_iterations{100'000};

    /// Moves sampled at the initial solution to estimate the initial
    /// temperature; 0 keeps initial_temperature.
    std::size_t calibration_samples{0};
    /// Acceptance probability of an average worsening move at the estimated
    /// initial temperature.
    double initial_acceptance{0.5};
    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return std::tuple_cat(
            detail::cooling_fields<FixedLengthParameters>(),
            config::fields(
                config::field<
                    "allowed_iterations",
                    &FixedLengthParameters::allowed_iterations>(
                    "Proposals of the annealing, spread over its levels",
                    config::range(1, easylocal::unlimited))),
            detail::calibration_fields<FixedLengthParameters>(),
            config::fields(detail::cooling_rule()));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        if (const auto schema = config::check_schema(*this); !schema)
            return schema;
        return detail::validate_finite(
            initial_temperature,
            "initial_temperature must be finite");
    }
};

/// A geometric schedule with a budget of allowed_iterations proposals, spread
/// evenly over the temperature levels from initial_temperature to
/// final_temperature.
///
/// The annealing ends when the budget is spent.
class FixedLength
{
public:
    /// The parameter block of the policy.
    using parameters_type = FixedLengthParameters;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit FixedLength(const FixedLengthParameters& parameters)
        : parameters_{config::require_valid(parameters)},
          samples_per_temperature_{detail::positive_quotient(
              parameters.allowed_iterations,
              detail::temperature_level_count(
                  parameters.initial_temperature,
                  parameters.final_temperature,
                  parameters.cooling_rate))}
    {
        reset();
    }

    /// The parameters.
    [[nodiscard]]
    const FixedLengthParameters& parameters() const noexcept
    {
        return parameters_;
    }

    /// The moves sampled to estimate the initial temperature; 0 skips the
    /// calibration.
    [[nodiscard]]
    std::size_t calibration_samples() const noexcept
    {
        return parameters_.calibration_samples;
    }

    /// Rebuilds the schedule from the initial temperature at which a worsening
    /// move of average delta is accepted with probability initial_acceptance.
    ///
    /// The estimate is kept at least final_temperature / cooling_rate; without
    /// worsening deltas nothing changes.
    void calibrate(const std::span<const double> deltas)
    {
        detail::calibrate_initial_temperature(*this, parameters_, deltas);
    }

    /// Restarts from initial_temperature, with the whole budget.
    void reset() noexcept
    {
        temperature_ = parameters_.initial_temperature;
        iterations_ = 0;
        sampled_ = 0;
    }

    /// The current temperature.
    [[nodiscard]]
    double temperature() const noexcept
    {
        return temperature_;
    }

    /// Counts the proposal, and multiplies the temperature by cooling_rate
    /// after samples_per_temperature() of them.
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

    /// Whether allowed_iterations proposals have been made.
    [[nodiscard]]
    bool finished() const noexcept
    {
        return iterations_ >= parameters_.allowed_iterations;
    }

    /// The proposals of each temperature level: allowed_iterations over the
    /// number of levels, at least 1.
    [[nodiscard]]
    std::size_t samples_per_temperature() const noexcept
    {
        return samples_per_temperature_;
    }

private:
    FixedLengthParameters parameters_;
    std::size_t samples_per_temperature_{};
    double temperature_{};
    std::size_t iterations_{};
    std::size_t sampled_{};
};

/// The parameters of the Cutoff and Hybrid schedules.
struct CutoffParameters
{
    /// The temperature the annealing starts from.
    double initial_temperature{10.0};
    /// The temperature the cooling stops at.
    double final_temperature{0.01};
    /// The factor that multiplies the temperature at each cooling.
    double cooling_rate{0.95};
    /// Proposals in all, spread over the temperature levels.
    std::size_t allowed_iterations{100'000};
    /// Accepted proposals that cool, as a share of a level's proposals
    /// (allowed_iterations over the temperature levels).
    double accepted_ratio{0.1};

    /// Moves sampled at the initial solution to estimate the initial
    /// temperature; 0 keeps initial_temperature.
    std::size_t calibration_samples{0};
    /// Acceptance probability of an average worsening move at the estimated
    /// initial temperature.
    double initial_acceptance{0.5};
    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return std::tuple_cat(
            detail::cooling_fields<CutoffParameters>(),
            config::fields(
                config::field<
                    "allowed_iterations",
                    &CutoffParameters::allowed_iterations>(
                    "Proposals of the annealing, spread over its levels",
                    config::range(1, easylocal::unlimited)),
                config::field<"accepted_ratio", &CutoffParameters::accepted_ratio>(
                    "Fraction of accepted proposals that triggers cooling",
                    config::range(0.0, 1.0).open_low())),
            detail::calibration_fields<CutoffParameters>(),
            config::fields(detail::cooling_rule()));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        if (const auto schema = config::check_schema(*this); !schema)
            return schema;
        return detail::validate_finite(
            initial_temperature,
            "initial_temperature must be finite");
    }
};

/// A schedule with a budget of allowed_iterations proposals that cools on
/// acceptances only: after accepted_ratio times a level's share of the budget
/// (allowed_iterations over the temperature levels) has been accepted.
///
/// The annealing ends when the budget is spent.
class Cutoff
{
public:
    /// The parameter block of the policy.
    using parameters_type = CutoffParameters;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit Cutoff(const CutoffParameters& parameters)
        : parameters_{config::require_valid(parameters)},
          // The proposals of a level, were the iterations shared evenly.
          accepted_limit_{detail::accepted_limit(
              detail::positive_quotient(
                  parameters.allowed_iterations,
                  detail::temperature_level_count(
                      parameters.initial_temperature,
                      parameters.final_temperature,
                      parameters.cooling_rate)),
              parameters.accepted_ratio)}
    {
        reset();
    }

    /// The parameters.
    [[nodiscard]]
    const CutoffParameters& parameters() const noexcept
    {
        return parameters_;
    }

    /// The moves sampled to estimate the initial temperature; 0 skips the
    /// calibration.
    [[nodiscard]]
    std::size_t calibration_samples() const noexcept
    {
        return parameters_.calibration_samples;
    }

    /// Rebuilds the schedule from the initial temperature at which a worsening
    /// move of average delta is accepted with probability initial_acceptance.
    ///
    /// The estimate is kept at least final_temperature / cooling_rate; without
    /// worsening deltas nothing changes.
    void calibrate(const std::span<const double> deltas)
    {
        detail::calibrate_initial_temperature(*this, parameters_, deltas);
    }

    /// Restarts from initial_temperature, with the whole budget.
    void reset() noexcept
    {
        temperature_ = parameters_.initial_temperature;
        iterations_ = 0;
        accepted_ = 0;
    }

    /// The current temperature.
    [[nodiscard]]
    double temperature() const noexcept
    {
        return temperature_;
    }

    /// Counts the proposal, and multiplies the temperature by cooling_rate
    /// after accepted_limit() acceptances at the same temperature.
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

    /// Whether allowed_iterations proposals have been made.
    [[nodiscard]]
    bool finished() const noexcept
    {
        return iterations_ >= parameters_.allowed_iterations;
    }

    /// The acceptances that cool: accepted_ratio times a level's share of
    /// allowed_iterations, at least 1.
    [[nodiscard]]
    std::size_t accepted_limit() const noexcept
    {
        return accepted_limit_;
    }

private:
    CutoffParameters parameters_;
    std::size_t accepted_limit_{};
    double temperature_{};
    std::size_t iterations_{};
    std::size_t accepted_{};
};

/// The parameters of the Hybrid schedule, those of Cutoff.
using HybridParameters = CutoffParameters;

/// The schedule of EasyLocal 3: a level ends after its share of
/// allowed_iterations proposals or, earlier, after accepted_ratio of them have
/// been accepted; the proposals an early cooling saves are spread over the
/// remaining levels.
///
/// The annealing ends when allowed_iterations proposals are spent.
class Hybrid
{
public:
    /// The parameter block of the policy.
    using parameters_type = HybridParameters;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit Hybrid(const HybridParameters& parameters)
        : parameters_{config::require_valid(parameters)},
          temperature_levels_{detail::temperature_level_count(
              parameters.initial_temperature,
              parameters.final_temperature,
              parameters.cooling_rate)},
          initial_sample_limit_{detail::positive_quotient(
              parameters.allowed_iterations,
              temperature_levels_)},
          accepted_limit_{
              detail::accepted_limit(initial_sample_limit_, parameters.accepted_ratio)}
    {
        reset();
    }

    /// The parameters.
    [[nodiscard]]
    const HybridParameters& parameters() const noexcept
    {
        return parameters_;
    }

    /// The moves sampled to estimate the initial temperature; 0 skips the
    /// calibration.
    [[nodiscard]]
    std::size_t calibration_samples() const noexcept
    {
        return parameters_.calibration_samples;
    }

    /// Rebuilds the schedule from the initial temperature at which a worsening
    /// move of average delta is accepted with probability initial_acceptance.
    ///
    /// The estimate is kept at least final_temperature / cooling_rate; without
    /// worsening deltas nothing changes.
    void calibrate(const std::span<const double> deltas)
    {
        detail::calibrate_initial_temperature(*this, parameters_, deltas);
    }

    /// Restarts from initial_temperature, with the whole budget spread evenly
    /// over the levels.
    void reset() noexcept
    {
        temperature_ = parameters_.initial_temperature;
        iterations_ = 0;
        sampled_ = 0;
        accepted_ = 0;
        completed_levels_ = 0;
        current_sample_limit_ = initial_sample_limit_;
    }

    /// The current temperature.
    [[nodiscard]]
    double temperature() const noexcept
    {
        return temperature_;
    }

    /// Counts the proposal, and multiplies the temperature by cooling_rate
    /// after samples_per_temperature() proposals or accepted_limit()
    /// acceptances at the same temperature.
    ///
    /// After an early cooling by acceptances, the proposals left are spread
    /// evenly over the remaining levels.
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
                    parameters_.allowed_iterations - iterations_;
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

    /// Whether allowed_iterations proposals have been made.
    [[nodiscard]]
    bool finished() const noexcept
    {
        return iterations_ >= parameters_.allowed_iterations;
    }

    /// The proposals of the current level: allowed_iterations over the levels
    /// or, after an early cooling, the proposals left over the remaining
    /// levels.
    [[nodiscard]]
    std::size_t samples_per_temperature() const noexcept
    {
        return current_sample_limit_;
    }

    /// The acceptances that end a level early: accepted_ratio times a level's
    /// initial share of allowed_iterations, at least 1.
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

/// The parameters of the FixedTemperature schedule.
struct FixedTemperatureParameters
{
    /// The constant temperature.
    double temperature{1.0};
    /// Proposals in all.
    std::size_t allowed_iterations{100'000};
    /// Accepted proposals after which the annealing ends; unlimited by default.
    limit max_accepted{unlimited};

    /// Moves sampled at the initial solution to estimate the temperature; 0
    /// keeps temperature.
    std::size_t calibration_samples{0};
    /// Acceptance probability of an average worsening move at the estimated
    /// initial temperature.
    double initial_acceptance{0.5};
    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return std::tuple_cat(
            config::fields(
                config::field<"temperature", &FixedTemperatureParameters::temperature>(
                    "Constant annealing temperature",
                    config::range(0.0, easylocal::unlimited).open_low()),
                config::field<
                    "allowed_iterations",
                    &FixedTemperatureParameters::allowed_iterations>(
                    "Proposals of the annealing",
                    config::range(1, easylocal::unlimited)),
                config::field<"max_accepted", &FixedTemperatureParameters::max_accepted>(
                    "Accepted proposals that end the annealing, or unlimited",
                    config::range(1, easylocal::unlimited))),
            detail::calibration_fields<FixedTemperatureParameters>());
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        if (const auto schema = config::check_schema(*this); !schema)
            return schema;
        return detail::validate_finite(temperature, "temperature must be finite");
    }
};

/// A constant temperature: the search ends after allowed_iterations proposals,
/// or earlier once max_accepted of them have been accepted.
class FixedTemperature
{
public:
    /// The parameter block of the policy.
    using parameters_type = FixedTemperatureParameters;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit FixedTemperature(const FixedTemperatureParameters& parameters)
        : parameters_{config::require_valid(parameters)}
    {
        reset();
    }

    /// The parameters.
    [[nodiscard]]
    const FixedTemperatureParameters& parameters() const noexcept
    {
        return parameters_;
    }

    /// The moves sampled to estimate the initial temperature; 0 skips the
    /// calibration.
    [[nodiscard]]
    std::size_t calibration_samples() const noexcept
    {
        return parameters_.calibration_samples;
    }

    /// Sets temperature to the one at which a worsening move of average delta
    /// is accepted with probability initial_acceptance, and restarts.
    ///
    /// Without worsening deltas nothing changes.
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

    /// Clears the counts of proposals and acceptances.
    void reset() noexcept
    {
        iterations_ = 0;
        accepted_ = 0;
    }

    /// The constant temperature.
    [[nodiscard]]
    double temperature() const noexcept
    {
        return parameters_.temperature;
    }

    /// Counts the proposal, and the acceptance when it was accepted.
    void on_iteration(const bool accepted) noexcept
    {
        assert(!finished());
        ++iterations_;
        accepted_ += accepted ? 1U : 0U;
    }

    /// Whether allowed_iterations proposals or max_accepted acceptances have
    /// been made.
    [[nodiscard]]
    bool finished() const noexcept
    {
        return iterations_ >= parameters_.allowed_iterations
            || accepted_ >= parameters_.max_accepted;
    }

private:
    FixedTemperatureParameters parameters_;
    std::size_t iterations_{};
    std::size_t accepted_{};
};

/// The parameters of the TimeBased schedule.
struct TimeBasedParameters
{
    /// The temperature the annealing starts from.
    double initial_temperature{10.0};
    /// The temperature the cooling stops at.
    double final_temperature{0.01};
    /// The factor that multiplies the temperature at each cooling.
    double cooling_rate{0.95};
    /// Seconds.
    double allowed_running_time{10.0};
    /// Accepted proposals that cool early; unlimited cools only on time.
    limit accepted_per_temperature{unlimited};

    /// Moves sampled at the initial solution to estimate the initial
    /// temperature; 0 keeps initial_temperature.
    std::size_t calibration_samples{0};
    /// Acceptance probability of an average worsening move at the estimated
    /// initial temperature.
    double initial_acceptance{0.5};
    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return std::tuple_cat(
            detail::cooling_fields<TimeBasedParameters>(),
            config::fields(
                config::field<
                    "allowed_running_time",
                    &TimeBasedParameters::allowed_running_time>(
                    "Running time of the annealing, in seconds",
                    config::range(0.0, easylocal::unlimited).open_low()),
                config::field<
                    "accepted_per_temperature",
                    &TimeBasedParameters::accepted_per_temperature>(
                    "Accepted proposals that trigger cooling (unlimited: cool only on time)",
                    config::range(1, easylocal::unlimited))),
            detail::calibration_fields<TimeBasedParameters>(),
            config::fields(detail::cooling_rule()));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        if (const auto schema = config::check_schema(*this); !schema)
            return schema;
        if (const auto finite = detail::validate_finite(
                initial_temperature,
                "initial_temperature must be finite");
            !finite)
            return finite;
        return detail::validate_finite(
            allowed_running_time,
            "allowed_running_time must be finite");
    }
};

/// The cooling schedule spread over a running time instead of an iteration
/// budget: the allowed time is divided evenly among the temperature levels, and
/// the temperature cools when the time of its level is over or, with
/// accepted_per_temperature, after that many acceptances; the time an early
/// cooling saves is redistributed over the remaining levels.
///
/// The annealing ends when the time is over or the final temperature is
/// reached. The trajectory depends on the speed of the machine, so equal seeds
/// do not give equal runs. The clock is read at an interval of proposals that
/// adapts so that readings come about a millisecond apart, as a run's time
/// limit is checked, and at each early cooling.
template<class Clock = std::chrono::steady_clock>
class BasicTimeBased
{
public:
    /// The parameter block of the policy.
    using parameters_type = TimeBasedParameters;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit BasicTimeBased(const TimeBasedParameters& parameters)
        : parameters_{config::require_valid(parameters)},
          temperature_levels_{detail::temperature_level_count(
              parameters.initial_temperature,
              parameters.final_temperature,
              parameters.cooling_rate)},
          running_time_{clock_duration(parameters.allowed_running_time)}
    {
        reset();
    }

    /// The parameters.
    [[nodiscard]]
    const TimeBasedParameters& parameters() const noexcept
    {
        return parameters_;
    }

    /// The moves sampled to estimate the initial temperature; 0 skips the
    /// calibration.
    [[nodiscard]]
    std::size_t calibration_samples() const noexcept
    {
        return parameters_.calibration_samples;
    }

    /// Rebuilds the schedule from the initial temperature at which a worsening
    /// move of average delta is accepted with probability initial_acceptance.
    ///
    /// The estimate is kept at least final_temperature / cooling_rate; without
    /// worsening deltas nothing changes.
    void calibrate(const std::span<const double> deltas)
    {
        detail::calibrate_initial_temperature(*this, parameters_, deltas);
    }

    /// Starts the clock.
    void reset() noexcept
    {
        temperature_ = parameters_.initial_temperature;
        clock_.reset();
        start_ = clock_.now();
        level_start_ = start_;
        level_time_ =
            running_time_ / static_cast<typename duration::rep>(temperature_levels_);
        completed_levels_ = 0;
        accepted_ = 0;
        timed_out_ = false;
    }

    /// The current temperature.
    [[nodiscard]]
    double temperature() const noexcept
    {
        return temperature_;
    }

    /// Multiplies the temperature by cooling_rate when the time of the level
    /// is over or after accepted_per_temperature acceptances at it.
    ///
    /// The clock is read when its interval of proposals is due, and at an
    /// early cooling. An early cooling spreads the time left over the remaining
    /// levels; once allowed_running_time is over the annealing is finished.
    void on_iteration(const bool accepted) noexcept
    {
        assert(!finished());
        accepted_ += accepted ? 1U : 0U;
        const auto accepted_cutoff = accepted_ >= parameters_.accepted_per_temperature;
        const auto reading = accepted_cutoff ? clock_.now() : clock_.due();
        if (!reading)
            return;
        const auto now = *reading;
        if (now - start_ >= running_time_)
        {
            timed_out_ = true;
            return;
        }

        const auto level_elapsed = now - level_start_;
        const auto time_over = level_elapsed >= level_time_;
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

    /// Whether allowed_running_time is over or the temperature has reached
    /// final_temperature (every level has been completed).
    [[nodiscard]]
    bool finished() const noexcept
    {
        return timed_out_ || completed_levels_ >= temperature_levels_;
    }

    /// The time of the current level: allowed_running_time over the levels or,
    /// after an early cooling, the time left over the remaining levels.
    [[nodiscard]]
    typename Clock::duration level_time() const noexcept
    {
        return level_time_;
    }

private:
    using duration = typename Clock::duration;

    // Seconds as the clock's duration, at most the longest it holds.
    [[nodiscard]]
    static duration clock_duration(const double seconds) noexcept
    {
        const std::chrono::duration<double> time{seconds};
        if (time >= std::chrono::duration<double>{(duration::max)()})
            return (duration::max)();
        return std::chrono::duration_cast<duration>(time);
    }

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
    easylocal::detail::throttled_clock<Clock> clock_;
};

/// The TimeBased schedule, timed by `std::chrono::steady_clock`.
using TimeBased = BasicTimeBased<>;

} // namespace temperature

namespace detail
{

// The budget of a schedule that Reheating divides among its descents: an
// iteration budget or a running time.
template<class Parameters>
concept iteration_budget = requires(Parameters parameters) {
    { parameters.allowed_iterations } -> std::convertible_to<std::size_t>;
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

/// The parameters of Reheating: those of its schedule, as the group descent,
/// and of the reheats.
template<class DescentParameters>
struct ReheatingParameters
{
    /// The schedule of the descents; the reheats restart it from a lower
    /// initial temperature.
    DescentParameters descent{};
    /// Descents after the first one: the size of the schedule, not a limit.
    std::size_t allowed_reheats{3};
    /// The temperature a reheat restarts from, as a factor of the descent's
    /// initial_temperature.
    double reheat_ratio{0.5};
    /// The share of the descent's budget (allowed_iterations or
    /// allowed_running_time) spent by the first descent; the reheats divide the
    /// rest evenly.
    ///
    /// Ignored by a schedule without a budget.
    double first_descent_share{0.5};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        // Only a descent with an iteration or time budget shares it.
        constexpr bool budgeted = detail::iteration_budget<DescentParameters>
            || detail::time_budget<DescentParameters>;
        const auto fields = config::fields(
            config::group<"descent", &ReheatingParameters::descent>(
                "The schedule of each descent"),
            config::field<"allowed_reheats", &ReheatingParameters::allowed_reheats>(
                "Number of reheats after the first descent",
                config::range(0, easylocal::unlimited)),
            config::field<"reheat_ratio", &ReheatingParameters::reheat_ratio>(
                "Restart temperature of a reheat, as a factor of the initial one",
                config::range(0.0, easylocal::unlimited).open_low())
                .only_if(config::value<"allowed_reheats"> > 0),
            config::field<
                "first_descent_share",
                &ReheatingParameters::first_descent_share>(
                "Share of the budget spent by the first descent",
                config::range(0.0, 1.0).open())
                .only_if(config::value<"allowed_reheats"> > 0 && budgeted));
        // A reheat restarts above the final temperature.
        const auto above_final = [&] {
            if constexpr (detail::final_temperature_schedule<DescentParameters>)
            {
                const auto rule = config::require(
                    config::value<"allowed_reheats"> == 0
                        || config::value<"descent.initial_temperature">
                                * config::value<"reheat_ratio"> > config::value<
                               "descent.final_temperature">,
                    "reheat_ratio must keep the reheat temperature above "
                    "final_temperature");
                return std::tuple_cat(fields, std::tuple<decltype(rule)>{rule});
            }
            else
            {
                return fields;
            }
        }();
        if constexpr (detail::iteration_budget<DescentParameters>)
        {
            // The first descent spends ceil(allowed_iterations * share), and
            // each reheat at least one of the rest.
            const auto rest = config::require(
                config::value<"allowed_reheats"> == 0
                    || config::value<"descent.allowed_iterations">
                                * config::value<"first_descent_share">
                            + config::value<"allowed_reheats">
                        <= config::value<"descent.allowed_iterations">,
                "allowed_reheats must not exceed the iterations the first descent leaves");
            return std::tuple_cat(above_final, std::tuple<decltype(rest)>{rest});
        }
        else
        {
            return above_final;
        }
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        if (const auto schema = config::check_schema(*this); !schema)
            return schema;
        const auto schedule = descent.validate();
        if (!schedule)
            return schedule;
        if (allowed_reheats == 0)
            return config::validation_result::success();
        // Its domain has no upper bound: it lets infinity through.
        if (!std::isfinite(reheat_ratio))
            return config::validation_result::failure("reheat_ratio must be finite");
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

/// Reheats any schedule with an initial temperature: a first descent, then up
/// to allowed_reheats descents restarting from reheat_ratio times the initial
/// temperature.
///
/// When the schedule has a budget, allowed_iterations or allowed_running_time,
/// the first descent spends first_descent_share of it and the reheats divide
/// the rest evenly; otherwise each descent runs the whole schedule. It
/// calibrates when the schedule does, and the reheat temperature stays above
/// the final one. `Reheating<Hybrid>` is EasyLocal 3's annealing with
/// reheating.
template<detail::reheatable_policy Descent>
class Reheating
{
public:
    /// The parameter block of the reheated schedule.
    using descent_parameters_type = typename Descent::parameters_type;
    /// The parameter block of the policy.
    using parameters_type = ReheatingParameters<descent_parameters_type>;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit Reheating(const parameters_type parameters)
        : parameters_{config::require_valid(parameters)},
          descent_{first_descent(parameters)}
    {
    }

    /// The parameters.
    [[nodiscard]]
    const parameters_type& parameters() const noexcept
    {
        return parameters_;
    }

    /// The moves the schedule samples to estimate its initial temperature.
    [[nodiscard]]
    std::size_t calibration_samples() const noexcept
        requires calibrating_temperature_policy<Descent>
    {
        return descent_.calibration_samples();
    }

    /// The schedule's estimate of the initial temperature, kept high enough
    /// for the reheats.
    void calibrate(const std::span<const double> deltas)
        requires calibrating_temperature_policy<Descent>
    {
        Descent probe{first_descent(parameters_)};
        probe.calibrate(deltas);
        parameters_.descent.initial_temperature =
            (std::max)(probe.parameters().initial_temperature,
                lowest_initial_temperature());
        reset();
    }

    /// Restarts with the first descent, and no reheat done.
    void reset()
    {
        descent_ = Descent{first_descent(parameters_)};
        reheats_ = 0;
    }

    /// The temperature of the descent under way.
    [[nodiscard]]
    double temperature() const noexcept
    {
        return descent_.temperature();
    }

    /// Passes the proposal to the descent under way, and starts a reheat when
    /// it finishes and fewer than allowed_reheats have been done.
    void on_iteration(const bool accepted)
    {
        assert(!finished());
        descent_.on_iteration(accepted);
        if (descent_.finished() && reheats_ < parameters_.allowed_reheats)
        {
            descent_ = Descent{reheat_descent(parameters_)};
            ++reheats_;
        }
    }

    /// Whether the last descent, after allowed_reheats reheats, has finished.
    [[nodiscard]]
    bool finished() const noexcept
    {
        return reheats_ >= parameters_.allowed_reheats && descent_.finished();
    }

    /// The reheats done so far.
    [[nodiscard]]
    std::size_t reheats() const noexcept
    {
        return reheats_;
    }

    /// The descent under way.
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
            const auto reheat_factor = parameters_.allowed_reheats == 0
                ? 1.0
                : (std::min)(1.0, parameters_.reheat_ratio);
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
        if (parameters.allowed_reheats == 0)
            return descent;
        if constexpr (detail::iteration_budget<descent_parameters_type>)
        {
            descent.allowed_iterations = (std::max)(std::size_t{1},
                static_cast<std::size_t>(std::ceil(
                    static_cast<double>(parameters.descent.allowed_iterations)
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
            const auto first = first_descent(parameters).allowed_iterations;
            const auto total = parameters.descent.allowed_iterations;
            descent.allowed_iterations = detail::positive_quotient(
                total > first ? total - first : 0,
                parameters.allowed_reheats);
        }
        else if constexpr (detail::time_budget<descent_parameters_type>)
        {
            descent.allowed_running_time = parameters.descent.allowed_running_time
                * (1.0 - parameters.first_descent_share)
                / static_cast<double>(parameters.allowed_reheats);
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


} // namespace detail

/// An acceptance criterion of Simulated Annealing: accept(candidate, current,
/// temperature, rng) tells whether a move of cost candidate is accepted from a
/// solution of cost current at temperature, drawing from rng if it needs to.
template<class Acceptance, class Cost, class RNG>
concept acceptance_policy_for = std::uniform_random_bit_generator<RNG>
    && requires(
        const Acceptance& acceptance,
        const Cost candidate,
        const Cost current,
        const double temperature,
        RNG& rng) {
           {
               acceptance.accept(candidate, current, temperature, rng)
           } -> std::convertible_to<bool>;
       };

/// The Metropolis criterion: a move that does not worsen the cost is accepted,
/// a worsening one with probability exp(-delta / temperature).
///
/// It requires costs with a numeric difference, cost::delta (cost::has_delta);
/// an infinite delta, a worsening of a hierarchical hard level, is never
/// accepted.
struct MetropolisAcceptance
{
    /// Whether candidate is accepted over current at temperature.
    template<cost::has_delta Cost, std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    bool accept(
        const Cost& candidate,
        const Cost& current,
        const double temperature,
        RNG& rng) const
    {
        assert(std::isfinite(temperature));
        assert(temperature > 0.0);

        // In double: long double arithmetic, software on aarch64 Linux and
        // x87 on x86-64, costs several times more and dominated cheap moves,
        // and the probability is compared with a double anyway.
        using cost::delta;
        const auto difference = static_cast<double>(delta(candidate, current));
        assert(!std::isnan(difference));

        if (difference <= 0.0)
        {
            return true;
        }
        if (std::isinf(difference))
        {
            return false;
        }

        const auto probability = std::exp(-difference / temperature);
        std::uniform_real_distribution<double> draw{0.0, 1.0};
        return draw(rng) < probability;
    }
};

// Algorithm.

/// The parameters of Simulated Annealing: its temperature policy's, as the
/// group "temperature" (paths temperature.*), and its evaluation budget.
template<class TemperatureParameters>
struct SimulatedAnnealingParameters
{
    /// The parameters of the temperature schedule.
    TemperatureParameters temperature{};
    /// Evaluation budget, including the initial evaluation; unlimited by
    /// default.
    limit max_evaluations{unlimited};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::group<"temperature", &SimulatedAnnealingParameters::temperature>(
                "The temperature schedule"),
            config::field<
                "max_evaluations",
                &SimulatedAnnealingParameters::max_evaluations>(
                "Maximum number of solution evaluations, or unlimited",
                config::range(0, easylocal::unlimited)));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
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
        "the acceptance policy of SimulatedAnnealing cannot compare this cost: "
        "MetropolisAcceptance requires a cost with a numeric cost::delta, an "
        "arithmetic cost, a cost::hierarchical with an arithmetic soft level, or "
        "a cost type with a delta(candidate, current) found by ADL");
    return true;
}

} // namespace detail

/// Simulated Annealing: at each iteration a random move is proposed and
/// accepted by the acceptance policy at the temperature of the schedule (by
/// default, the Metropolis criterion on the classic geometric schedule).
///
/// The annealing ends when the schedule finishes, and returns the best
/// solution found. A calibrating schedule estimates its initial temperature
/// first, from random moves evaluated at the initial solution. Its
/// parameters_type, for a schedule with parameters P, is
/// `SimulatedAnnealingParameters<P>`. Requires a neighborhood explorer with
/// random_move(), a cost with better(), and a cost the acceptance policy can
/// compare (MetropolisAcceptance: cost::delta).
template<
    temperature_policy TemperaturePolicy = temperature::Classic,
    class Acceptance = MetropolisAcceptance>
class SimulatedAnnealing
    : public detail::policy_parameters<TemperaturePolicy>
{
public:
    /// From a temperature schedule and an acceptance policy.
    explicit SimulatedAnnealing(
        TemperaturePolicy temperature_policy,
        Acceptance acceptance = {})
        : temperature_policy_{std::move(temperature_policy)},
          acceptance_{std::move(acceptance)}
    {
    }

    /// From its parameters, {.temperature = {...}}: the form a Runner and an
    /// app hold, and build it from.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    template<class Policy = TemperaturePolicy>
        requires requires { typename Policy::parameters_type; }
    explicit SimulatedAnnealing(
        const SimulatedAnnealingParameters<typename Policy::parameters_type>& parameters,
        Acceptance acceptance = {})
        : temperature_policy_{config::require_valid(parameters).temperature},
          acceptance_{std::move(acceptance)},
          max_evaluations_{parameters.max_evaluations}
    {
    }

    /// Rejects, with a readable message, a cost the acceptance policy cannot
    /// compare.
    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::random_move_context<typename Run::context_type, RNG>
        && detail::strict_improvement_context<typename Run::context_type>
        && (!acceptance_policy_for<Acceptance, typename Run::cost_type, RNG>)
    [[nodiscard]]
    typename Run::result_type run(Run&, typename Run::solution_type, RNG&) const
    {
        static_assert(
            detail::validate_simulated_annealing_acceptance<
                typename Run::context_type, Acceptance, RNG>());
        std::unreachable();
    }

    /// Runs the search from solution, drawing random moves and acceptances with
    /// rng.
    ///
    /// The bound runner calls it, with the run of its context (neighborhood,
    /// evaluation, cost relations).
    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::simulated_annealing_context<
            typename Run::context_type, Acceptance, RNG>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
        static_assert(
            detail::delta_agrees_with_better<typename Run::context_type>(),
            "Simulated Annealing decides with the sign of cost::delta, which"
            " must agree with better(): the compare of the cost expression "
            "finds a larger cost better");
        run.limit_evaluations(max_evaluations_);
        auto temperature = temperature_policy_;
        auto current = run.start(solution);
        if constexpr (calibrating_temperature_policy<TemperaturePolicy>)
            calibrate(run, temperature, solution, current, rng);
        temperature.reset();
        // The temperature as the trace last saw it, for a tracer that
        // observes its changes.
        constexpr bool traces_temperature =
            trace::observes<typename Run::tracer_type, trace::event::temperature_changed>;
        [[maybe_unused]] double traced_temperature = 0.0;
        if constexpr (traces_temperature)
            traced_temperature = report_temperature(run, 0.0, temperature.temperature());

        best_so_far best{solution, current.cost()};

        while (!temperature.finished() && !run.should_stop())
        {
            auto move = run.random_move(solution, rng);
            if (!move.has_value())
            {
                // No move to try: as Hill Climbing, a local optimum.
                return run.finish(
                    std::move(best.solution),
                    std::move(best.cost),
                    termination_reason::local_optimum);
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

                best.update(run, solution, current);
            }

            temperature.on_iteration(accepted);
            if constexpr (traces_temperature)
            {
                if (temperature.temperature() != traced_temperature)
                {
                    traced_temperature = report_temperature(
                        run,
                        traced_temperature,
                        temperature.temperature());
                }
            }
        }

        return run.finish(std::move(best.solution), std::move(best.cost));
    }

private:
    // Sends the change of temperature to the trace; the new temperature.
    template<class Run>
    static double report_temperature(Run& run, const double previous, const double now)
    {
        run.emit(
            trace::event::temperature_changed{
                .evaluations = run.evaluations(),
                .iterations = run.iterations(),
                .previous_temperature = previous,
                .temperature = now,
            });
        return now;
    }

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
            throw std::invalid_argument{
                "temperature calibration (calibration_samples above 0) requires a cost "
                "with cost::delta"};
        }
    }

    EASYLOCAL_NO_UNIQUE_ADDRESS TemperaturePolicy temperature_policy_;
    EASYLOCAL_NO_UNIQUE_ADDRESS Acceptance acceptance_;
    limit max_evaluations_{unlimited};
};

/// Deduces the algorithm from its temperature policy, with the Metropolis
/// acceptance.
template<temperature_policy TemperaturePolicy>
SimulatedAnnealing(TemperaturePolicy)
    -> SimulatedAnnealing<TemperaturePolicy, MetropolisAcceptance>;

/// Deduces the algorithm from its temperature policy and acceptance criterion.
template<temperature_policy TemperaturePolicy, class Acceptance>
SimulatedAnnealing(TemperaturePolicy, Acceptance)
    -> SimulatedAnnealing<TemperaturePolicy, Acceptance>;

} // namespace easylocal::runners
