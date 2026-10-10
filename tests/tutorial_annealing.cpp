#include "../examples/tutorial/reheat_on_rejection.hpp"
#include "../examples/tutorial/tsp.hpp"
#include "support/expect.hpp"

#include <easylocal/easylocal.hpp>

#include <array>
#include <cmath>
#include <random>
#include <stdexcept>

namespace
{

// Reject every proposal to exercise reheating and the run budget deterministically.
struct RejectAll
{
    template<class RNG>
    bool accept(double, double, double, RNG&) const
    {
        return false;
    }
};

// Accepted moves break the rejection streak; exhausted reheats allow cooling to end.
bool check_rejection_trigger()
{
    tutorial::ReheatOnRejection policy{{
        .descent =
            {.initial_temperature = 8.0,
                .final_temperature = 0.125,
                .cooling_rate = 0.5,
                .samples_per_temperature = 1},
        .rejections_before_reheat = 2,
        .allowed_reheats = 1,
    }};
    bool ok = true;
    policy.on_iteration(false);
    policy.on_iteration(true);
    policy.on_iteration(false);
    ok &= expect(
        policy.temperature() == 1.0 && policy.reheats() == 0,
        "an acceptance breaks the rejection streak");
    policy.on_iteration(false);
    ok &= expect(
        policy.temperature() == 8.0 && policy.reheats() == 1,
        "two consecutive rejections restore the initial temperature");
    for (int proposal = 0; proposal < 6; ++proposal)
        policy.on_iteration(false);
    ok &= expect(
        policy.finished() && policy.reheats() == 1,
        "the reheat limit leaves a finite final cooling schedule");
    policy.reset();
    ok &= expect(
        !policy.finished() && policy.temperature() == 8.0 && policy.reheats() == 0,
        "reset starts a fresh schedule and reheat allowance");
    policy.on_iteration(false);
    ok &= expect(policy.reheats() == 0, "reset also clears the rejection streak");
    return ok;
}

// Zero reheats matches Classic; calibration is retained across reheats and resets.
bool check_cooling_and_calibration()
{
    tutorial::ReheatOnRejectionParameters parameters{
        .descent =
            {.initial_temperature = 8.0,
                .final_temperature = 0.125,
                .cooling_rate = 0.5,
                .samples_per_temperature = 1,
                .calibration_samples = 2,
                .initial_acceptance = 0.5},
        .rejections_before_reheat = 1,
        .allowed_reheats = 0,
    };
    tutorial::ReheatOnRejection plain{parameters};
    easylocal::runners::temperature::Classic classic{parameters.descent};
    bool ok = true;
    for (int proposal = 0; proposal < 6; ++proposal)
    {
        plain.on_iteration(false);
        classic.on_iteration(false);
        ok &= expect(
            plain.temperature() == classic.temperature()
                && plain.finished() == classic.finished(),
            "zero reheats preserves the Classic schedule");
    }
    parameters.allowed_reheats = 1;
    tutorial::ReheatOnRejection calibrated{parameters};
    const std::array deltas{2.0, 4.0};
    calibrated.calibrate(deltas);
    const auto expected = -3.0 / std::log(0.5);
    ok &= expect(
        calibrated.calibration_samples() == 2
            && std::abs(calibrated.temperature() - expected) < 1e-12,
        "calibration delegates to Classic");
    calibrated.on_iteration(false);
    ok &= expect(
        calibrated.reheats() == 1
            && std::abs(calibrated.temperature() - expected) < 1e-12,
        "reheating uses the calibrated initial temperature");
    calibrated.reset();
    ok &= expect(
        std::abs(calibrated.temperature() - expected) < 1e-12,
        "reset retains calibration");
    parameters.rejections_before_reheat = 0;
    try
    {
        const tutorial::ReheatOnRejection invalid{parameters};
        ok &= expect(false, "a zero rejection threshold is rejected");
    }
    catch (const std::invalid_argument&)
    {
    }
    return ok;
}

// The customized algorithm retains the framework's budget and empty-neighborhood
// handling.
bool check_runner_integration()
{
    namespace el = easylocal;
    using namespace tutorial;
    using Algorithm = el::runners::SimulatedAnnealing<ReheatOnRejection, RejectAll>;
    auto runner =
        el::make_runner<Algorithm>(
            {
                .temperature = {.rejections_before_reheat = 1, .allowed_reheats = 100},
                .max_evaluations = 4,
            })
            .with_solution_manager(
                el::solution_manager<TourManager>().with_cost(
                    el::component<TourLength>()))
            .with_neighborhood(
                el::neighborhood<TwoOptExplorer>()
                    .with_delta<TourLength, TwoOptLengthDelta>());
    const auto tsp = five_cities();
    auto search = runner.bind(tsp);
    std::mt19937_64 rng{42};
    const auto initial = search.initial_solution();
    const auto result = search.run(initial, rng);
    bool ok = expect(
        result.evaluations == 4 && result.iterations == 3
            && result.termination == el::termination_reason::evaluation_budget_exhausted,
        "reheats do not reset the run's evaluation budget");
    ok &= expect(
        result.solution == initial && result.cost == 29.0,
        "rejected moves leave the best solution unchanged");
    const Tsp small{.distance = {{0, 1, 2}, {1, 0, 3}, {2, 3, 0}}};
    auto empty_search = runner.bind(small);
    const auto empty = empty_search.run(empty_search.initial_solution(), rng);
    ok &= expect(
        empty.evaluations == 1 && empty.iterations == 0
            && empty.termination == el::termination_reason::local_optimum,
        "an empty neighborhood ends without attempting reheats");
    return ok;
}

} // namespace

int main()
{
    bool ok = check_rejection_trigger();
    ok &= check_cooling_and_calibration();
    ok &= check_runner_integration();
    return ok ? 0 : 1;
}
