#include "support/expect.hpp"

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/great_deluge.hpp>
#include <easylocal/runners/hill_climbing.hpp>
#include <easylocal/runners/late_acceptance_hill_climbing.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/trace.hpp>
#include <easylocal/utils/generator.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <random>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace
{

using namespace easylocal;
using namespace easylocal::runners;

// A chain 0, 1, ..., 10: the only move advances by one, and the end of the
// chain has no move. The cost profiles below decide which steps improve,
// keep or worsen the cost.

struct ChainInstance
{
};

struct ChainSolution
{
    int value{};
};

struct ChainMove
{
};

class ChainSolutionManager
{
public:
    using input_type = ChainInstance;
    using solution_type = ChainSolution;

    explicit ChainSolutionManager(const ChainInstance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]] auto input() const noexcept -> const ChainInstance&
    {
        return instance_;
    }

    [[nodiscard]] static auto is_valid(const ChainSolution&) noexcept -> bool
    {
        return true;
    }

private:
    const ChainInstance& instance_;
};

class ChainNeighborhood
{
public:
    using input_type = ChainInstance;
    using solution_type = ChainSolution;
    using move_type = ChainMove;

    static constexpr int end = 10;

    explicit ChainNeighborhood(const ChainSolutionManager& solution_manager) noexcept
        : instance_{solution_manager.input()}
    {
    }

    [[nodiscard]] auto input() const noexcept -> const ChainInstance&
    {
        return instance_;
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const ChainSolution& solution, RNG&) noexcept
        -> std::optional<ChainMove>
    {
        if (solution.value >= end)
            return std::nullopt;
        return ChainMove{};
    }

    // The same move, for the runners that enumerate the neighborhood.
    [[nodiscard]] static auto moves(const ChainSolution& solution)
        -> easylocal::generator<ChainMove>
    {
        if (solution.value < end)
            co_yield ChainMove{};
    }

    [[nodiscard]] static auto is_valid(const ChainSolution&, const ChainMove&) noexcept
        -> bool
    {
        return true;
    }

    static void make_move(ChainSolution& solution, const ChainMove&) noexcept
    {
        ++solution.value;
    }

private:
    const ChainInstance& instance_;
};

// A plateau up to 7, then a lower plateau: a descent stops at 0.
struct PlateauCost
{
    [[nodiscard]] static auto evaluate(const ChainSolution& solution) noexcept -> int
    {
        return solution.value < 8 ? 1 : 0;
    }
};

// Every step worsens.
struct UphillCost
{
    [[nodiscard]] static auto evaluate(const ChainSolution& solution) noexcept -> int
    {
        return solution.value;
    }
};

// Every step improves: 10 at the start, 0 at the end of the chain.
struct DownhillCost
{
    [[nodiscard]] static auto evaluate(const ChainSolution& solution) noexcept -> int
    {
        return ChainNeighborhood::end - solution.value;
    }
};

// Two improving steps, then a plateau.
struct SlopeThenPlateauCost
{
    [[nodiscard]] static auto evaluate(const ChainSolution& solution) noexcept -> int
    {
        return solution.value < 2 ? -solution.value : -2;
    }
};

template<class Cost>
[[nodiscard]]
auto chain_runner(const HillClimbingParameters parameters)
{
    return easylocal::make_runner<HillClimbing>(parameters)
        | (solution_manager<ChainSolutionManager>() | component<Cost>())
        | neighborhood<ChainNeighborhood>();
}

// The incumbent_updated records of a trace.
[[nodiscard]]
auto incumbents(const easylocal::trace::memory_recorder<int>& trace)
    -> std::vector<easylocal::trace::memory_recorder<int>::incumbent_updated_record>
{
    std::vector<easylocal::trace::memory_recorder<int>::incumbent_updated_record> found;
    for (const auto& record : trace.records())
        if (const auto* incumbent = std::get_if<
                easylocal::trace::memory_recorder<int>::incumbent_updated_record>(
                &record))
            found.push_back(*incumbent);
    return found;
}

} // namespace

