#pragma once

#include <easylocal/run_control.hpp>
#include <easylocal/search/detail/context_concepts.hpp>
#include <easylocal/search/metropolis_acceptance.hpp>
#include <easylocal/search/temperature_policy.hpp>
#include <easylocal/trace.hpp>

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

template<class Solution, class Cost>
struct SimulatedAnnealingResult
{
    Solution solution;
    Cost cost;
    std::size_t iterations;
    std::size_t evaluations;
};

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

    template<class Context, std::uniform_random_bit_generator RNG>
        requires detail::random_move_context<Context, RNG> &&
                 detail::strict_improvement_context<Context> &&
                 (!detail::acceptance_policy_for<
                     Acceptance, typename Context::cost_type, RNG>)
    [[nodiscard]]
    auto run(
        const Context&,
        typename Context::solution_type,
        RNG&)
    {
        static_assert(
            detail::validate_simulated_annealing_acceptance<
                Context, Acceptance, RNG>());
    }

    template<class Context, std::uniform_random_bit_generator RNG>
        requires detail::random_move_context<Context, RNG> &&
                 detail::strict_improvement_context<Context> &&
                 (!detail::acceptance_policy_for<
                     Acceptance, typename Context::cost_type, RNG>)
    [[nodiscard]]
    auto run(
        const Context&,
        typename Context::solution_type,
        RNG&,
        const run_control&)
    {
        static_assert(
            detail::validate_simulated_annealing_acceptance<
                Context, Acceptance, RNG>());
    }

    template<class Context, std::uniform_random_bit_generator RNG>
        requires detail::simulated_annealing_context<Context, Acceptance, RNG>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution,
        RNG& rng)
    {
        trace::null_tracer tracer;
        return run_impl(
            context,
            std::move(solution),
            rng,
            easylocal::detail::no_run_control{},
            tracer);
    }

    template<class Context, std::uniform_random_bit_generator RNG>
        requires detail::simulated_annealing_context<Context, Acceptance, RNG>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution,
        RNG& rng,
        const run_control& control)
    {
        trace::null_tracer tracer;
        return run_impl(context, std::move(solution), rng, control, tracer);
    }

    template<
        class Context,
        std::uniform_random_bit_generator RNG,
        class Tracer>
        requires detail::simulated_annealing_context<Context, Acceptance, RNG> &&
                 trace::tracer_for<
                     Tracer,
                     trace::event::run_started<typename Context::cost_type>>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution,
        RNG& rng,
        Tracer& tracer)
    {
        return run_impl(
            context,
            std::move(solution),
            rng,
            easylocal::detail::no_run_control{},
            tracer);
    }

    template<
        class Context,
        std::uniform_random_bit_generator RNG,
        class Tracer>
        requires detail::simulated_annealing_context<Context, Acceptance, RNG> &&
                 trace::tracer_for<
                     Tracer,
                     trace::event::run_started<typename Context::cost_type>>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution,
        RNG& rng,
        const run_control& control,
        Tracer& tracer)
    {
        return run_impl(context, std::move(solution), rng, control, tracer);
    }

