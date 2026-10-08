#include "cost_components.hpp"
#include "cost_deltas.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"
#include "support/exam_timeslot_load_delta.hpp"
#include "support/expect.hpp"

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/trace.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

using namespace easylocal;
using namespace easylocal::runners;
namespace exam = exam_timetabling;

struct CountingEngine
{
    using result_type = std::uint32_t;

    std::size_t calls{};

    [[nodiscard]] static constexpr auto min() noexcept -> result_type
    {
        return 0;
    }

    [[nodiscard]] static constexpr auto max() noexcept -> result_type
    {
        return 0xffffffffU;
    }

    // A varying value: a constant max() makes libstdc++'s generate_canonical
    // (GCC 16, P0952) reject and redraw forever.
    auto operator()() noexcept -> result_type
    {
        ++calls;
        return static_cast<result_type>(calls * 0x9e3779b9U);
    }
};

struct AlwaysAccept
{
    template<class Cost, class RNG>
    [[nodiscard]]
    constexpr auto accept(
        const Cost,
        const Cost,
        const double,
        RNG&) const noexcept -> bool
    {
        return true;
    }
};

// A clock that moves only when the test advances it.
struct ManualClock
{
    using rep = long long;
    using period = std::milli;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<ManualClock>;

    static inline time_point current{};

    [[nodiscard]] static auto now() noexcept -> time_point
    {
        return current;
    }

    static void advance(const duration elapsed) noexcept
    {
        current += elapsed;
    }
};

// A fixed temperature that records the deltas it is calibrated with.
class RecordingPolicy
{
public:
    explicit RecordingPolicy(
        std::vector<double>& deltas,
        const std::size_t samples) noexcept
        : deltas_{&deltas}, samples_{samples}
    {
    }

    void reset() noexcept
    {
        iterations_ = 0;
    }

    [[nodiscard]] static auto temperature() noexcept -> double
    {
        return 1.0;
    }

    void on_iteration(const bool) noexcept
    {
        ++iterations_;
    }

    [[nodiscard]] auto finished() const noexcept -> bool
    {
        return iterations_ >= 5;
    }

    [[nodiscard]] auto calibration_samples() const noexcept -> std::size_t
    {
        return samples_;
    }

    void calibrate(const std::span<const double> deltas)
    {
        deltas_->assign(deltas.begin(), deltas.end());
    }

private:
    std::vector<double>* deltas_;
    std::size_t samples_{};
    std::size_t iterations_{};
};

static_assert(calibrating_temperature_policy<RecordingPolicy>);

struct ChainInstance
{
};

struct ChainSolution
{
    int value{};
};

struct ChainMove
{
    int delta{};
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

struct ChainValue
{
    [[nodiscard]] static auto evaluate(const ChainSolution& solution) noexcept -> int
    {
        return solution.value;
    }
};

class RandomOnlyChainNeighborhood
{
public:
    using input_type = ChainInstance;
    using solution_type = ChainSolution;
    using move_type = ChainMove;

    explicit RandomOnlyChainNeighborhood(
        const ChainSolutionManager& solution_manager) noexcept
        : instance_{solution_manager.input()}
    {
    }

    [[nodiscard]] auto input() const noexcept -> const ChainInstance&
    {
        return instance_;
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(
        const ChainSolution& solution,
        RNG&) noexcept -> std::optional<ChainMove>
    {
        if (solution.value == 0)
        {
            return ChainMove{.delta = -5};
        }
        if (solution.value == -5)
        {
            return ChainMove{.delta = 3};
        }
        return std::nullopt;
    }

    [[nodiscard]] static auto is_valid(const ChainSolution&, const ChainMove&) noexcept -> bool { return true; }

    static void make_move(
        ChainSolution& solution,
        const ChainMove& move) noexcept
    {
        solution.value += move.delta;
    }

private:
    const ChainInstance& instance_;
};

static_assert(easylocal::neighborhood_explorer_for<
              RandomOnlyChainNeighborhood,
              ChainSolutionManager>);
static_assert(!easylocal::deterministic_neighborhood_for<
              RandomOnlyChainNeighborhood,
              ChainSolution>);
static_assert(easylocal::cost::arithmetic<int>);
static_assert(easylocal::cost::arithmetic<double>);
static_assert(!easylocal::cost::arithmetic<bool>);
struct StructuredCost { int hard; int soft; };
static_assert(!easylocal::cost::arithmetic<StructuredCost>);

// A cost ordered by its value, without a numeric difference (cost::delta).
struct OrderedCost
{
    int value{};