int main()
{
    bool ok = true;
    const ChainInstance instance;

    {
        ok &= expect(
            !HillClimbingParameters{.max_idle_iterations = 0}.validate(),
            "hill climbing rejects a zero idle limit");
        ok &= expect(
            static_cast<bool>(HillClimbingParameters{}.validate()),
            "hill climbing defaults pass validation");

        HillClimbingParameters climbing_parameters;
        easylocal::config::parameter_set configuration;
        configuration.add(climbing_parameters);
        ok &= expect(
            std::ranges::any_of(
                configuration.parameters(),
                [](const easylocal::config::parameter_info& parameter) {
                    return parameter.path == "max_idle_iterations"
                        && parameter.value == "1000";
                }),
            "hill climbing exposes max_idle_iterations in its configuration");

        const std::array invalid{
            easylocal::config::text_override{"max_idle_iterations", "0"}};
        ok &= expect(
            !configuration.apply(invalid),
            "hill climbing configuration rejects invalid parameters");
        const std::array valid{
            easylocal::config::text_override{"max_idle_iterations", "5"}};
        ok &= expect(
            static_cast<bool>(configuration.apply(valid))
                && climbing_parameters.max_idle_iterations == 5,
            "hill climbing configuration updates its parameters");
        const std::array never_idle{
            easylocal::config::text_override{"max_idle_iterations", "unlimited"}};
        ok &= expect(
            static_cast<bool>(configuration.apply(never_idle))
                && climbing_parameters.max_idle_iterations.is_unlimited(),
            "the idle limit of hill climbing can be unlimited from text");
    }

    {
        auto runner = chain_runner<PlateauCost>({.max_idle_iterations = 100});
        std::mt19937 rng{7U};
        const auto result = runner.bind(instance).run(ChainSolution{}, rng);
        ok &= expect(
            result.solution.value == ChainNeighborhood::end && result.cost == 0,
            "hill climbing crosses a plateau with sideways moves");
        ok &= expect(
            result.termination == termination_reason::local_optimum
                && result.iterations == 10 && result.evaluations == 11,
            "hill climbing ends at a solution without moves");
    }

    // The other runners that draw random moves end the same way when the
    // neighborhood proposes none: here the solution is the end of the chain.
    {
        const auto chain = [](auto runner) {
            return std::move(runner)
                | (solution_manager<ChainSolutionManager>() | component<UphillCost>())
                | neighborhood<ChainNeighborhood>();
        };
        const auto ends_at_local_optimum = [&](auto runner, const std::string_view name) {
            std::mt19937 rng{7U};
            const auto result = runner.bind(instance).run(
                ChainSolution{.value = ChainNeighborhood::end},
                rng);
            return expect(result.termination == termination_reason::local_optimum, name);
        };
        ok &= ends_at_local_optimum(
            chain(make_runner<LateAcceptanceHillClimbing>({})),
            "late acceptance ends at a solution without moves");
        ok &= ends_at_local_optimum(
            chain(make_runner<GreatDeluge>({})),
            "great deluge ends at a solution without moves");
        ok &= ends_at_local_optimum(
            chain(make_runner<SimulatedAnnealing<>>({})),
            "simulated annealing ends at a solution without moves");
    }

    {
        auto runner = chain_runner<UphillCost>({.max_idle_iterations = 5});
        std::mt19937 rng{7U};
        const auto result = runner.bind(instance).run(ChainSolution{}, rng);
        ok &= expect(
            result.solution.value == 0 && result.cost == 0,
            "hill climbing rejects worsening moves");
        ok &= expect(
            result.termination == termination_reason::idle_limit_reached
                && result.iterations == 5 && result.evaluations == 6,
            "hill climbing stops after the idle limit");
        ok &= expect(
            to_string(result.termination) == "idle limit reached",
            "the idle termination has a readable name");
    }

    {
        auto runner = chain_runner<SlopeThenPlateauCost>({.max_idle_iterations = 4});
        std::mt19937 rng{7U};
        const auto result = runner.bind(instance).run(ChainSolution{}, rng);
        ok &= expect(
            result.cost == -2 && result.solution.value == 6 && result.iterations == 6
                && result.termination == termination_reason::idle_limit_reached,
            "improvements reset the idle count, sideways moves do not");
    }

    {
        auto runner =
            chain_runner<PlateauCost>({.max_idle_iterations = 100, .max_evaluations = 3});
        std::mt19937 rng{7U};
        const auto result = runner.bind(instance).run(ChainSolution{}, rng);
        ok &= expect(
            result.termination == termination_reason::evaluation_budget_exhausted
                && result.evaluations == 3 && result.solution.value == 2,
            "hill climbing honours its evaluation budget");
    }

    {
        auto runner = chain_runner<PlateauCost>({.max_idle_iterations = 100});
        std::mt19937 rng{7U};
        easylocal::trace::memory_recorder<int> trace;
        const auto result =
            runner.bind(instance).run(ChainSolution{}, rng, easylocal::with(trace));
        std::size_t accepted = 0;
        for (const auto& record : trace.records())
        {
            accepted += std::holds_alternative<
                easylocal::trace::memory_recorder<int>::move_accepted_record>(record);
        }
        ok &= expect(
            accepted == 10 && result.solution.value == ChainNeighborhood::end,
            "hill climbing traces every accepted move, sideways included");
    }

    {
        auto runner = chain_runner<PlateauCost>({.max_idle_iterations = 100});
        std::mt19937 rng{7U};
        const auto result =
            runner.bind(instance).run(ChainSolution{}, rng, easylocal::stop_at(0));
        ok &= expect(
            result.termination == termination_reason::target_reached
                && result.solution.value == 8,
            "hill climbing stops at a reached target");
    }

    {
        auto runner = chain_runner<PlateauCost>({.max_idle_iterations = 100});
        std::stop_source stop;
        stop.request_stop();
        const easylocal::run_control control{stop.get_token()};
        std::mt19937 rng{7U};
        const auto result =
            runner.bind(instance).run(ChainSolution{}, rng, easylocal::with(control));
        ok &= expect(
            result.termination == termination_reason::cancelled && result.iterations == 0,
            "hill climbing is cancellable");
    }

    // First and Best Improvement on the chain: each step improves, so they go
    // to its end, a local optimum, or stop at their budget.
    {
        const auto descent = [](auto runner) {
            return std::move(runner)
                | (solution_manager<ChainSolutionManager>() | component<DownhillCost>())
                | neighborhood<ChainNeighborhood>();
        };
        auto first = descent(easylocal::make_runner<FirstImprovement>({}));
        auto best = descent(easylocal::make_runner<BestImprovement>({}));
        const auto first_result = first.bind(instance).run(ChainSolution{});
        const auto best_result = best.bind(instance).run(ChainSolution{});
        ok &= expect(
            first_result.termination == termination_reason::local_optimum
                && first_result.solution.value == ChainNeighborhood::end
                && first_result.iterations == 10 && first_result.evaluations == 11,
            "first improvement descends to a local optimum");
        ok &= expect(
            best_result.termination == termination_reason::local_optimum
                && best_result.solution.value == ChainNeighborhood::end
                && best_result.iterations == 10 && best_result.evaluations == 11,
            "best improvement descends to a local optimum");

        auto first_budget =
            descent(easylocal::make_runner<FirstImprovement>({.max_evaluations = 4}));
        auto best_budget =
            descent(easylocal::make_runner<BestImprovement>({.max_evaluations = 4}));
        const auto first_spent = first_budget.bind(instance).run(ChainSolution{});
        const auto best_spent = best_budget.bind(instance).run(ChainSolution{});
        ok &= expect(
            first_spent.termination == termination_reason::evaluation_budget_exhausted
                && first_spent.evaluations == 4 && first_spent.solution.value == 3,
            "first improvement stops at its evaluation budget");
        ok &= expect(
            best_spent.termination == termination_reason::evaluation_budget_exhausted
                && best_spent.evaluations == 4 && best_spent.solution.value == 3,
            "best improvement stops at its evaluation budget");

        // Their current solution is their best one: each improving move is a
        // new incumbent, as for Hill Climbing.
        easylocal::trace::memory_recorder<int> first_trace;
        easylocal::trace::memory_recorder<int> best_trace;
        easylocal::trace::memory_recorder<int> climbing_trace;
        static_cast<void>(
            first.bind(instance).run(ChainSolution{}, easylocal::with(first_trace)));
        static_cast<void>(
            best.bind(instance).run(ChainSolution{}, easylocal::with(best_trace)));
        auto climbing = chain_runner<DownhillCost>({.max_idle_iterations = 5});
        std::mt19937 rng{7U};
        static_cast<void>(climbing.bind(instance)
                .run(ChainSolution{}, rng, easylocal::with(climbing_trace)));
        for (const auto& [trace, name] :
            {std::pair{&first_trace, "first improvement"},
                std::pair{&best_trace, "best improvement"},
                std::pair{&climbing_trace, "hill climbing"}})
        {
            const auto updates = incumbents(*trace);
            ok &= expect(
                updates.size() == 10 && updates.front().previous_cost == 10
                    && updates.back().cost == 0,
                std::string{name} + " reports each improvement as a new incumbent");
        }
    }

    return ok ? 0 : 1;
}
