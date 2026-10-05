#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/great_deluge.hpp>
#include <easylocal/runners/hill_climbing.hpp>
#include <easylocal/runners/late_acceptance_hill_climbing.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/trace.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <optional>
#include <random>
#include <stop_token>
#include <string_view>
#include <type_traits>
#include <variant>

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

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }
    return true;
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

    return ok ? 0 : 1;
}
