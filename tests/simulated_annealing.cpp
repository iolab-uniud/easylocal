#include "cost_components.hpp"
#include "cost_deltas.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"
#include "support/approximate.hpp"

#include <easylocal/aggregation.hpp>
#include <easylocal/runner.hpp>
#include <easylocal/config/tree.hpp>
#include <easylocal/search/metropolis_acceptance.hpp>
#include <easylocal/search/simulated_annealing.hpp>
#include <easylocal/search/temperature_policy.hpp>

#include <cstddef>
#include <cstdint>
#include <array>
#include <iostream>
#include <optional>
#include <random>
#include <string_view>
#include <type_traits>

namespace
{

using namespace easylocal;
using namespace easylocal::search;
namespace exam = easylocal::mwe::exam_timetabling;

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

    auto operator()() noexcept -> result_type
    {
        ++calls;
        return max();
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
    using instance_type = ChainInstance;
    using solution_type = ChainSolution;
    using cost_type = int;

    explicit ChainSolutionManager(const ChainInstance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]] auto instance() const noexcept -> const ChainInstance&
    {
        return instance_;
    }

    [[nodiscard]] static auto is_valid(const ChainSolution&) noexcept -> bool
    {
        return true;
    }

    [[nodiscard]] static auto evaluate(const ChainSolution& solution) noexcept -> int
    {
        return solution.value;
    }

private:
    const ChainInstance& instance_;
};

class RandomOnlyChainNeighborhood
{
public:
    using instance_type = ChainInstance;
    using solution_type = ChainSolution;
    using move_type = ChainMove;

    explicit RandomOnlyChainNeighborhood(
        const ChainSolutionManager& solution_manager) noexcept
        : instance_{solution_manager.instance()}
    {
    }

    [[nodiscard]] auto instance() const noexcept -> const ChainInstance&
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

    static void make_move(
        ChainSolution& solution,
        const ChainMove& move) noexcept
    {
        solution.value += move.delta;
    }

private:
    const ChainInstance& instance_;
};

static_assert(easylocal::detail::runner_neighborhood_explorer<
              RandomOnlyChainNeighborhood,
              ChainSolutionManager>);
static_assert(!easylocal::detail::enumerable_runner_neighborhood_explorer<
              RandomOnlyChainNeighborhood,
              ChainSolutionManager>);
static_assert(numeric_cost<int>);
static_assert(numeric_cost<double>);
static_assert(!numeric_cost<bool>);
struct StructuredCost { int hard; int soft; };
static_assert(!numeric_cost<StructuredCost>);

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }
    return true;
}

template<class Path, std::size_t Size>
[[nodiscard]]
consteval auto path_is(const std::array<std::string_view, Size>& expected)
    -> bool
{
    constexpr auto actual = Path::segments();
    if constexpr (actual.size() != Size)
    {
        return false;
    }
    else
    {
        return actual == expected;
    }
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
        SimulatedAnnealing annealing{
            temperature::FixedLength{temperature::FixedLengthParameters{
                .initial_temperature = 8.0,
                .final_temperature = 0.25,
                .cooling_rate = 0.75,
                .max_iterations = 200,
            }}};

        const auto configuration = easylocal::config::root(
            annealing.configuration());

        bool saw_max_iterations = false;
        easylocal::config::for_each_config_parameter(
            configuration,
            [&](const auto path, const auto, const auto& value) {
                using path_type = std::remove_cvref_t<decltype(path)>;
                if constexpr (path_is<path_type>(
                                  std::array<std::string_view, 3>{
                                      "search",
                                      "temperature",
                                      "max_iterations"}))
                {
                    saw_max_iterations = value == 200;
                }
            });

        ok &= expect(
            saw_max_iterations,
            "SA configuration exposes the nested temperature policy parameters");

        const auto& temperature_endpoint =
            easylocal::config::at<"search", "temperature">(configuration);
        auto updated = temperature_endpoint.parameters();
        updated.max_iterations = 20;
        ok &= expect(
            static_cast<bool>(temperature_endpoint.configure(updated)),
            "SA configuration can update its nested temperature policy");
        ok &= expect(
            temperature_endpoint.parameters().max_iterations == 20,
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
        using easylocal::aggregation::hierarchical;

        const auto current = hierarchical{}(0, 10.0);
        const auto soft_improvement = hierarchical{}(0, 9.0);
        const auto hard_improvement = hierarchical{}(-1, 1000.0);
        const auto hard_worsening = hierarchical{}(1, -1000.0);

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
        (void)metropolis.accept(hierarchical{}(0, 11.0), current, 2.0, rng);
        ok &= expect(
            rng.calls > before,
            "hierarchical soft worsening uses probabilistic Metropolis acceptance");
    }

    {
        const ChainInstance instance;
        auto runner =
            Runner{SimulatedAnnealing{
                temperature::FixedLength{temperature::FixedLengthParameters{
                    .initial_temperature = 4.0,
                    .final_temperature = 1.0,
                    .cooling_rate = 0.5,
                    .max_iterations = 2,
                }},
                AlwaysAccept{}}}
            | solution_manager<ChainSolutionManager>()
            | neighborhood<RandomOnlyChainNeighborhood>();

        std::mt19937 rng{7U};
        const auto result = runner.bind(instance).run(ChainSolution{}, rng);
        ok &= expect(result.solution.value == -5 && result.cost == -5,
            "SA returns best-so-far rather than the final accepted chain state");
        ok &= expect(result.iterations == 2 && result.evaluations == 3,
            "SA reports proposal iterations separately from evaluations including the initial state");
    }

    {
        const auto instance = exam_instance();
        const exam::ExamTimetable initial{
            .timeslot_by_exam = {0, 0, 1, 1, 2},
        };

        const auto solution_manager_recipe =
            solution_manager<exam::ExamTimetablingSolutionManager>()
            | component<exam::StudentConflictComponent>()
            | component<exam::ConsecutiveExamComponent>()
            | component<exam::TimeslotLoadComponent>()
            | easylocal::aggregator(easylocal::aggregation::weighted_sum{
                  exam::penalty_type{1000},
                  exam::penalty_type{10},
                  exam::penalty_type{1}});

        auto runner =
            Runner{SimulatedAnnealing{
                temperature::FixedLength{temperature::FixedLengthParameters{
                    .initial_temperature = 100.0,
                    .final_temperature = 1.0,
                    .cooling_rate = 0.5,
                    .max_iterations = 30,
                }}}}
            | solution_manager_recipe
            | (neighborhood<exam::MoveExamNeighborhoodExplorer>()
               | delta<
                     exam::StudentConflictComponent,
                     exam::StudentConflictDeltaEvaluator>()
               | delta<
                     exam::ConsecutiveExamComponent,
                     exam::ConsecutiveExamDeltaEvaluator>()
               | delta<
                     exam::TimeslotLoadComponent,
                     exam::TimeslotLoadDeltaEvaluator>());

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
        const auto aggregate = easylocal::aggregation::weighted_sum{
            exam::penalty_type{1000},
            exam::penalty_type{10},
            exam::penalty_type{1},
        };
        const auto full_cost = aggregate(
            conflicts.evaluate(result_a.solution),
            consecutive.evaluate(result_a.solution),
            load.evaluate(result_a.solution));

        ok &= expect(result_a.cost == full_cost,
            "three-component weighted SA cost agrees with full evaluation");
    }

    return ok ? 0 : 1;
}