private:
    template<
        class Context,
        std::uniform_random_bit_generator RNG,
        easylocal::detail::run_control_like Control,
        class Tracer>
    [[nodiscard]]
    auto run_impl(
        const Context& context,
        typename Context::solution_type solution,
        RNG& rng,
        const Control& control,
        Tracer& tracer)
    {
        const auto& neighborhood = context.neighborhood_explorer();
        const auto evaluation = context.evaluation();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using result_type = SimulatedAnnealingResult<solution_type, cost_type>;
        constexpr bool controlled_run =
            !std::same_as<Control, easylocal::detail::no_run_control>;

        auto current = evaluation.evaluate(solution);
        auto best_solution = solution;
        auto best_cost = current.cost();
        std::size_t iterations = 0;
        std::size_t evaluations = 1;

        trace::emit(tracer, trace::event::run_started<cost_type>{current.cost()});

        temperature_policy_.reset();
        if constexpr (controlled_run)
        {
            control.report(run_progress{
                .evaluations = evaluations,
                .iterations = iterations,
                .evaluation_limit = std::nullopt,
            });
        }

        while (!temperature_policy_.finished())
        {
            if constexpr (controlled_run)
            {
                if (control.stop_requested())
                {
                    break;
                }
            }
            auto sampling_observer = [&](const trace::event::neighborhood_selection& value) {
                trace::emit(tracer, value);
            };

            auto move = [&]() {
                if constexpr (
                    trace::observes<Tracer, trace::event::neighborhood_selection> &&
                    requires {
                        neighborhood.random_move_traced(
                            solution, rng, sampling_observer);
                    })
                {
                    return neighborhood.random_move_traced(
                        solution, rng, sampling_observer);
                }
                else
                {
                    return easylocal::random_move(neighborhood, solution, rng);
                }
            }();

            if (!move.has_value())
            {
                break;
            }

            auto candidate = evaluation.evaluate_move(
                solution,
                current,
                *move);
            ++iterations;
            ++evaluations;

            trace::with_move_route(*move, [&](const auto* route) {
                trace::emit(tracer, trace::event::move_evaluated<cost_type>{
                    .evaluations = evaluations,
                    .iterations = iterations,
                    .current_cost = current.cost(),
                    .candidate_cost = candidate.cost(),
                    .neighborhood = route,
                });
            });
            if constexpr (controlled_run)
            {
                control.report(run_progress{
                    .evaluations = evaluations,
                    .iterations = iterations,
                    .evaluation_limit = std::nullopt,
                });
            }

            const auto accepted = static_cast<bool>(acceptance_.accept(
                candidate.cost(),
                current.cost(),
                temperature_policy_.temperature(),
                rng));

            if (accepted)
            {
                const auto previous_cost = current.cost();
                evaluation.commit(
                    solution,
                    current,
                    std::move(candidate));

                trace::with_move_route(*move, [&](const auto* route) {
                    trace::emit(tracer, trace::event::move_accepted<cost_type>{
                        .evaluations = evaluations,
                        .iterations = iterations,
                        .previous_cost = previous_cost,
                        .cost = current.cost(),
                        .neighborhood = route,
                    });
                });

                if (context.better(current.cost(), best_cost))
                {
                    const auto previous_best = best_cost;
                    best_solution = solution;
                    best_cost = current.cost();
                    trace::emit(tracer, trace::event::incumbent_updated<cost_type>{
                        .evaluations = evaluations,
                        .iterations = iterations,
                        .previous_cost = previous_best,
                        .cost = best_cost,
                    });
                }
            }

            temperature_policy_.on_iteration(accepted);
        }

        trace::emit(tracer, trace::event::run_finished<cost_type>{
            .evaluations = evaluations,
            .iterations = iterations,
            .cost = best_cost,
        });

        return result_type{
            .solution = std::move(best_solution),
            .cost = std::move(best_cost),
            .iterations = iterations,
            .evaluations = evaluations,
        };
    }

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

namespace easylocal::runner
{

template<
    search::temperature_policy TemperaturePolicy = search::temperature::Classic,
    class Acceptance = search::MetropolisAcceptance>
struct SimulatedAnnealingConfig
{
    TemperaturePolicy temperature_policy;
    Acceptance acceptance{};
};

struct simulated_annealing
{
    template<search::temperature_policy TemperaturePolicy, class Acceptance>
    [[nodiscard]]
    static auto make(
        SimulatedAnnealingConfig<TemperaturePolicy, Acceptance> config)
    {
        return search::SimulatedAnnealing{
            std::move(config.temperature_policy),
            std::move(config.acceptance)};
    }

    template<search::temperature_policy TemperaturePolicy>
    [[nodiscard]]
    static auto make(TemperaturePolicy temperature_policy)
    {
        return search::SimulatedAnnealing{std::move(temperature_policy)};
    }
};

} // namespace easylocal::runner
