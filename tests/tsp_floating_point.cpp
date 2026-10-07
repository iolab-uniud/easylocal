// A TSP with distances that binary floating point cannot represent: the cost
// updated by deltas drifts from the full evaluation, which the checks of the
// library forgive within their tolerance, and only within it.
#include "apps.hpp"
#include "neighborhood_explorer.hpp"
#include "solution.hpp"
#include "solution_manager.hpp"
#include "support/expect.hpp"
#include "tour_length_component.hpp"
#include "tour_length_delta.hpp"

#include <easylocal/app/check.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/cost/tolerance.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/testing/delta_cost_component.hpp>
#include <easylocal/testing/fixture.hpp>

#include <compare>
#include <cstddef>
#include <functional>

namespace
{

using namespace tsp;
using easylocal::cost::approximately_equal;

constexpr easylocal::cost::tolerance tsp_tolerance{
    .relative = 1.0e-12,
    .absolute = 1.0e-12,
};

[[nodiscard]]
auto definitely_less(
    const double lhs,
    const double rhs,
    const easylocal::cost::tolerance within) -> bool
{
    return easylocal::cost::approximate_compare(lhs, rhs, within) < 0;
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
    const TwoOptTourLengthDelta length_delta{instance};

    const auto current = component.evaluate(solution);

    Tour candidate = solution;
    neighborhood.make_move(candidate, move);

    return EvaluatedMove{
        .current = current,
        .full_candidate = component.evaluate(candidate),
        .incremental_candidate = current + length_delta.delta_evaluate(solution, move),
    };
}

} // namespace

int main()
{
    using namespace tsp;

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
    const TwoOptTourLengthDelta drift_delta{drift_instance};
    const auto drift_current = drift_component.evaluate(solution);

    bool observed_exact_delta_mismatch = false;
    std::size_t checked_moves = 0;

    for (const auto candidate_move : easylocal::moves(drift_neighborhood, solution))
    {
        Tour candidate = solution;
        drift_neighborhood.make_move(candidate, candidate_move);

        const auto full = drift_component.evaluate(candidate);
        const auto incremental =
            drift_current + drift_delta.delta_evaluate(solution, candidate_move);

        ++checked_moves;
        observed_exact_delta_mismatch |= incremental != full;
        ok &= expect(
            approximately_equal(incremental, full, tsp_tolerance),
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

    // The library's checks on the drift instance: within their default
    // tolerance the delta law holds, compared exactly it does not.
    namespace elt = easylocal::testing;
    const auto check_delta = [](const auto& fixture) {
        return elt::check_delta_cost_component<
            TwoOptNeighborhoodExplorer,
            TourLengthComponent,
            TwoOptTourLengthDelta>(fixture)
            .passed();
    };
    const elt::fixture<TspSolutionManager> tolerant{drift_instance, solution};
    const elt::fixture<TspSolutionManager> exact{
        drift_instance,
        solution,
        {.tolerance = {.relative = 0.0, .absolute = 0.0}}};
    const elt::fixture<TspSolutionManager, std::equal_to<>> equal{
        drift_instance,
        solution};
    ok &= expect(check_delta(tolerant), "the delta check forgives the drift by default");
    ok &= expect(
        !check_delta(exact) && !check_delta(equal),
        "the delta check without tolerance reports the drift");

    const auto application = tsp::two_opt_app();
    ok &= expect(
        easylocal::check(application, drift_instance, solution).passed(),
        "check(app) forgives the drift by default");
    ok &= expect(
        !easylocal::check(
            application,
            drift_instance,
            solution,
            {.tolerance = {.relative = 0.0, .absolute = 0.0}})
            .passed(),
        "check(app) without tolerance reports the drift");

    easylocal::Session session{application, drift_instance, 1};
    session.set_solution(solution);
    ok &= expect(
        session.check_neighborhood_costs().mismatches == 0
            && session.check_neighborhood_costs({.relative = 0.0, .absolute = 0.0})
                    .mismatches
                != 0,
        "the Session's cost check forgives the drift within its tolerance only");

    return ok ? 0 : 1;
}
