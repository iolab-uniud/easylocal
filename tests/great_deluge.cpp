#include "support/expect.hpp"

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/great_deluge.hpp>
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

// The cost along the chain: the listed values, then 100.
template<int... Values>
struct ProfileCost
{
    [[nodiscard]] static auto evaluate(const ChainSolution& solution) noexcept -> int
    {
        constexpr std::array<int, sizeof...(Values)> profile{Values...};
        const auto index = static_cast<std::size_t>(solution.value);
        return index < profile.size() ? profile[index] : 100;
    }
};

// The water level is a value of the cost: structured costs are rejected.
template<class Cost>
struct CostOnlyContext
{
    using cost_type = Cost;
};
static_assert(easylocal::runners::detail::great_deluge_cost<CostOnlyContext<int>>);
static_assert(easylocal::runners::detail::great_deluge_cost<CostOnlyContext<double>>);
static_assert(!easylocal::runners::detail::great_deluge_cost<
    CostOnlyContext<easylocal::cost::hierarchical<int, int>>>);

template<class Cost>
[[nodiscard]]
auto chain_runner(const GreatDelugeParameters parameters)
{
    return easylocal::make_runner<GreatDeluge>(parameters)
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
            static_cast<bool>(GreatDelugeParameters{}.validate()),
            "great deluge defaults pass validation");
        ok &= expect(
            !GreatDelugeParameters{.initial_level = 0.0}.validate()
                && !GreatDelugeParameters{.min_level = 1.2}.validate()
                && !GreatDelugeParameters{.level_rate = 1.0}.validate()
                && !GreatDelugeParameters{.neighbors_sampled = 0}.validate(),
            "great deluge rejects invalid levels, rates and samples");

        GreatDelugeParameters deluge_parameters;
        easylocal::config::parameter_set configuration;
        configuration.add(deluge_parameters);
        ok &= expect(
            std::ranges::any_of(
                configuration.parameters(),
                [](const easylocal::config::parameter_info& parameter) {
                    return parameter.path == "level_rate" && parameter.value == "0.99";
                }),
            "great deluge exposes level_rate in its configuration");

        const std::array invalid{easylocal::config::text_override{"level_rate", "0"}};
        ok &= expect(
            !configuration.apply(invalid),
            "great deluge configuration rejects invalid parameters");
        const std::array valid{
            easylocal::config::text_override{"neighbors_sampled", "7"}};
        ok &= expect(
            static_cast<bool>(configuration.apply(valid))
                && deluge_parameters.neighbors_sampled == 7,
            "great deluge configuration updates its parameters");
    }

    {
        // Level 12 accepts the step to 11 and 5 improves; 100 is then
        // rejected while the level, 0.8 times lower at each proposal, falls
        // from 7.68 below 0.9 times 5 after the fifth proposal.
        auto runner = chain_runner<ProfileCost<10, 11, 5>>(
            {.initial_level = 1.2,
                .min_level = 0.9,
                .level_rate = 0.8,
                .neighbors_sampled = 1});
        std::mt19937 rng{7U};
        const auto result = runner.bind(instance).run(ChainSolution{}, rng);
        ok &= expect(
            result.solution.value == 2 && result.cost == 5,
            "great deluge accepts a worsening move below the water level");
        ok &= expect(
            result.termination == termination_reason::completed && result.iterations == 5,
            "great deluge stops when the level falls below the final level");
    }

    {
        // Level 10.5 rejects 11; halved to 5.25 it is below 9 at once.
        auto runner = chain_runner<ProfileCost<10, 11, 5>>(
            {.initial_level = 1.05,
                .min_level = 0.9,
                .level_rate = 0.5,
                .neighbors_sampled = 1});
        std::mt19937 rng{7U};
        const auto result = runner.bind(instance).run(ChainSolution{}, rng);
        ok &= expect(
            result.solution.value == 0 && result.cost == 10 && result.iterations == 1,
            "great deluge rejects a worsening move above the water level");
    }

    {
        // 8 improves, 9 is accepted below level 10, 100 is rejected twice at
        // level 5, which then halves below 0.5 times 8.
        auto runner = chain_runner<ProfileCost<10, 8, 9>>(
            {.initial_level = 1.0,
                .min_level = 0.5,
                .level_rate = 0.5,
                .neighbors_sampled = 2});
        std::mt19937 rng{7U};
        easylocal::trace::memory_recorder<int> trace;
        const auto result =
            runner.bind(instance).run(ChainSolution{}, rng, easylocal::with(trace));
        ok &= expect(
            result.solution.value == 1 && result.cost == 8 && result.iterations == 4,
            "great deluge returns the best solution, not the current one");

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
            "great deluge traces accepted moves and best-cost updates");
    }

    {
        auto runner = chain_runner<ProfileCost<0, 0>>(GreatDelugeParameters{});
        std::mt19937 rng{7U};
        const auto result = runner.bind(instance).run(ChainSolution{}, rng);
        ok &= expect(
            result.iterations == 0 && result.cost == 0,
            "great deluge stops at once on a zero cost");
    }

    {
        auto runner = chain_runner<ProfileCost<10, 10, 10, 10, 10>>(
            {.initial_level = 1.2, .max_evaluations = 3});
        std::mt19937 rng{7U};
        const auto result = runner.bind(instance).run(ChainSolution{}, rng);
        ok &= expect(
            result.termination == termination_reason::evaluation_budget_exhausted
                && result.evaluations == 3,
            "great deluge honours its evaluation budget");
    }

    {
        auto runner = chain_runner<ProfileCost<10, 11, 5>>(
            {.initial_level = 1.2, .level_rate = 0.5, .neighbors_sampled = 10});
        std::mt19937 rng{7U};
        const auto result =
            runner.bind(instance).run(ChainSolution{}, rng, easylocal::stop_at(5));
        ok &= expect(
            result.termination == termination_reason::target_reached && result.cost == 5,
            "great deluge stops at a reached target");
    }

    return ok ? 0 : 1;
}
