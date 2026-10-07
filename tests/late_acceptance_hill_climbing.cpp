#include "support/expect.hpp"

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/hill_climbing.hpp>
#include <easylocal/runners/late_acceptance_hill_climbing.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/trace.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <random>
#include <stop_token>
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

// 10, 5, 7, 3 and then 100: the step from 5 to 7 worsens, so Hill Climbing
// stops at 5, while a history of two costs still holds the initial 10 and
// accepts it, reaching 3.
struct ValleysCost
{
    [[nodiscard]] static auto evaluate(const ChainSolution& solution) noexcept -> int
    {
        constexpr std::array<int, 4> profile{10, 5, 7, 3};
        return solution.value < 4
            ? profile[static_cast<std::size_t>(solution.value)]
            : 100;
    }
};

// 10, 5, 7 and then 100: late acceptance leaves the best solution, 5, for 7.
struct LeaveTheBestCost
{
    [[nodiscard]] static auto evaluate(const ChainSolution& solution) noexcept -> int
    {
        constexpr std::array<int, 3> profile{10, 5, 7};
        return solution.value < 3
            ? profile[static_cast<std::size_t>(solution.value)]
            : 100;
    }
};

// A plateau up to 7, then a lower plateau.
struct PlateauCost
{
    [[nodiscard]] static auto evaluate(const ChainSolution& solution) noexcept -> int
    {
        return solution.value < 8 ? 1 : 0;
    }
};

template<class Cost>
[[nodiscard]]
auto chain_runner(const LateAcceptanceHillClimbingParameters parameters)
{
    return easylocal::make_runner<LateAcceptanceHillClimbing>(parameters)
        | (solution_manager<ChainSolutionManager>() | component<Cost>())
        | neighborhood<ChainNeighborhood>();
}

} // namespace

int main()
{
    bool ok = true;
    const ChainInstance instance;

    {
        ok &= expect(
            !LateAcceptanceHillClimbingParameters{.history_length = 0}.validate(),
            "late acceptance rejects an empty history");
        ok &= expect(
            !LateAcceptanceHillClimbingParameters{.max_idle_iterations = 0}.validate(),
            "late acceptance rejects a zero idle limit");
        ok &= expect(
            static_cast<bool>(LateAcceptanceHillClimbingParameters{}.validate()),
            "late acceptance defaults pass validation");

        LateAcceptanceHillClimbingParameters climbing_parameters;
        easylocal::config::parameter_set configuration;
        configuration.add(climbing_parameters);
        ok &= expect(
            std::ranges::any_of(
                configuration.parameters(),
                [](const easylocal::config::parameter_info& parameter) {
                    return parameter.path == "history_length" && parameter.value == "10";
                }),
            "late acceptance exposes history_length in its configuration");

        const std::array invalid{easylocal::config::text_override{"history_length", "0"}};
        ok &= expect(
            !configuration.apply(invalid),
            "late acceptance configuration rejects invalid parameters");
        const std::array valid{easylocal::config::text_override{"history_length", "3"}};
        ok &= expect(
            static_cast<bool>(configuration.apply(valid))
                && climbing_parameters.history_length == 3,
            "late acceptance configuration updates its parameters");
    }

    {
        std::mt19937 rng{7U};
        const auto late =
            chain_runner<ValleysCost>({.history_length = 2, .max_idle_iterations = 5})
                .bind(instance)
                .run(ChainSolution{}, rng);
        ok &= expect(
            late.solution.value == 3 && late.cost == 3,
            "late acceptance accepts a worsening move the history allows");

        const auto immediate =
            chain_runner<ValleysCost>({.history_length = 1, .max_idle_iterations = 5})
                .bind(instance)
                .run(ChainSolution{}, rng);
        ok &= expect(
            immediate.solution.value == 1 && immediate.cost == 5,
            "with a history of one cost the worsening move is rejected");
    }

    {
        auto runner = chain_runner<LeaveTheBestCost>(
            {.history_length = 2, .max_idle_iterations = 3});
        std::mt19937 rng{7U};
        easylocal::trace::memory_recorder<int> trace;
        const auto result =
            runner.bind(instance).run(ChainSolution{}, rng, easylocal::with(trace));
        ok &= expect(
            result.solution.value == 1 && result.cost == 5,
            "late acceptance returns the best solution, not the current one");
        ok &= expect(
            result.termination == termination_reason::idle_limit_reached
                && result.iterations == 4 && result.evaluations == 5,
            "the idle count runs from the last improvement of the best cost");

        std::size_t accepted = 0;
        std::size_t incumbents = 0;
        for (const auto& record : trace.records())
        {
            using recorder = easylocal::trace::memory_recorder<int>;
            accepted += std::holds_alternative<recorder::move_accepted_record>(record);
            incumbents +=
                std::holds_alternative<recorder::incumbent_updated_record>(record);
        }
        ok &= expect(
            accepted == 2 && incumbents == 1,
            "late acceptance traces accepted moves and best-cost updates");
    }

    {
        std::mt19937 rng_late{7U};
        std::mt19937 rng_climbing{7U};
        const auto late =
            chain_runner<PlateauCost>({.history_length = 1, .max_idle_iterations = 4})
                .bind(instance)
                .run(ChainSolution{}, rng_late);
        const auto climbing =
            (easylocal::make_runner<HillClimbing>({.max_idle_iterations = 4})
                | (solution_manager<ChainSolutionManager>() | component<PlateauCost>())
                | neighborhood<ChainNeighborhood>())
                .bind(instance)
                .run(ChainSolution{}, rng_climbing);
        ok &= expect(
            late.cost == climbing.cost && late.iterations == climbing.iterations
                && late.evaluations == climbing.evaluations,
            "with a history of one cost late acceptance moves as Hill Climbing");
    }

    {
        auto runner = chain_runner<PlateauCost>(
            {.history_length = 2, .max_idle_iterations = 100, .max_evaluations = 3});
        std::mt19937 rng{7U};
        const auto result = runner.bind(instance).run(ChainSolution{}, rng);
        ok &= expect(
            result.termination == termination_reason::evaluation_budget_exhausted
                && result.evaluations == 3,
            "late acceptance honours its evaluation budget");
    }

    {
        auto runner = chain_runner<PlateauCost>({.history_length = 2});
        std::mt19937 rng{7U};
        const auto result =
            runner.bind(instance).run(ChainSolution{}, rng, easylocal::stop_at(0));
        ok &= expect(
            result.termination == termination_reason::target_reached
                && result.solution.value == 8,
            "late acceptance stops at a reached target");
    }

    return ok ? 0 : 1;
}
