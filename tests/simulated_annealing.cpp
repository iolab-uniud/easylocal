#include "assignment/capacity_delta.hpp"
#include "assignment/cost_components.hpp"
#include "assignment/neighborhood_explorer.hpp"
#include "assignment/solution_manager.hpp"
#include "search/metropolis_acceptance.hpp"
#include "search/simulated_annealing.hpp"
#include "support/approximate.hpp"
#include "tsp/neighborhood_explorer.hpp"
#include "tsp/solution_manager.hpp"
#include "tsp/tour_length_component.hpp"
#include "tsp/tour_length_delta.hpp"

#include <easylocal/runner.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <string_view>
#include <utility>

namespace
{

namespace assignment = easylocal::mwe::assignment;
namespace search = easylocal::mwe::search;
namespace tsp = easylocal::mwe::tsp;

using easylocal::test_support::ApproximateTolerance;
using easylocal::test_support::approximately_equal;

struct AlwaysAccept
{
    template<class Cost, class RNG>
    [[nodiscard]]
    constexpr auto accept(
        const Cost&,
        const Cost&,
        const double,
        RNG&) const noexcept -> bool
    {
        return true;
    }
};

struct AlwaysReject
{
    template<class Cost, class RNG>
    [[nodiscard]]
    constexpr auto accept(
        const Cost&,
        const Cost&,
        const double,
        RNG&) const noexcept -> bool
    {
        return false;
    }
};

struct IdentityEnergy
{
    [[nodiscard]]
    constexpr auto operator()(const double cost) const noexcept -> double
    {
        return cost;
    }
};

class AssignmentEnergy
{
public:
    explicit constexpr AssignmentEnergy(
        const std::int64_t secondary_span) noexcept
        : secondary_span_{secondary_span}
    {
    }

    [[nodiscard]]
    constexpr auto operator()(const assignment::Cost& cost) const noexcept
        -> std::int64_t
    {
        return cost.template get<0>() * secondary_span_ +
               cost.template get<1>();
    }

private:
    std::int64_t secondary_span_;
};

struct ApproximateEquivalent
{
    ApproximateTolerance tolerance;

    [[nodiscard]]
    auto operator()(const double lhs, const double rhs) const noexcept -> bool
    {
        return approximately_equal(lhs, rhs, tolerance);
    }
};

class CountingEngine
{
public:
    using result_type = std::uint32_t;

    [[nodiscard]]
    static constexpr auto min() noexcept -> result_type
    {
        return std::numeric_limits<result_type>::min();
    }

    [[nodiscard]]
    static constexpr auto max() noexcept -> result_type
    {
        return std::numeric_limits<result_type>::max();
    }

    [[nodiscard]]
    auto operator()() noexcept -> result_type
    {
        ++calls;
        return result_type{0x80000000U};
    }

    std::size_t calls{0};
};

class CountingTspNeighborhoodExplorer : public tsp::NeighborhoodExplorer
{
public:
    CountingTspNeighborhoodExplorer(
        const tsp::SolutionManager& solution_manager,
        int& make_move_count) noexcept
        : tsp::NeighborhoodExplorer{solution_manager},
          make_move_count_{make_move_count}
    {
    }

    void make_move(
        tsp::Solution& solution,
        const tsp::TwoOptMove& move) const noexcept
    {
        ++make_move_count_;
        tsp::NeighborhoodExplorer::make_move(solution, move);
    }

private:
    int& make_move_count_;
};

[[nodiscard]]
auto tsp_instance() -> tsp::Instance
{
    return tsp::Instance{
        .city_count = 5,
        .distances = {
            0.0, 0.1, 0.1, 0.3, 0.1,
            0.1, 0.0, 0.1, 0.1, 0.4,
            0.1, 0.1, 0.0, 0.2, 0.5,
            0.3, 0.1, 0.2, 0.0, 0.1,
            0.1, 0.4, 0.5, 0.1, 0.0,
        },
    };
}

[[nodiscard]]
auto empty_tsp_instance() -> tsp::Instance
{
    return tsp::Instance{
        .city_count = 3,
        .distances = {
            0.0, 1.0, 2.0,
            1.0, 0.0, 1.5,
            2.0, 1.5, 0.0,
        },
    };
}

[[nodiscard]]
auto assignment_instance() -> assignment::Instance
{
    return assignment::Instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };
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
    using easylocal::Runner;
    using easylocal::component;
    using easylocal::delta;
    using easylocal::neighborhood;
    using easylocal::solution_manager;

    bool ok = true;

    const auto tsp_problem = tsp_instance();
    const tsp::Solution tsp_initial{
        .tour = {0, 2, 1, 3, 4},
    };
    constexpr ApproximateTolerance tsp_tolerance{
        .relative = 1.0e-12,
        .absolute = 1.0e-12,
    };

