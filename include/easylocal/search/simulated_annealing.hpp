#pragma once

#include <easylocal/search/detail/context_concepts.hpp>
#include <easylocal/search/metropolis_acceptance.hpp>
#include <easylocal/search/temperature_policy.hpp>
#include <easylocal/search_run.hpp>

#include <concepts>
#include <cstddef>
#include <optional>
#include <random>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::search
{

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

} // namespace easylocal::search
