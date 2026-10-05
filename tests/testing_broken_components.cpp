// The contract checks on components with the mistakes they exist to catch
// (tests/support/broken_components.hpp): each check fails, with a message
// that names the move, and the correct components pass.
#include "support/broken_components.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/app/check.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/testing.hpp>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

namespace el = easylocal;
namespace elt = easylocal::testing;

bool expect(bool condition, std::string_view message)
{
    if (!condition)
        std::cerr << "FAILED: " << message << '\n';
    return condition;
}

// Symmetric integer distances that differ from city to city.
broken::Tsp cities(const std::size_t count)
{
    broken::Tsp tsp;
    tsp.distance.assign(count, std::vector<double>(count));
    for (std::size_t a = 0; a < count; ++a)
        for (std::size_t b = 0; b < count; ++b)
            if (a != b)
                tsp.distance[a][b] = static_cast<double>(1 + (a * b + a + b) % 97);
    return tsp;
}

broken::Tour identity(const std::size_t count)
{
    broken::Tour tour{std::vector<std::size_t>(count)};
    for (std::size_t city = 0; city < count; ++city)
        tour.order[city] = city;
    return tour;
}

// Whether a check of that name failed, with a message containing text.
bool failed(
    const elt::check_report& report,
    std::string_view check,
    std::string_view text)
{
    return std::ranges::any_of(report.failures(), [&](const elt::check_failure& failure) {
        return failure.check == check && failure.message.find(text) != std::string::npos;
    });
}

} // namespace

int main()
{
    using tutorial::TourLength;
    using tutorial::TwoOptExplorer;
    using tutorial::TwoOptLengthDelta;
    bool ok = true;

    // A delta that confuses positions and cities is right on the identity
    // tour: the random solutions of the check catch it, and the failure names
    // the move, the solution and the two values.
    const elt::fixture<broken::TourManager> from_identity{cities(8), identity(8)};
    const auto positions = elt::check_delta_evaluator<
        TwoOptExplorer,
        TourLength,
        broken::PositionsAsCitiesDelta>(from_identity);
    ok &= expect(
        !positions.passed() && failed(positions, "delta law", "2-opt(")
            && failed(positions, "delta law", "random solution")
            && failed(positions, "delta law", "value + delta is"),
        "the delta law catches a delta right on the identity tour only");
    const auto correct =
        elt::check_delta_evaluator<TwoOptExplorer, TourLength, TwoOptLengthDelta>(
            from_identity);
    ok &= expect(correct.passed(), "the tutorial's 2-opt delta passes");
    const elt::fixture<broken::TourManager>
        identity_only{cities(8), identity(8), {.random_solutions = 0}};
    ok &= expect(
        elt::check_delta_evaluator<
            TwoOptExplorer,
            TourLength,
            broken::PositionsAsCitiesDelta>(identity_only)
            .passed(),
        "from the identity tour alone, the faulty delta passes");

    // The failures of one check are printed together, three of them.
    std::ostringstream printed;
    elt::print_report(printed, positions);
    const auto text = printed.str();
    ok &= expect(
        text.find("[fail] delta law (") != std::string::npos
            && text.find("more\n") != std::string::npos
            && text.find("[fail] delta law", text.find("[fail] delta law") + 1)
                == std::string::npos,
        "print_report groups the failures of a check");

    // A make_move that takes the tour by value leaves it unchanged.
    const elt::fixture<broken::TourManager> tsp{cities(8), identity(8)};
    const auto by_value = elt::check_neighborhood<broken::ByValueTwoOpt>(tsp);
    ok &= expect(
        failed(by_value, "null moves", "by value"),
        "check_neighborhood reports a make_move that changes nothing");
    ok &= expect(
        elt::check_neighborhood<TwoOptExplorer>(tsp).passed(),
        "the tutorial's 2-opt explorer passes check_neighborhood");

    // Random moves outside the enumeration, none at all, or from another
    // generator than the one given.
    const auto outside = elt::check_neighborhood<broken::OutsideTwoOpt>(tsp);
    ok &= expect(
        failed(outside, "random move in the neighborhood", "does not enumerate")
            || failed(outside, "random proposal", "is_valid"),
        "check_neighborhood reports a random move outside the neighborhood");
    const auto empty = elt::check_neighborhood<broken::EmptyRandomTwoOpt>(tsp);
    ok &= expect(
        failed(empty, "random move availability", "found no move"),
        "check_neighborhood reports a random_move that finds no move");
    const auto own = elt::check_neighborhood<broken::OwnGeneratorTwoOpt>(tsp);
    ok &= expect(
        failed(own, "random move reproducibility", "same seed"),
        "check_neighborhood reports a random_move that ignores its generator");

    // An exception of a hook is a failed check, not an escaped exception.
    const auto throwing = elt::check_neighborhood<broken::ThrowingTwoOpt>(tsp);
    ok &= expect(
        failed(throwing, "native moves traversal", "threw: cannot apply this move")
            || failed(throwing, "cursor traversal", "threw: cannot apply this move"),
        "check_neighborhood reports a hook that throws, with the move");

    // check(app) catches the faulty delta too, and run_checks takes its
    // report with the others.
    const auto application = el::app("broken-tsp")
        | (el::solution_manager<broken::TourManager>() | el::component<TourLength>())
        | (el::neighborhood<TwoOptExplorer>()
            | el::delta<TourLength, broken::PositionsAsCitiesDelta>())
        | el::runner<el::runners::FirstImprovement>("fi");
    const auto instance = cities(8);
    const auto app_report = el::check(application, instance, identity(8));
    ok &= expect(
        !app_report.passed()
            && std::ranges::any_of(
                app_report.failures(),
                [](const auto& failure) {
                    return failure.check == "incremental evaluation"
                        && failure.message.find("2-opt(") != std::string::npos
                        && failure.message.find("random solution") != std::string::npos;
                }),
        "check(app) catches a delta right on the identity tour only");
    std::ostringstream run_output;
    ok &= expect(
        elt::run_checks(run_output, app_report, correct) != 0
            && run_output.str().find("composition: ") != std::string::npos,
        "run_checks takes the report of check(app)");

    return ok ? 0 : 1;
}