    const auto tsp_manager_recipe =
        solution_manager<tsp::SolutionManager>()
        | component<tsp::TourLengthComponent>();

    auto budget_one_runner =
        Runner{search::SimulatedAnnealing{
            search::SimulatedAnnealingParameters{
                .max_evaluations = 1,
                .initial_temperature = 1.0,
                .cooling_factor = 0.9,
            },
            AlwaysAccept{}}}
        | tsp_manager_recipe
        | neighborhood<tsp::NeighborhoodExplorer>();

    CountingEngine budget_one_rng;
    const auto budget_one_result =
        budget_one_runner.bind(tsp_problem).run(tsp_initial, budget_one_rng);

    ok &= expect(
        budget_one_result.evaluations == 1,
        "SA initial evaluation counts against the budget");
    ok &= expect(
        budget_one_rng.calls == 0,
        "SA with budget one does not sample a random proposal");

    int rejected_make_moves = 0;
    auto rejected_runner =
        Runner{search::SimulatedAnnealing{
            search::SimulatedAnnealingParameters{
                .max_evaluations = 2,
                .initial_temperature = 1.0,
                .cooling_factor = 0.9,
            },
            AlwaysReject{}}}
        | tsp_manager_recipe
        | (neighborhood<CountingTspNeighborhoodExplorer>(
               std::ref(rejected_make_moves))
           | delta<
                 tsp::TourLengthComponent,
                 tsp::TwoOptTourLengthDeltaEvaluator>());

    std::mt19937 reject_rng{7U};
    const auto rejected_result =
        rejected_runner.bind(tsp_problem).run(tsp_initial, reject_rng);

    ok &= expect(
        rejected_result.solution.tour == tsp_initial.tour,
        "SA rejection leaves the incumbent TSP solution unchanged");
    ok &= expect(
        rejected_make_moves == 0,
        "rejected all-delta SA candidate does not materialize a Solution");
    ok &= expect(
        rejected_result.evaluations == 2,
        "SA counts the initial evaluation and one rejected proposal");
    ok &= expect(
        rejected_result.termination ==
            search::SimulatedAnnealingTermination::evaluation_budget_exhausted,
        "non-empty SA run stops when the evaluation budget is exhausted");

    int accepted_make_moves = 0;
    auto accepted_runner =
        Runner{search::SimulatedAnnealing{
            search::SimulatedAnnealingParameters{
                .max_evaluations = 2,
                .initial_temperature = 1.0,
                .cooling_factor = 0.9,
            },
            AlwaysAccept{}}}
        | tsp_manager_recipe
        | (neighborhood<CountingTspNeighborhoodExplorer>(
               std::ref(accepted_make_moves))
           | delta<
                 tsp::TourLengthComponent,
                 tsp::TwoOptTourLengthDeltaEvaluator>());

    std::mt19937 accept_rng{7U};
    const auto accepted_result =
        accepted_runner.bind(tsp_problem).run(tsp_initial, accept_rng);

    ok &= expect(
        accepted_make_moves == 1,
        "accepted all-delta SA candidate applies make_move exactly once");
    ok &= expect(
        accepted_result.solution.tour != tsp_initial.tour,
        "accepting the sampled 2-opt proposal changes the TSP solution");

    {
        const tsp::SolutionManager manager{tsp_problem};
        const tsp::TourLengthComponent component{tsp_problem};
        ok &= expect(
            approximately_equal(
                manager.aggregate(component.evaluate(accepted_result.solution)),
                accepted_result.cost,
                tsp_tolerance),
            "accepted SA result cost agrees approximately with full TSP evaluation");
    }

    const auto empty_problem = empty_tsp_instance();
    const tsp::Solution empty_initial{
        .tour = {0, 1, 2},
    };
    auto empty_runner =
        Runner{search::SimulatedAnnealing{
            search::SimulatedAnnealingParameters{
                .max_evaluations = 10,
                .initial_temperature = 1.0,
                .cooling_factor = 0.9,
            },
            AlwaysAccept{}}}
        | (solution_manager<tsp::SolutionManager>()
           | component<tsp::TourLengthComponent>())
        | neighborhood<tsp::NeighborhoodExplorer>();

    std::mt19937 empty_rng{17U};
    const auto empty_result =
        empty_runner.bind(empty_problem).run(empty_initial, empty_rng);

    ok &= expect(
        empty_result.evaluations == 1,
        "SA does not spend candidate evaluations on an empty neighborhood");
    ok &= expect(
        empty_result.termination ==
            search::SimulatedAnnealingTermination::empty_neighborhood,
        "SA reports an empty random neighborhood explicitly");

    const auto tsp_metropolis = search::MetropolisAcceptance{
        IdentityEnergy{},
        ApproximateEquivalent{tsp_tolerance},
    };

