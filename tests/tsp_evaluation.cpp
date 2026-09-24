#include "move.hpp"
#include "neighborhood_explorer.hpp"
#include "solution.hpp"
#include "solution_manager.hpp"
#include "tour_length_component.hpp"
#include "tour_length_delta.hpp"

#include <easylocal/runner.hpp>

#include <functional>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using namespace easylocal::mwe::tsp;

struct ProbeResult
{
    Solution solution;
    distance_type initial_cost;
    distance_type candidate_cost;
    distance_type current_cost;
};

class ProbeOneMove
{
public:
    ProbeOneMove(const TwoOptMove move, const bool accept) noexcept
        : move_{move},
          accept_{accept}
    {
    }

    template<class Context>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const -> ProbeResult
    {
        const auto evaluation = context.evaluation();
        auto current = evaluation.evaluate(solution);
        const auto initial_cost = current.cost();

        auto candidate =
            evaluation.after_move(solution, current, move_);
        const auto candidate_cost = candidate.cost();

        if (accept_)
        {
            evaluation.accept(solution, current, std::move(candidate));
        }

        return ProbeResult{
            .solution = std::move(solution),
            .initial_cost = initial_cost,
            .candidate_cost = candidate_cost,
            .current_cost = current.cost(),
        };
    }

private:
    TwoOptMove move_;
    bool accept_;
};

class CountingNeighborhoodExplorer : public NeighborhoodExplorer
{
public:
    CountingNeighborhoodExplorer(
        const SolutionManager& solution_manager,
        int& make_move_count) noexcept
        : NeighborhoodExplorer{solution_manager},
          make_move_count_{make_move_count}
    {
    }

    void make_move(
        Solution& solution,
        const TwoOptMove& move) const noexcept
    {
        ++make_move_count_;
        NeighborhoodExplorer::make_move(solution, move);
    }

private:
    int& make_move_count_;
};

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
    using namespace easylocal::mwe::tsp;
    using easylocal::Runner;
    using easylocal::component;
    using easylocal::delta;
    using easylocal::neighborhood;
    using easylocal::solution_manager;

    bool ok = true;

    const Instance instance{
        .city_count = 5,
        .distances = {
            0.0, 1.0, 2.0, 2.5, 1.5,
            1.0, 0.0, 1.25, 2.0, 2.5,
            2.0, 1.25, 0.0, 1.0, 2.0,
            2.5, 2.0, 1.0, 0.0, 1.25,
            1.5, 2.5, 2.0, 1.25, 0.0,
        },
    };

    const Solution initial{
        .tour = {0, 2, 1, 3, 4},
    };
    const TwoOptMove improving_move{
        .first_edge = 0,
        .second_edge = 2,
    };

    const auto manager_recipe =
        solution_manager<SolutionManager>()
        | component<TourLengthComponent>();

    auto fallback_runner =
        Runner{ProbeOneMove{improving_move, true}}
        | manager_recipe
        | neighborhood<NeighborhoodExplorer>();

    const auto fallback_result = fallback_runner.bind(instance).run(initial);

    ok &= expect(
        fallback_result.initial_cost == 8.0,
        "runner evaluates the initial structured component to scalar double cost");
    ok &= expect(
        fallback_result.candidate_cost == 6.0,
        "after_move uses full component fallback for the 2-opt candidate");
    ok &= expect(
        fallback_result.current_cost == 6.0,
        "accept promotes the fallback candidate evaluation");
    ok &= expect(
        fallback_result.solution.tour == std::vector<city_id>{0, 1, 2, 3, 4},
        "accept promotes the materialized 2-opt candidate solution");

    int rejected_make_moves = 0;
    auto rejected_delta_runner =
        Runner{ProbeOneMove{improving_move, false}}
        | manager_recipe
        | (neighborhood<CountingNeighborhoodExplorer>(
               std::ref(rejected_make_moves))
           | delta<
                 TourLengthComponent,
                 TwoOptTourLengthDeltaEvaluator>());

    const auto rejected_delta_result =
        rejected_delta_runner.bind(instance).run(initial);

    ok &= expect(
        rejected_delta_result.candidate_cost == 6.0,
        "recipe-local 2-opt delta produces the same candidate cost");
    ok &= expect(
        rejected_delta_result.current_cost == 8.0,
        "rejecting an all-delta candidate keeps the current evaluation");
    ok &= expect(
        rejected_delta_result.solution.tour == initial.tour,
        "rejecting an all-delta candidate keeps the solution unchanged");
    ok &= expect(
        rejected_make_moves == 0,
        "rejected all-delta candidate does not materialize the moved solution");

    int accepted_make_moves = 0;
    auto accepted_delta_runner =
        Runner{ProbeOneMove{improving_move, true}}
        | manager_recipe
        | (neighborhood<CountingNeighborhoodExplorer>(
               std::ref(accepted_make_moves))
           | delta<
                 TourLengthComponent,
                 TwoOptTourLengthDeltaEvaluator>());

    const auto accepted_delta_result =
        accepted_delta_runner.bind(instance).run(initial);

    ok &= expect(
        accepted_delta_result.candidate_cost == 6.0,
        "accepted all-delta candidate uses incremental tour-length evaluation");
    ok &= expect(
        accepted_delta_result.current_cost == 6.0,
        "accepted all-delta candidate promotes its incremental evaluation");
    ok &= expect(
        accepted_delta_result.solution.tour ==
            std::vector<city_id>{0, 1, 2, 3, 4},
        "accepted all-delta candidate applies the 2-opt move");
    ok &= expect(
        accepted_make_moves == 1,
        "accepted all-delta candidate applies make_move exactly once");

    return ok ? 0 : 1;
}
