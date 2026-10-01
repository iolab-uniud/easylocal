#pragma once

#include <easylocal/config/tree.hpp>
#include <easylocal/cost/concepts.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/search_run.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <optional>
#include <random>
#include <tuple>
#include <type_traits>
#include <utility>

// Simulated Annealing together with its policies: temperature schedules
// (runners::temperature) and the Metropolis acceptance criterion.
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

[[nodiscard]]
inline auto validate_cooling_schedule(
    const double initial_temperature,
    const double final_temperature,
    const double cooling_rate) noexcept -> config::validation_result
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

// configure() for policies whose derived state is computed by the
// constructor: validate, then rebuild from the new parameters.
template<class Policy, class Parameters>
[[nodiscard]]
auto reconfigure(Policy& policy, const Parameters& parameters) noexcept
    -> config::validation_result
{
    const auto validation = parameters.validate();
    if (!validation)
    {
        return validation;
    }
    policy = Policy{parameters};
    return config::validation_result::success();
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
            config::field<"samples_per_temperature", &ClassicParameters::samples_per_temperature>(
                "Proposals evaluated at each temperature"));
    }

    [[nodiscard]]
    auto validate() const noexcept -> config::validation_result
    {
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
    auto parameters() const noexcept -> const ClassicParameters&
    {
        return parameters_;
    }

    [[nodiscard]]
    auto configure(ClassicParameters parameters) noexcept -> config::validation_result
    {
        return detail::reconfigure(*this, parameters);
    }

    [[nodiscard]]
    auto configuration() noexcept
    {
        return config::endpoint<"temperature">(*this);
    }

    [[nodiscard]]
    auto configuration() const noexcept
    {
        return config::endpoint<"temperature">(*this);
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

    [[nodiscard]]
    auto parameters() const noexcept -> const FixedLengthParameters&
    {
        return parameters_;
    }

    [[nodiscard]]
    auto configure(FixedLengthParameters parameters) noexcept
        -> config::validation_result
    {
        const auto validation = parameters.validate();
        if (!validation)
        {
            return validation;
        }

        const auto temperature_levels = detail::temperature_level_count(
            parameters.initial_temperature,
            parameters.final_temperature,
            parameters.cooling_rate);
        const auto samples_per_temperature = detail::positive_quotient(
            parameters.max_iterations,
            temperature_levels);

        parameters_ = parameters;
        temperature_levels_ = temperature_levels;
        samples_per_temperature_ = samples_per_temperature;
        reset();

        return config::validation_result::success();
    }

    [[nodiscard]]
    auto configuration() noexcept
    {
        return config::endpoint<"temperature">(*this);
    }

    [[nodiscard]]
    auto configuration() const noexcept
    {
        return config::endpoint<"temperature">(*this);
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
                "Fraction of accepted proposals that triggers cooling"));
    }

    [[nodiscard]]
    auto validate() const noexcept -> config::validation_result
    {
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
    auto parameters() const noexcept -> const CutoffParameters&
    {
        return parameters_;
    }

    [[nodiscard]]
    auto configure(CutoffParameters parameters) noexcept -> config::validation_result
    {
        return detail::reconfigure(*this, parameters);
    }

    [[nodiscard]]
    auto configuration() noexcept
    {
        return config::endpoint<"temperature">(*this);
    }

    [[nodiscard]]
    auto configuration() const noexcept
    {
        return config::endpoint<"temperature">(*this);
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

    [[nodiscard]]
    auto parameters() const noexcept -> const HybridParameters&
    {
        return parameters_;
    }

    [[nodiscard]]
    auto configure(HybridParameters parameters) noexcept -> config::validation_result
    {
        return detail::reconfigure(*this, parameters);
    }

    [[nodiscard]]
    auto configuration() noexcept
    {
        return config::endpoint<"temperature">(*this);
    }

    [[nodiscard]]
    auto configuration() const noexcept
    {
        return config::endpoint<"temperature">(*this);
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
    auto accept(
        const Cost& candidate,
        const Cost& current,
        const double temperature,
        RNG& rng) const -> bool
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

        const auto probability = std::exp(
            -difference / static_cast<long double>(temperature));
        std::uniform_real_distribution<double> draw{0.0, 1.0};
        return draw(rng) < static_cast<double>(probability);
    }
};

// Algorithm.

namespace detail
{

template<class Context, class RNG>
concept random_move_context =
    search_context<Context> &&
    std::uniform_random_bit_generator<RNG> &&
    easylocal::random_neighborhood_for<
        typename Context::neighborhood_explorer_type,
        typename Context::solution_type,
        RNG>;

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
consteval auto validate_simulated_annealing_acceptance() -> bool
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
{
public:
    explicit SimulatedAnnealing(
        TemperaturePolicy temperature_policy,
        Acceptance acceptance = {})
        : temperature_policy_{std::move(temperature_policy)},
          acceptance_{std::move(acceptance)}
    {
    }

    [[nodiscard]]
    auto configuration()
        requires (
            config::configuration_provider<TemperaturePolicy> ||
            config::configuration_provider<Acceptance>)
    {
        auto children = std::tuple_cat(
            config::configuration_nodes(temperature_policy_),
            config::configuration_nodes(acceptance_));

        return std::apply(
            [](auto... nodes) {
                return config::named<"search">(std::move(nodes)...);
            },
            std::move(children));
    }

    [[nodiscard]]
    auto configuration() const
        requires (
            config::configuration_provider<const TemperaturePolicy> ||
            config::configuration_provider<const Acceptance>)
    {
        auto children = std::tuple_cat(
            config::configuration_nodes(temperature_policy_),
            config::configuration_nodes(acceptance_));

        return std::apply(
            [](auto... nodes) {
                return config::named<"search">(std::move(nodes)...);
            },
            std::move(children));
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
        temperature.reset();

        auto current = run.start(solution);
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
    [[no_unique_address]] TemperaturePolicy temperature_policy_;
    [[no_unique_address]] Acceptance acceptance_;
};

template<temperature_policy TemperaturePolicy>
SimulatedAnnealing(TemperaturePolicy)
    -> SimulatedAnnealing<TemperaturePolicy, MetropolisAcceptance>;

template<temperature_policy TemperaturePolicy, class Acceptance>
SimulatedAnnealing(TemperaturePolicy, Acceptance)
    -> SimulatedAnnealing<TemperaturePolicy, Acceptance>;

} // namespace easylocal::runners