    CountingEngine metropolis_rng;
    ok &= expect(
        tsp_metropolis.accept(0.9, 1.0, 1.0, metropolis_rng),
        "provisional Metropolis policy accepts a strict energy improvement");
    ok &= expect(
        metropolis_rng.calls == 0,
        "strict Metropolis improvement does not consume random numbers");

    const auto adjacent = std::nextafter(1.0, 2.0);
    ok &= expect(
        tsp_metropolis.accept(adjacent, 1.0, 1.0, metropolis_rng),
        "approximate equivalence treats one-ulp energy drift as neutral");
    ok &= expect(
        metropolis_rng.calls == 0,
        "approximately neutral Metropolis move avoids a stochastic draw");

    (void)tsp_metropolis.accept(2.0, 1.0, 1.0, metropolis_rng);
    ok &= expect(
        metropolis_rng.calls > 0,
        "a genuinely worsening Metropolis move enters the stochastic branch");

    auto tsp_metropolis_runner =
        Runner{search::SimulatedAnnealing{
            search::SimulatedAnnealingParameters{
                .max_evaluations = 12,
                .initial_temperature = 2.0,
                .cooling_factor = 0.95,
            },
            tsp_metropolis}}
        | tsp_manager_recipe
        | (neighborhood<tsp::NeighborhoodExplorer>()
           | delta<
                 tsp::TourLengthComponent,
                 tsp::TwoOptTourLengthDeltaEvaluator>());

    auto tsp_bound = tsp_metropolis_runner.bind(tsp_problem);
    std::mt19937 tsp_rng_a{2026U};
    std::mt19937 tsp_rng_b{2026U};
    const auto tsp_result_a = tsp_bound.run(tsp_initial, tsp_rng_a);
    const auto tsp_result_b = tsp_bound.run(tsp_initial, tsp_rng_b);

    ok &= expect(
        tsp_result_a.solution.tour == tsp_result_b.solution.tour &&
            tsp_result_a.cost == tsp_result_b.cost &&
            tsp_result_a.evaluations == tsp_result_b.evaluations,
        "TSP SA is reproducible for the same explicit RNG state");

    const auto assignment_problem = assignment_instance();
    const assignment::Solution assignment_initial{
        .assignment = {0, 0, 1},
    };
    const AssignmentEnergy assignment_energy{
        static_cast<std::int64_t>(assignment_problem.capacity.size() + 1),
    };

    for (std::int64_t first_overload = 0; first_overload <= 4; ++first_overload)
    {
        for (std::int64_t first_machines = 0; first_machines <= 2; ++first_machines)
        {
            for (std::int64_t second_overload = 0;
                 second_overload <= 4;
                 ++second_overload)
            {
                for (std::int64_t second_machines = 0;
                     second_machines <= 2;
                     ++second_machines)
                {
                    const assignment::Cost first{
                        first_overload,
                        first_machines,
                    };
                    const assignment::Cost second{
                        second_overload,
                        second_machines,
                    };

                    ok &= expect(
                        (first < second) ==
                            (assignment_energy(first) < assignment_energy(second)),
                        "Assignment scalar energy preserves hierarchical cost order");
                }
            }
        }
    }

    const auto assignment_metropolis =
        search::MetropolisAcceptance{assignment_energy};
    auto assignment_runner =
        Runner{search::SimulatedAnnealing{
            search::SimulatedAnnealingParameters{
                .max_evaluations = 12,
                .initial_temperature = 4.0,
                .cooling_factor = 0.9,
            },
            assignment_metropolis}}
        | (solution_manager<assignment::SolutionManager>()
           | component<assignment::CapacityCostComponent>())
        | (neighborhood<assignment::NeighborhoodExplorer>()
           | delta<
                 assignment::CapacityCostComponent,
                 assignment::ReassignCapacityDeltaEvaluator>());

    auto assignment_bound = assignment_runner.bind(assignment_problem);
    std::mt19937 assignment_rng_a{2026U};
    std::mt19937 assignment_rng_b{2026U};
    const auto assignment_result_a =
        assignment_bound.run(assignment_initial, assignment_rng_a);
    const auto assignment_result_b =
        assignment_bound.run(assignment_initial, assignment_rng_b);

    ok &= expect(
        assignment_result_a.solution.assignment ==
                assignment_result_b.solution.assignment &&
            assignment_result_a.cost == assignment_result_b.cost &&
            assignment_result_a.evaluations == assignment_result_b.evaluations,
        "Assignment SA is reproducible for the same explicit RNG state");

    {
        const assignment::SolutionManager manager{assignment_problem};
        const assignment::CapacityCostComponent component{assignment_problem};
        ok &= expect(
            manager.is_valid(assignment_result_a.solution),
            "Assignment SA returns a valid solution");
        ok &= expect(
            manager.aggregate(component.evaluate(assignment_result_a.solution)) ==
                assignment_result_a.cost,
            "Assignment SA result cost agrees with full evaluation");
    }

    return ok ? 0 : 1;
}
