#include "neighborhood_explorer.hpp"
#include "solution.hpp"
#include "solution_manager.hpp"
#include "tour_length_component.hpp"
#include "tour_length_delta.hpp"

#include "support/approximate.hpp"

#include <cstddef>
#include <iostream>
#include <string_view>

namespace
{

using namespace easylocal::mwe::tsp;
using easylocal::test_support::ApproximateTolerance;
using easylocal::test_support::approximately_equal;
using easylocal::test_support::definitely_less;

constexpr ApproximateTolerance tsp_tolerance{
    .relative = 1.0e-12,
    .absolute = 1.0e-12,
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

[[nodiscard]]
auto make_delta_drift_instance() -> TspInstance
{
    return TspInstance{
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
auto make_neutral_move_instance(const distance_type two_three) -> TspInstance
{
    return TspInstance{
        .city_count = 5,
        .distances = {
            0.0, 0.02, 0.01, 0.07, 0.03,
            0.02, 0.0, 0.01, 0.05, 0.08,
            0.01, 0.01, 0.0, two_three, 0.09,
            0.07, 0.05, two_three, 0.0, 0.01,
            0.03, 0.08, 0.09, 0.01, 0.0,
        },
    };
}

struct EvaluatedMove
{
    distance_type current;
    distance_type full_candidate;
    distance_type incremental_candidate;
};

[[nodiscard]]
auto evaluate_move(
    const TspInstance& instance,
    const Tour& solution,
    const TwoOptMove move) -> EvaluatedMove
{
    const TspSolutionManager solution_manager{instance};
    const TwoOptNeighborhoodExplorer neighborhood{solution_manager};
    const TourLengthComponent component{instance};
    const TwoOptTourLengthDeltaEvaluator delta_evaluator{instance};

    const auto current = component.evaluate(solution);

    Tour candidate = solution;
    neighborhood.make_move(candidate, move);

    return EvaluatedMove{
        .current = current.total,
        .full_candidate = component.evaluate(candidate).total,
        .incremental_candidate =
            (current + delta_evaluator.delta_evaluate(solution, move)).total,
    };
}

} // namespace

int main()
{
    using namespace easylocal::mwe::tsp;

    bool ok = true;

    const Tour solution{
        .tour = {0, 2, 1, 3, 4},
    };
    const TwoOptMove move{
        .first_edge = 0,
        .second_edge = 2,
    };

    const auto drift_instance = make_delta_drift_instance();
    const TspSolutionManager drift_manager{drift_instance};
    const TwoOptNeighborhoodExplorer drift_neighborhood{drift_manager};
    const TourLengthComponent drift_component{drift_instance};
    const TwoOptTourLengthDeltaEvaluator drift_delta{drift_instance};
    const auto drift_current = drift_component.evaluate(solution);

    bool observed_exact_delta_mismatch = false;
    std::size_t checked_moves = 0;

    for (const auto candidate_move : drift_neighborhood.moves(solution))
    {
        Tour candidate = solution;
        drift_neighborhood.make_move(candidate, candidate_move);

        const auto full = drift_component.evaluate(candidate);
        const auto incremental =
            drift_current + drift_delta.delta_evaluate(solution, candidate_move);

        ++checked_moves;
        observed_exact_delta_mismatch |= incremental != full;
        ok &= expect(
            approximately_equal(incremental.total, full.total, tsp_tolerance),
            "approximate delta law holds for every non-binary-exact 2-opt move");
    }

    ok &= expect(
        checked_moves == 5,
        "floating-point delta law covers the complete five-city 2-opt neighborhood");
    ok &= expect(
        observed_exact_delta_mismatch,
        "non-binary-exact distances exercise a real exact-equality mismatch");

    const auto known_drift = evaluate_move(drift_instance, solution, move);
    ok &= expect(
        known_drift.full_candidate == 0.6,
        "known decimal 2-opt candidate has the expected full cost");
    ok &= expect(
        known_drift.incremental_candidate != known_drift.full_candidate,
        "known decimal 2-opt candidate exposes different floating-point evaluation paths");
    ok &= expect(
        approximately_equal(
            known_drift.incremental_candidate,
            known_drift.full_candidate,
            tsp_tolerance),
        "known floating-point evaluation-path difference is numerically negligible");

    const auto neutral_instance = make_neutral_move_instance(0.04);
    const auto neutral = evaluate_move(neutral_instance, solution, move);

    ok &= expect(
        neutral.current == neutral.full_candidate,
        "mathematically neutral 2-opt move is neutral under full evaluation");
    ok &= expect(
        neutral.incremental_candidate < neutral.current,
        "raw incremental double comparison can report a spurious improvement");
    ok &= expect(
        approximately_equal(
            neutral.incremental_candidate,
            neutral.current,
            tsp_tolerance),
        "approximate comparison classifies the spurious delta improvement as equivalent");
    ok &= expect(
        !definitely_less(
            neutral.incremental_candidate,
            neutral.current,
            tsp_tolerance),
        "approximate improvement predicate rejects representation-only improvement");

    const auto improving_instance = make_neutral_move_instance(0.039);
    const auto improving = evaluate_move(improving_instance, solution, move);

    ok &= expect(
        definitely_less(
            improving.full_candidate,
            improving.current,
            tsp_tolerance),
        "full evaluation preserves a real improvement beyond tolerance");
    ok &= expect(
        definitely_less(
            improving.incremental_candidate,
            improving.current,
            tsp_tolerance),
        "incremental evaluation preserves the same real improvement beyond tolerance");
    ok &= expect(
        approximately_equal(
            improving.incremental_candidate,
            improving.full_candidate,
            tsp_tolerance),
        "full and incremental paths agree approximately on a real improvement");

    return ok ? 0 : 1;
}