    auto operator<=>(const OrderedCost&) const = default;
};
static_assert(!easylocal::cost::has_delta<OrderedCost>);
static_assert(easylocal::runners::acceptance_policy_for<
    easylocal::runners::MetropolisAcceptance,
    int,
    std::mt19937>);
static_assert(!easylocal::runners::acceptance_policy_for<
    easylocal::runners::MetropolisAcceptance,
    OrderedCost,
    std::mt19937>);
static_assert(
    easylocal::runners::acceptance_policy_for<AlwaysAccept, OrderedCost, std::mt19937>);

struct OrderedChainValue
{
    [[nodiscard]] static auto evaluate(const ChainSolution& solution) noexcept
        -> OrderedCost
    {
        return {solution.value};
    }
};


[[nodiscard]]
auto exam_instance() -> exam::ExamTimetablingInstance
{
    return {
        .exam_count = 5,
        .timeslot_count = 3,
        .conflicts = {
            {0, 1, 4},
            {0, 2, 2},
            {1, 3, 3},
            {2, 3, 5},
            {3, 4, 1},
        },
    };
}

} // namespace

int main()
{
    bool ok = true;

    {
        easylocal::runners::SimulatedAnnealingParameters<
            temperature::FixedLengthParameters>
            annealing{
                .temperature = {
                    .initial_temperature = 8.0,
                    .final_temperature = 0.25,
                    .cooling_rate = 0.75,
                    .allowed_iterations = 200,
                }};

        easylocal::config::parameter_set configuration;
        configuration.add(annealing);
        const auto allowed_iterations = [&] {
            for (const auto& parameter : configuration.parameters())
                if (parameter.path == "temperature.allowed_iterations")
                    return parameter.value;
            return std::string{};
        };
        ok &= expect(
            allowed_iterations() == "200",
            "SA configuration exposes the nested temperature policy parameters");

        const std::array update{
            easylocal::config::text_override{"temperature.allowed_iterations", "20"}};
        ok &= expect(
            static_cast<bool>(configuration.apply(update)),
            "SA configuration can update its nested temperature policy");
        ok &= expect(
            allowed_iterations() == "20",
            "SA nested configuration update changes the owned policy");
    }

    {
        temperature::Classic policy{temperature::ClassicParameters{
            .initial_temperature = 8.0,
            .final_temperature = 1.0,
            .cooling_rate = 0.5,
            .samples_per_temperature = 2,
        }};

        ok &= expect(policy.temperature() == 8.0,
            "classic temperature starts at T0");
        policy.on_iteration(false);
        ok &= expect(policy.temperature() == 8.0,
            "classic temperature stays fixed within its level");
        policy.on_iteration(false);
        ok &= expect(policy.temperature() == 4.0,
            "classic temperature cools after the configured sample count");
        for (int i = 0; i < 4; ++i)
        {
            policy.on_iteration(false);
        }
        ok &= expect(policy.finished(),
            "classic temperature terminates at the final temperature");
        policy.reset();
        ok &= expect(!policy.finished() && policy.temperature() == 8.0,
            "classic temperature reset restores run state");
    }

    {
        temperature::FixedLength policy{temperature::FixedLengthParameters{
            .initial_temperature = 8.0,
            .final_temperature = 1.0,
            .cooling_rate = 0.5,
            .allowed_iterations = 12,
        }};
        ok &= expect(policy.samples_per_temperature() == 4,
            "fixed-length policy distributes the iteration budget over temperature levels");
        for (int i = 0; i < 4; ++i)
        {
            policy.on_iteration(false);
        }
        ok &= expect(policy.temperature() == 4.0,
            "fixed-length policy cools after its sampled-move quota");
        for (int i = 0; i < 8; ++i)
        {
            policy.on_iteration(false);
        }
        ok &= expect(policy.finished(),
            "fixed-length policy owns its iteration-budget termination");
    }

    {
        temperature::Cutoff policy{temperature::CutoffParameters{
            .initial_temperature = 8.0,
            .final_temperature = 1.0,
            .cooling_rate = 0.5,
            .allowed_iterations = 12,
            .accepted_ratio = 0.5,
        }};
        ok &= expect(policy.accepted_limit() == 2,
            "cutoff policy derives the accepted-move cutoff from rho");
        policy.on_iteration(false);
        policy.on_iteration(false);
        ok &= expect(policy.temperature() == 8.0,
            "cutoff policy ignores rejected moves for temperature changes");
        policy.on_iteration(true);
        policy.on_iteration(true);
        ok &= expect(policy.temperature() == 4.0,
            "cutoff policy cools after the accepted-move cutoff");
    }

    {
        // Without acceptances both spend their budget of 12 proposals: Cutoff,
        // which cools on acceptances only, stays at its initial temperature;
        // Hybrid runs its three levels of 4 proposals, 8, 4 and 2.
        const auto run_to_end = [](auto policy) {
            std::size_t proposals = 0;
            while (!policy.finished() && proposals < 100)
            {
                policy.on_iteration(false);
                ++proposals;
            }
            return std::pair{proposals, policy.temperature()};
        };
        const auto cutoff_end = run_to_end(
            temperature::Cutoff{temperature::CutoffParameters{
                .initial_temperature = 8.0,
                .final_temperature = 1.0,
                .cooling_rate = 0.5,
                .allowed_iterations = 12,
                .accepted_ratio = 0.5,
            }});
        const auto hybrid_end = run_to_end(
            temperature::Hybrid{temperature::HybridParameters{
                .initial_temperature = 8.0,
                .final_temperature = 1.0,
                .cooling_rate = 0.5,
                .allowed_iterations = 12,
                .accepted_ratio = 0.5,
            }});
        ok &= expect(
            cutoff_end == std::pair{std::size_t{12}, 8.0},
            "cutoff spends its budget, without cooling when nothing is accepted");
        ok &= expect(
            hybrid_end == std::pair{std::size_t{12}, 2.0},
            "hybrid spends its budget over its levels");
    }

    {
        // A reheated time-based schedule: each descent runs on the clock,
        // the first for half the time, the reheat for the rest.
        using timed_descent = temperature::BasicTimeBased<ManualClock>;
        temperature::Reheating<timed_descent> timed{
            {.descent =
                    {.initial_temperature = 8.0,
                        .final_temperature = 1.0,
                        .cooling_rate = 0.5,
                        .allowed_running_time = 4.0},
                .allowed_reheats = 1,
                .reheat_ratio = 0.5,
                .first_descent_share = 0.5}};
        ManualClock::advance(std::chrono::milliseconds{1});
        timed.on_iteration(false);
        const bool first_running = !timed.finished() && timed.reheats() == 0;
        ManualClock::advance(std::chrono::milliseconds{2100});
        timed.on_iteration(false);
        const bool reheated =
            timed.reheats() == 1 && !timed.finished() && timed.temperature() == 4.0;
        ManualClock::advance(std::chrono::milliseconds{2100});
        timed.on_iteration(false);
        ok &= expect(
            first_running && reheated && timed.finished(),
            "a reheated time-based schedule reheats when its first descent's time "
            "is over, and ends with the second's");
    }

    {
        temperature::Hybrid policy{temperature::HybridParameters{
            .initial_temperature = 8.0,
            .final_temperature = 1.0,
            .cooling_rate = 0.5,
            .allowed_iterations = 12,
            .accepted_ratio = 0.5,
        }};
        ok &= expect(
            policy.samples_per_temperature() == 4 && policy.accepted_limit() == 2,
            "hybrid policy starts with sampled and accepted limits");
        policy.on_iteration(true);
        policy.on_iteration(true);
        ok &= expect(policy.temperature() == 4.0,
            "hybrid policy applies the accepted cutoff early");
        ok &= expect(
            policy.samples_per_temperature() == 5,
            "hybrid policy redistributes unused iterations over remaining levels");
        for (int i = 0; i < 5; ++i)
        {
            policy.on_iteration(false);
        }
        ok &= expect(policy.temperature() == 2.0,
            "hybrid policy also cools on the sampled-move limit");
    }

    {
        temperature::FixedTemperature policy{temperature::FixedTemperatureParameters{
            .temperature = 2.0,
            .allowed_iterations = 4,
            .max_accepted = 2,
        }};
        ok &= expect(
            policy.temperature() == 2.0,
            "fixed temperature starts at its temperature");
        policy.on_iteration(true);
        policy.on_iteration(false);
        ok &= expect(
            !policy.finished() && policy.temperature() == 2.0,
            "fixed temperature keeps its temperature");
        policy.on_iteration(true);
        ok &= expect(policy.finished(), "fixed temperature ends on enough acceptances");
        policy.reset();
        for (int i = 0; i < 4; ++i)
            policy.on_iteration(false);
        ok &= expect(policy.finished(), "fixed temperature ends on its iteration budget");

        temperature::FixedTemperature unbounded{
            temperature::FixedTemperatureParameters{.allowed_iterations = 3}};
        for (int i = 0; i < 2; ++i)
            unbounded.on_iteration(true);
        ok &= expect(
            !unbounded.finished()
                && temperature::FixedTemperatureParameters{}.max_accepted.is_unlimited(),
            "fixed temperature accepts without limit by default");
        ok &= expect(
            !temperature::FixedTemperatureParameters{.temperature = 0.0}.validate()
                && !temperature::FixedTemperatureParameters{.max_accepted = 0}.validate(),
            "fixed temperature rejects invalid parameters");
    }

    {
        // 1 -> 0.1 -> 0.01 -> 0.001 is three levels, although 0.1 * 0.1 * 0.1
        // is a little above 0.001 in floating point.
        temperature::Classic classic{temperature::ClassicParameters{
            .initial_temperature = 1.0,
            .final_temperature = 0.001,
            .cooling_rate = 0.1,
            .samples_per_temperature = 1,
        }};
        std::size_t proposals = 0;
        while (!classic.finished() && proposals < 10)
        {
            classic.on_iteration(false);
            ++proposals;
        }
        ok &= expect(
            proposals == 3,
            "classic temperature runs as many levels as it counts");

        temperature::BasicTimeBased<ManualClock> timed{temperature::TimeBasedParameters{
            .initial_temperature = 1.0,
            .final_temperature = 0.001,
            .cooling_rate = 0.1,
            .allowed_running_time = 30.0,
            .accepted_per_temperature = 1,
        }};
        proposals = 0;
        while (!timed.finished() && proposals < 10)
        {
            ManualClock::advance(std::chrono::milliseconds{1});
            timed.on_iteration(true);
            ++proposals;
        }
        ok &= expect(
            proposals == 3,
            "time-based temperature runs as many levels as it counts");
        ok &= expect(
            easylocal::runners::detail::temperature_level_count(1.0, 0.729, 0.9) == 3,
            "the level count is robust to rounding");

        // Temperatures whose ratio is not representable: the count comes from
        // the difference of the logarithms, and stays a number.
        const auto extreme_levels =
            easylocal::runners::detail::temperature_level_count(1e300, 1e-300, 0.95);
        ok &= expect(
            extreme_levels > 0
                && extreme_levels < (std::numeric_limits<std::size_t>::max)(),
            "the level count of temperatures orders of magnitude apart is finite");
    }

    {
        // Three levels, 8 -> 4 -> 2 -> 1, over three seconds.
        const temperature::TimeBasedParameters parameters{
            .initial_temperature = 8.0,
            .final_temperature = 1.0,
            .cooling_rate = 0.5,
            .allowed_running_time = 3.0,
        };
        using std::chrono::milliseconds;

        temperature::BasicTimeBased<ManualClock> policy{parameters};
        ok &= expect(
            policy.level_time() == milliseconds{1000},
            "time-based policy divides the running time among the levels");
        ManualClock::advance(milliseconds{500});
        policy.on_iteration(true);
        ok &= expect(
            policy.temperature() == 8.0,
            "time-based policy waits for its level time");
        ManualClock::advance(milliseconds{500});
        policy.on_iteration(false);
        ok &= expect(
            policy.temperature() == 4.0,
            "time-based policy cools when the level time is over");
        ManualClock::advance(milliseconds{1000});
        policy.on_iteration(false);
        ManualClock::advance(milliseconds{1000});
        policy.on_iteration(false);
        ok &= expect(policy.finished(), "time-based policy ends when the time is over");

        policy.reset();
        ok &= expect(
            !policy.finished() && policy.temperature() == 8.0,
            "time-based policy reset restarts the clock");
        auto cutoff_parameters = parameters;
        cutoff_parameters.accepted_per_temperature = 1;
        temperature::BasicTimeBased<ManualClock> quick{cutoff_parameters};
        for (int level = 0; level < 3; ++level)
        {
            ManualClock::advance(milliseconds{1});
            quick.on_iteration(true);
        }
        ok &= expect(
            quick.finished() && quick.temperature() == 1.0,
            "time-based policy ends at the final temperature");

        cutoff_parameters.accepted_per_temperature = 2;
        temperature::BasicTimeBased<ManualClock> cutoff{cutoff_parameters};
        ManualClock::advance(milliseconds{100});
        cutoff.on_iteration(true);
        ManualClock::advance(milliseconds{100});
        cutoff.on_iteration(true);
        ok &= expect(
            cutoff.temperature() == 4.0 && cutoff.level_time() == milliseconds{1400},
            "time-based cutoff cools early and redistributes the time it saves");
        // A time longer than the clock can count is as long as it can.
        const temperature::TimeBased endless{
            temperature::TimeBasedParameters{.allowed_running_time = 1e30}};
        ok &= expect(
            endless.level_time() > std::chrono::steady_clock::duration::zero()
                && !endless.finished(),
            "time-based SA holds an allowed time longer than the clock counts");
        ok &= expect(
            !temperature::TimeBasedParameters{.allowed_running_time = 0.0}.validate(),
            "time-based policy rejects a non-positive running time");
    }

    {
        // The first descent spends 6 of 12 iterations over three levels,
        // 8 -> 4 -> 2 -> 1; each of two reheats restarts from 4 and spends 3.
        using HybridReheating =
            temperature::ReheatingParameters<temperature::HybridParameters>;
        temperature::Reheating<temperature::Hybrid> policy{{
            .descent =
                {.initial_temperature = 8.0,
                    .final_temperature = 1.0,
                    .cooling_rate = 0.5,
                    .allowed_iterations = 12,
                    .accepted_ratio = 1.0},
            .allowed_reheats = 2,
            .reheat_ratio = 0.5,
            .first_descent_share = 0.5,
        }};
        ok &= expect(policy.temperature() == 8.0, "reheating starts at T0");
        for (int i = 0; i < 2; ++i)
            policy.on_iteration(false);
        ok &= expect(
            policy.temperature() == 4.0,
            "reheating cools within its first descent");
        for (int i = 0; i < 4; ++i)
            policy.on_iteration(false);
        ok &= expect(
            policy.reheats() == 1 && policy.temperature() == 4.0 && !policy.finished(),
            "reheating restarts from the reheat temperature after the first descent");
        for (int i = 0; i < 3; ++i)
            policy.on_iteration(false);
        ok &= expect(
            policy.reheats() == 2 && policy.temperature() == 4.0,
            "each reheat spends its share of the remaining iterations");
        for (int i = 0; i < 3; ++i)
            policy.on_iteration(false);
        ok &= expect(policy.finished(), "reheating ends after its last descent");
        policy.reset();
        ok &= expect(
            policy.reheats() == 0 && policy.temperature() == 8.0 && !policy.finished(),
            "reheating reset restarts the first descent");

        ok &= expect(
            static_cast<bool>(HybridReheating{}.validate())
                && static_cast<bool>(
                    HybridReheating{.allowed_reheats = 0, .first_descent_share = 1.0}
                        .validate())
                && !HybridReheating{.reheat_ratio = 0.0}.validate()
                && !HybridReheating{.first_descent_share = 1.0}.validate()
                && !HybridReheating{.descent = {.final_temperature = 6.0}, .reheat_ratio = 0.5}
                    .validate()
                && !HybridReheating{.descent = {.cooling_rate = 1.0}}.validate(),
            "reheating validates the descent, its reheat ratio and first-descent share");

        // Without a budget, each descent runs the whole schedule: Classic
        // cools 8 -> 4 -> 2 -> 1 after two samples per temperature.
        temperature::Reheating<temperature::Classic> classic{
            {.descent =
                    {.initial_temperature = 8.0,
                        .final_temperature = 1.0,
                        .cooling_rate = 0.5,
                        .samples_per_temperature = 2},
                .allowed_reheats = 1,
                .reheat_ratio = 0.5,
                .first_descent_share = 1.0}};
        std::size_t first_descent = 0;
        while (classic.reheats() == 0)
        {
            classic.on_iteration(false);
            ++first_descent;
        }
        ok &= expect(
            classic.temperature() == 4.0 && !classic.finished(),
            "a reheated schedule without a budget restarts it from the reheat temperature");
        std::size_t reheat = 0;
        while (!classic.finished())
        {
            classic.on_iteration(false);
            ++reheat;
        }
        ok &= expect(
            first_descent == 6 && reheat == 4,
            "each descent of a schedule without a budget runs it whole");

        // The reheats share what the first descent leaves, at least one
        // proposal each: 9 of 10 leave 1, too few for 3 reheats.
        using FixedReheating =
            temperature::ReheatingParameters<temperature::FixedLengthParameters>;
        const FixedReheating tight{
            .descent = {.allowed_iterations = 10},
            .allowed_reheats = 1,
            .first_descent_share = 0.9};
        temperature::Reheating<temperature::FixedLength> spent{tight};
        std::size_t proposals = 0;
        while (!spent.finished() && proposals < 100)
        {
            spent.on_iteration(false);
            ++proposals;
        }
        ok &= expect(
            proposals == 10,
            "the descents of a reheated schedule spend its budget, no more");
        auto overspent = tight;
        overspent.allowed_reheats = 3;
        ok &= expect(
            !overspent.validate(),
            "reheating rejects more reheats than the proposals the first descent leaves");

        // A time budget is divided like an iteration budget.
        temperature::Reheating<temperature::TimeBased> timed{
            {.descent = {.allowed_running_time = 8.0},
                .allowed_reheats = 2,
                .first_descent_share = 0.5}};
        ok &= expect(
            timed.descent().parameters().allowed_running_time == 4.0,
            "the first descent spends its share of the running time");
    }

    {
        using easylocal::cost::approximately_equal;
        constexpr auto tolerance =
            easylocal::cost::tolerance{.relative = 1e-12, .absolute = 0.0};
        const auto expected = 3.0 / std::log(2.0);

        const std::array<double, 4>
            deltas{-1.0, 2.0, 4.0, std::numeric_limits<double>::infinity()};
        const auto estimate =
            easylocal::runners::detail::estimate_temperature(deltas, 0.5);
        ok &= expect(
            estimate.has_value() && approximately_equal(*estimate, expected, tolerance),
            "the estimate averages the finite worsening deltas (Johnson et al.)");
        const std::array<double, 2> improving{-1.0, 0.0};
        ok &= expect(
            !easylocal::runners::detail::estimate_temperature(improving, 0.5).has_value(),
            "no estimate without worsening moves");

        // Deltas whose sum, and whose temperature, are not representable:
        // the mean is kept as it goes, and an estimate that is not a number
        // leaves the configured initial temperature in place.
        const auto huge = (std::numeric_limits<double>::max)();
        const std::array<double, 2> huge_deltas{huge, huge};
        ok &= expect(
            !easylocal::runners::detail::estimate_temperature(huge_deltas, 0.5)
                .has_value(),
            "no estimate from deltas whose temperature is not representable");
        const std::array<double, 2> large_deltas{1e307, 3e307};
        const auto large_estimate =
            easylocal::runners::detail::estimate_temperature(large_deltas, 0.5);
        ok &= expect(
            large_estimate.has_value() && std::isfinite(*large_estimate),
            "deltas whose sum overflows but whose mean does not are averaged");

        temperature::Classic uncalibrated{temperature::ClassicParameters{
            .initial_temperature = 7.0,
            .calibration_samples = 10,
        }};
        uncalibrated.calibrate(huge_deltas);
        ok &= expect(
            uncalibrated.parameters().initial_temperature == 7.0,
            "a calibration without an estimate keeps the configured temperature");

        temperature::Classic classic{
            temperature::ClassicParameters{.calibration_samples = 10}};
        classic.calibrate(deltas);
        ok &= expect(
            approximately_equal(classic.temperature(), expected, tolerance)
                && approximately_equal(
                    classic.parameters().initial_temperature,
                    expected,
                    tolerance),
            "a calibrated policy starts from the estimated temperature");

        temperature::Classic clamped{temperature::ClassicParameters{
            .final_temperature = 1.0,
            .cooling_rate = 0.5,
            .calibration_samples = 10}};
        const std::array<double, 1> tiny{0.001};
        clamped.calibrate(tiny);
        ok &= expect(
            clamped.temperature() == 2.0,
            "a calibrated temperature keeps at least one cooling level");

        temperature::Classic unchanged{
            temperature::ClassicParameters{.calibration_samples = 10}};
        unchanged.calibrate(improving);
        ok &= expect(
            unchanged.temperature() == 10.0,
            "without worsening moves the configured temperature stays");

        temperature::FixedTemperature fixed{
            temperature::FixedTemperatureParameters{.calibration_samples = 10}};
        fixed.calibrate(deltas);
        ok &= expect(
            approximately_equal(fixed.temperature(), expected, tolerance),
            "fixed temperature calibrates its constant temperature");

        temperature::Reheating<temperature::Hybrid> reheating{
            {.descent =
                    {.final_temperature = 1.0,
                        .cooling_rate = 0.5,
                        .calibration_samples = 10},
                .reheat_ratio = 0.5}};
        reheating.calibrate(tiny);
        ok &= expect(
            reheating.temperature() == 4.0,
            "a calibrated reheating keeps its reheat temperature above the final one");

        ok &= expect(
            !temperature::ClassicParameters{
                .calibration_samples = 10,
                .initial_acceptance = 1.0}
                    .validate()
                && !temperature::
                    TimeBasedParameters{.calibration_samples = 10, .initial_acceptance = 0.0}
                        .validate(),
            "calibrating policies reject an initial acceptance outside (0, 1)");
    }

    {
        // A worsening by delta at temperature T is accepted with probability
        // exp(-delta / T): measured over many draws.
        MetropolisAcceptance metropolis;
        std::mt19937_64 rng{2026U};
        const auto rate = [&](const int worse, const double temperature) {
            constexpr int draws = 200'000;
            int accepted = 0;
            for (int draw = 0; draw < draws; ++draw)
                accepted += metropolis.accept(10 + worse, 10, temperature, rng) ? 1 : 0;
            return static_cast<double>(accepted) / draws;
        };
        ok &= expect(
            std::abs(rate(1, 1.0) - std::exp(-1.0)) < 0.005
                && std::abs(rate(2, 1.0) - std::exp(-2.0)) < 0.005
                && std::abs(rate(1, 4.0) - std::exp(-0.25)) < 0.005,
            "Metropolis accepts a worsening move with probability exp(-delta / T)");
    }

    {
        MetropolisAcceptance metropolis;
        CountingEngine rng;
        ok &= expect(metropolis.accept(9, 10, 2.0, rng),
            "Metropolis accepts a strict numeric improvement");
        ok &= expect(metropolis.accept(10, 10, 2.0, rng),
            "Metropolis accepts equal numeric energy");
        ok &= expect(rng.calls == 0,
            "Metropolis improvement and equality do not consume RNG state");
        (void)metropolis.accept(11, 10, 2.0, rng);
        ok &= expect(rng.calls > 0,
            "Metropolis worsening branch consumes RNG state");

        // The difference of the extremes of an int is not an int: computed in
        // the cost type it would wrap around and read as an improvement.
        ok &= expect(
            !metropolis.accept(
                (std::numeric_limits<int>::max)(),
                (std::numeric_limits<int>::min)(),
                1.0,
                rng),
            "Metropolis rejects the worsening from the lowest to the highest int");
        const auto worsening_calls = rng.calls;
        ok &= expect(
            metropolis.accept(
                (std::numeric_limits<int>::min)(),
                (std::numeric_limits<int>::max)(),
                1.0,
                rng)
                && rng.calls == worsening_calls,
            "Metropolis accepts the improvement from the highest to the lowest int");
    }

    {
        MetropolisAcceptance metropolis;
        CountingEngine rng;
        using easylocal::cost::hierarchical;

        const auto current = hierarchical{0, 10.0};
        const auto soft_improvement = hierarchical{0, 9.0};
        const auto hard_improvement = hierarchical{-1, 1000.0};
        const auto hard_worsening = hierarchical{1, -1000.0};

        // The delta is a double, so that Metropolis needs no long double
        // arithmetic, unless the soft delta is a long double.
        static_assert(
            std::same_as<decltype(easylocal::cost::delta(current, current)), double>);
        static_assert(std::same_as<
            decltype(easylocal::cost::delta(
                hierarchical{0, 1.0L},
                hierarchical{0, 1.0L})),
            long double>);

        ok &= expect(
            metropolis.accept(soft_improvement, current, 2.0, rng),
            "Metropolis accepts a hierarchical soft improvement");
        ok &= expect(
            metropolis.accept(hard_improvement, current, 2.0, rng),
            "Metropolis unconditionally accepts a hierarchical hard improvement");
        ok &= expect(
            !metropolis.accept(hard_worsening, current, 2.0, rng),
            "Metropolis unconditionally rejects a hierarchical hard worsening");
        ok &= expect(
            rng.calls == 0,
            "hierarchical hard boundaries and improvements do not consume RNG state");

        const auto before = rng.calls;
        (void)metropolis.accept(hierarchical{0, 11.0}, current, 2.0, rng);
        ok &= expect(
            rng.calls > before,
            "hierarchical soft worsening uses probabilistic Metropolis acceptance");
    }

    {
        const ChainInstance instance;
        auto runner =
            easylocal::make_runner<
                SimulatedAnnealing<temperature::FixedLength, AlwaysAccept>>({
                .temperature =
                    temperature::FixedLengthParameters{
                        .initial_temperature = 4.0,
                        .final_temperature = 1.0,
                        .cooling_rate = 0.5,
                        .allowed_iterations = 2,
                    },
            })
            | (solution_manager<ChainSolutionManager>() | component<ChainValue>())
            | neighborhood<RandomOnlyChainNeighborhood>();

        std::mt19937 rng{7U};
        const auto result = runner.bind(instance).run(ChainSolution{}, rng);
        ok &= expect(result.solution.value == -5 && result.cost == -5,
            "SA returns best-so-far rather than the final accepted chain state");
        ok &= expect(result.iterations == 2 && result.evaluations == 3,
            "SA reports proposal iterations separately from evaluations including the initial state");
        ok &= expect(
            result.termination == easylocal::termination_reason::completed,
            "SA ends completed when its schedule is over");

        // An acceptance that needs no delta runs on any ordered cost, but a
        // calibration estimates the temperature from deltas: it is rejected.
        auto calibrating =
            easylocal::make_runner<
                SimulatedAnnealing<temperature::Classic, AlwaysAccept>>(
                {.temperature = temperature::ClassicParameters{.calibration_samples = 4}})
            | (solution_manager<ChainSolutionManager>() | component<OrderedChainValue>())
            | neighborhood<RandomOnlyChainNeighborhood>();
        std::mt19937 calibration_rng{7U};
        bool rejected_calibration = false;
        try
        {
            (void)calibrating.bind(instance).run(ChainSolution{}, calibration_rng);
        }
        catch (const std::invalid_argument&)
        {
            rejected_calibration = true;
        }
        ok &= expect(
            rejected_calibration,
            "a calibration on a cost without cost::delta is rejected");

        // Its own evaluation budget, as every runner has.
        auto budgeted =
            easylocal::make_runner<
                SimulatedAnnealing<temperature::FixedLength, AlwaysAccept>>({
                .temperature =
                    temperature::FixedLengthParameters{
                        .initial_temperature = 4.0,
                        .final_temperature = 1.0,
                        .cooling_rate = 0.5,
                        .allowed_iterations = 2,
                    },
                .max_evaluations = 2,
            })
            | (solution_manager<ChainSolutionManager>() | component<ChainValue>())
            | neighborhood<RandomOnlyChainNeighborhood>();
        std::mt19937 budget_rng{7U};
        const auto spent = budgeted.bind(instance).run(ChainSolution{}, budget_rng);
        ok &= expect(
            spent.evaluations == 2
                && spent.termination
                    == easylocal::termination_reason::evaluation_budget_exhausted,
            "SA stops at its max_evaluations");

        std::mt19937 trace_rng{7U};
        easylocal::trace::memory_recorder<int> trace;
        const auto traced = runner.bind(instance).run(ChainSolution{}, trace_rng, easylocal::with(trace));
        bool saw_incumbent = false;
        bool saw_accepted = false;
        for (const auto& record : trace.records())
        {
            saw_incumbent = saw_incumbent || std::holds_alternative<
                easylocal::trace::memory_recorder<int>::incumbent_updated_record>(record);
            saw_accepted = saw_accepted || std::holds_alternative<
                easylocal::trace::memory_recorder<int>::move_accepted_record>(record);
        }
        ok &= expect(
            traced.solution.value == result.solution.value && saw_incumbent && saw_accepted,
            "SA tracing records accepted moves and best-so-far updates without changing semantics");

        // The temperature at the start, then at each cooling: 4, then 2 after
        // the first of the two proposals (the schedule ends at the second).
        std::vector<std::pair<double, double>> temperatures;
        for (const auto& record : trace.records())
            if (const auto* change = std::get_if<
                    easylocal::trace::memory_recorder<int>::temperature_changed_record>(
                    &record))
                temperatures.emplace_back(
                    change->previous_temperature,
                    change->temperature);
        ok &= expect(
            temperatures
                == std::vector<std::pair<double, double>>{{0.0, 4.0}, {4.0, 2.0}},
            "SA traces its temperature at the start and at each cooling");

        std::stop_source stop;
        std::size_t observations = 0;
        auto observer = [&](const easylocal::run_progress& progress) {
            ++observations;
            if (progress.evaluations >= 2)
            {
                stop.request_stop();
            }
        };
        const easylocal::run_control control{stop.get_token(), observer};
        std::mt19937 controlled_rng{7U};
        const auto controlled =
            runner.bind(instance).run(
                ChainSolution{}, controlled_rng, easylocal::with(control));
        ok &= expect(
            observations == 2 && controlled.iterations == 1 &&
                controlled.evaluations == 2 &&
                controlled.termination == easylocal::termination_reason::cancelled,
            "SA controlled execution reports progress and cooperatively stops");

        // The stop arrives at the last proposal, when the schedule is over
        // too: the loop never checks it again, and the run still reports the
        // cancellation rather than a completed schedule.
        std::stop_source late_stop;
        auto late_observer = [&](const easylocal::run_progress& progress) {
            if (progress.evaluations >= 3)
                late_stop.request_stop();
        };
        const easylocal::run_control late_control{late_stop.get_token(), late_observer};
        std::mt19937 late_rng{7U};
        const auto late_cancelled = runner.bind(instance).run(
            ChainSolution{},
            late_rng,
            easylocal::with(late_control));
        ok &= expect(
            late_cancelled.evaluations == 3
                && late_cancelled.termination == easylocal::termination_reason::cancelled,
            "a stop at the last proposal of SA is reported, not a completed schedule");
    }

    {
        const auto instance = exam_instance();
        const exam::ExamTimetable initial{
            .timeslot_by_exam = {0, 0, 1, 1, 2},
        };

        const auto solution_manager_recipe =
            solution_manager<exam::ExamTimetablingSolutionManager>()
            | easylocal::cost::sum(
                  easylocal::cost::weighted(
                      component<exam::StudentConflictComponent>(), 1000),
                  easylocal::cost::weighted(
                      component<exam::ConsecutiveExamComponent>(), 10),
                  component<exam::TimeslotLoadComponent>());

        auto runner =
            easylocal::make_runner<SimulatedAnnealing<temperature::FixedLength>>(
                {.temperature =
                        temperature::FixedLengthParameters{
                            .initial_temperature = 100.0,
                            .final_temperature = 1.0,
                            .cooling_rate = 0.5,
                            .allowed_iterations = 30,
                        }})
            | solution_manager_recipe
            | (neighborhood<exam::MoveExamNeighborhoodExplorer>()
                | delta<exam::StudentConflictComponent>()
                | delta<exam::ConsecutiveExamComponent, exam::ConsecutiveExamDelta>()
                | delta<exam::TimeslotLoadComponent, exam::TimeslotLoadDelta>());

        auto bound_runner = runner.bind(instance);
        std::mt19937 rng_a{2026U};
        std::mt19937 rng_b{2026U};
        const auto result_a = bound_runner.run(initial, rng_a);
        const auto result_b = bound_runner.run(initial, rng_b);

        ok &= expect(
            result_a.solution == result_b.solution &&
            result_a.cost == result_b.cost &&
            result_a.iterations == result_b.iterations,
            "exam timetabling SA is deterministic for the same explicit RNG state");

        const exam::ExamTimetablingSolutionManager manager{instance};
        const exam::StudentConflictComponent conflicts{instance};
        const exam::ConsecutiveExamComponent consecutive{instance};
        const exam::TimeslotLoadComponent load{instance};
        const auto full_cost =
            1000 * conflicts.evaluate(result_a.solution) +
            10 * consecutive.evaluate(result_a.solution) +
            load.evaluate(result_a.solution);

        ok &= expect(result_a.cost == full_cost,
            "three-component weighted SA cost agrees with full evaluation");
    }

    {
        const auto instance = exam_instance();
        const exam::ExamTimetable initial{
            .timeslot_by_exam = {0, 0, 1, 1, 2},
        };
        const auto solution_manager_recipe =
            solution_manager<exam::ExamTimetablingSolutionManager>()
            | easylocal::cost::sum(
                easylocal::cost::weighted(
                    component<exam::StudentConflictComponent>(),
                    1000),
                easylocal::cost::weighted(
                    component<exam::ConsecutiveExamComponent>(),
                    10),
                component<exam::TimeslotLoadComponent>());
        const auto neighborhood_recipe =
            neighborhood<exam::MoveExamNeighborhoodExplorer>();

        auto timed =
            easylocal::make_runner<SimulatedAnnealing<temperature::TimeBased>>(
                {.temperature =
                        temperature::TimeBasedParameters{.allowed_running_time = 0.02}})
            | solution_manager_recipe | neighborhood_recipe;
        std::mt19937 rng{2026U};
        const auto start = std::chrono::steady_clock::now();
        const auto timed_result = timed.bind(instance).run(initial, rng);
        ok &= expect(
            timed_result.iterations > 0
                && std::chrono::steady_clock::now() - start < std::chrono::seconds{5},
            "time-based SA runs for its allowed time");

        std::vector<double> recorded;
        auto recording = Runner{SimulatedAnnealing{RecordingPolicy{recorded, 20}}}
            | solution_manager_recipe | neighborhood_recipe;
        const auto recorded_result = recording.bind(instance).run(initial, rng);
        ok &= expect(
            recorded.size() == 20 && recorded_result.iterations == 5
                && recorded_result.evaluations == 1 + 20 + 5,
            "SA calibrates on sampled moves, counted as evaluations but not iterations");

        auto calibrated =
            easylocal::make_runner<SimulatedAnnealing<temperature::FixedLength>>(
                {.temperature =
                        temperature::FixedLengthParameters{
                            .allowed_iterations = 40,
                            .calibration_samples = 30}})
            | solution_manager_recipe | neighborhood_recipe;
        std::mt19937 rng_a{11U};
        std::mt19937 rng_b{11U};
        const auto calibrated_a = calibrated.bind(instance).run(initial, rng_a);
        const auto calibrated_b = calibrated.bind(instance).run(initial, rng_b);
        ok &= expect(
            calibrated_a.solution == calibrated_b.solution
                && calibrated_a.iterations == 40
                && calibrated_a.evaluations == 1 + 30 + 40,
            "calibrated SA is deterministic for the same RNG state");

        auto fixed =
            easylocal::make_runner<SimulatedAnnealing<temperature::FixedTemperature>>(
                {.temperature =
                        temperature::FixedTemperatureParameters{
                            .allowed_iterations = 50}})
            | solution_manager_recipe | neighborhood_recipe;
        const auto fixed_result = fixed.bind(instance).run(initial, rng);
        ok &= expect(
            fixed_result.iterations == 50,
            "fixed-temperature SA spends its iteration budget");
    }

    return ok ? 0 : 1;
}
