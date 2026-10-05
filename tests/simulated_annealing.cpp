#include "cost_components.hpp"
#include "cost_deltas.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"
#include "support/approximate.hpp"
#include "support/exam_timeslot_load_delta.hpp"

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/trace.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
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

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }
    return true;
}


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
                    .max_iterations = 200,
                }};

        easylocal::config::parameter_set configuration;
        configuration.add(annealing);
        const auto max_iterations = [&] {
            for (const auto& parameter : configuration.parameters())
                if (parameter.path == "temperature.max_iterations")
                    return parameter.value;
            return std::string{};
        };
        ok &= expect(
            max_iterations() == "200",
            "SA configuration exposes the nested temperature policy parameters");

        const std::array update{
            easylocal::config::text_override{"temperature.max_iterations", "20"}};
        ok &= expect(
            static_cast<bool>(configuration.apply(update)),
            "SA configuration can update its nested temperature policy");
        ok &= expect(
            max_iterations() == "20",
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
            .max_iterations = 12,
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
            .max_iterations = 12,
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
        temperature::Hybrid policy{temperature::HybridParameters{
            .initial_temperature = 8.0,
            .final_temperature = 1.0,
            .cooling_rate = 0.5,
            .max_iterations = 12,
            .accepted_ratio = 0.5,
        }};
        ok &= expect(policy.sample_limit() == 4 && policy.accepted_limit() == 2,
            "hybrid policy starts with sampled and accepted limits");
        policy.on_iteration(true);
        policy.on_iteration(true);
        ok &= expect(policy.temperature() == 4.0,
            "hybrid policy applies the accepted cutoff early");
        ok &= expect(policy.sample_limit() == 5,
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
            .max_iterations = 4,
            .accepted_ratio = 0.5,
        }};
        ok &= expect(
            policy.temperature() == 2.0 && policy.accepted_limit() == 2,
            "fixed temperature derives its accepted limit from the ratio");
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
        ok &= expect(
            !temperature::FixedTemperatureParameters{.temperature = 0.0}.validate()
                && !temperature::FixedTemperatureParameters{.accepted_ratio = 1.5}
                    .validate(),
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
                    .max_iterations = 12,
                    .accepted_ratio = 1.0},
            .max_reheats = 2,
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
                    HybridReheating{.max_reheats = 0, .first_descent_share = 1.0}
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
                .max_reheats = 1,
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

        // A time budget is divided like an iteration budget.
        temperature::Reheating<temperature::TimeBased> timed{
            {.descent = {.allowed_running_time = 8.0},
                .max_reheats = 2,
                .first_descent_share = 0.5}};
        ok &= expect(
            timed.descent().parameters().allowed_running_time == 4.0,
            "the first descent spends its share of the running time");
    }

    {
        using easylocal::test_support::approximately_equal;
        constexpr auto tolerance = easylocal::test_support::ApproximateTolerance{
            .relative = 1e-12,
            .absolute = 0.0};
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
    }

    {
        MetropolisAcceptance metropolis;
        CountingEngine rng;
        using easylocal::cost::hierarchical;

        const auto current = hierarchical{0, 10.0};
        const auto soft_improvement = hierarchical{0, 9.0};
        const auto hard_improvement = hierarchical{-1, 1000.0};
        const auto hard_worsening = hierarchical{1, -1000.0};

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
                        .max_iterations = 2,
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

        // Its own evaluation budget, as every runner has.
        auto budgeted =
            easylocal::make_runner<
                SimulatedAnnealing<temperature::FixedLength, AlwaysAccept>>({
                .temperature =
                    temperature::FixedLengthParameters{
                        .initial_temperature = 4.0,
                        .final_temperature = 1.0,
                        .cooling_rate = 0.5,
                        .max_iterations = 2,
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
                            .max_iterations = 30,
                        }})
            | solution_manager_recipe
            | (neighborhood<exam::MoveExamNeighborhoodExplorer>()
                | delta<exam::StudentConflictComponent>()
                | delta<
                    exam::ConsecutiveExamComponent,
                    exam::ConsecutiveExamDeltaEvaluator>()
                | delta<exam::TimeslotLoadComponent, exam::TimeslotLoadDeltaEvaluator>());

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
                            .max_iterations = 40,
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
                        temperature::FixedTemperatureParameters{.max_iterations = 50}})
            | solution_manager_recipe | neighborhood_recipe;
        const auto fixed_result = fixed.bind(instance).run(initial, rng);
        ok &= expect(
            fixed_result.iterations == 50,
            "fixed-temperature SA spends its iteration budget");
    }

    return ok ? 0 : 1;
}
