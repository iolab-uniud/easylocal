#pragma once

#include <easylocal/search/detail/context_concepts.hpp>
#include <easylocal/search/metropolis_acceptance.hpp>
#include <easylocal/search/temperature_policy.hpp>

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
    requires(
        const Context& context,
        const typename Context::solution_type& solution,
        RNG& rng)
    {
        {
            context.neighborhood_explorer().random_move(solution, rng)
        } -> std::same_as<std::optional<
            typename Context::neighborhood_explorer_type::move_type>>;
    };

template<class Acceptance, class Cost, class RNG>
concept acceptance_policy_for =
    numeric_cost<Cost> &&
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
    numeric_cost<typename Context::cost_type> &&
    acceptance_policy_for<Acceptance, typename Context::cost_type, RNG>;

template<class Cost>
consteval auto validate_simulated_annealing_cost() -> bool
{
    static_assert(
        numeric_cost<Cost>,
        "SimulatedAnnealing requires a numeric cost_type; structured, "
        "hierarchical, and lexicographic costs are not supported by the "
        "current SA contract");
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
    auto configuration() const
        requires (
            config::configuration_provider<TemperaturePolicy> ||
            config::configuration_provider<Acceptance>)
    {
        auto children = std::tuple_cat(
            config::detail::configuration_nodes(temperature_policy_),
            config::detail::configuration_nodes(acceptance_));

        return std::apply(
            [](auto... nodes) {
                return config::named<"search">(std::move(nodes)...);
            },
            std::move(children));
    }

    template<class Context, std::uniform_random_bit_generator RNG>
        requires detail::random_move_context<Context, RNG> &&
                 (!numeric_cost<typename Context::cost_type>)
    [[nodiscard]]
    auto run(
        const Context&,
        typename Context::solution_type,
        RNG&)
    {
        static_assert(
            detail::validate_simulated_annealing_cost<
                typename Context::cost_type>());
    }

    template<class Context, std::uniform_random_bit_generator RNG>
        requires detail::simulated_annealing_context<Context, Acceptance, RNG>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution,
        RNG& rng)
    {
        const auto& neighborhood = context.neighborhood_explorer();
        const auto evaluation = context.evaluation();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using result_type = SimulatedAnnealingResult<solution_type, cost_type>;

        static_assert(
            numeric_cost<cost_type>,
            "SimulatedAnnealing requires a numeric cost_type; structured, "
            "hierarchical, and lexicographic costs are not supported by the "
            "current SA contract");

        auto current = evaluation.evaluate(solution);
        auto best_solution = solution;
        auto best_cost = current.cost();
        std::size_t iterations = 0;
        std::size_t evaluations = 1;

        temperature_policy_.reset();

        while (!temperature_policy_.finished())
        {
            auto move = neighborhood.random_move(solution, rng);
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

            const auto accepted = static_cast<bool>(acceptance_.accept(
                candidate.cost(),
                current.cost(),
                temperature_policy_.temperature(),
                rng));

            if (accepted)
            {
                evaluation.commit(
                    solution,
                    current,
                    std::move(candidate));

                if (context.better(current.cost(), best_cost))
                {
                    best_solution = solution;
                    best_cost = current.cost();
                }
            }

            temperature_policy_.on_iteration(accepted);
        }

        return result_type{
            .solution = std::move(best_solution),
            .cost = std::move(best_cost),
            .iterations = iterations,
            .evaluations = evaluations,
        };
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
