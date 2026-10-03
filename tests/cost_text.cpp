// Costs written as text: the generic syntax of cost::from_text, a problem's own
// read_cost, and a target read through a Session.
#include "../examples/tutorial/tsp.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/app/run_parameters.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost/hierarchical.hpp>
#include <easylocal/cost/lexicographic.hpp>
#include <easylocal/cost/text.hpp>
#include <easylocal/runners/first_improvement.hpp>

#include <array>
#include <cassert>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{

namespace el = easylocal;

template<class Cost>
[[nodiscard]] std::string error_of(const std::string_view text)
{
    try
    {
        static_cast<void>(el::cost::from_text<Cost>(text));
    }
    catch (const std::invalid_argument& error)
    {
        return error.what();
    }
    return {};
}

void numbers_and_brackets_are_read()
{
    assert(el::cost::from_text<int>(" 12 ") == 12);
    assert(el::cost::from_text<double>("2.5") == 2.5);
    assert(error_of<int>("1.5") == "expected an integer, found '1.5'");
    assert(error_of<double>("") == "expected a number, found ''");

    using hard_soft = el::cost::hierarchical<int, double>;
    assert(el::cost::from_text<hard_soft>("[0, 120.5]") == hard_soft(0, 120.5));
    assert(
        error_of<hard_soft>("[1, 2, 3]")
        == "a hierarchical cost [hard, soft] expects 2 values, found 3");
    assert(error_of<hard_soft>("7").starts_with("expected [...]"));

    // Nested as the types are.
    using nested = el::cost::lexicographic<int, el::cost::hierarchical<int, int>>;
    assert(
        el::cost::from_text<nested>("[1, [2, 3]]")
        == nested(1, el::cost::hierarchical<int, int>(2, 3)));

    // Written back in the same form.
    assert(el::cost::to_text(2.5) == "2.5");
    assert(el::cost::to_text(0.1) == "0.1");
    assert(el::cost::to_text(hard_soft(0, 120.5)) == "[0, 120.5]");
    assert(
        el::cost::to_text(nested(1, el::cost::hierarchical<int, int>(2, 3)))
        == "[1, [2, 3]]");
}

} // namespace

namespace problem
{

// A problem that writes its costs in its own way: in hundredths.
struct Instance
{
};

[[nodiscard]] double read_cost(const Instance&, const std::string_view text)
{
    return el::cost::from_text<double>(text) / 100.0;
}

} // namespace problem

namespace
{

void a_problem_reads_its_own_costs()
{
    using el::read_cost;
    assert(read_cost<double>(problem::Instance{}, "250") == 2.5);
    assert(read_cost<double>(tutorial::Tsp{}, "250") == 250.0);
}

void a_target_from_the_configuration_stops_a_run()
{
    auto application = el::app("tsp")
        | (el::solution_manager<tutorial::TourManager>()
            | el::component<tutorial::TourLength>())
        | (el::neighborhood<tutorial::TwoOptExplorer>()
            | el::delta<tutorial::TourLength, tutorial::TwoOptLengthDelta>())
        | el::runner<el::runners::FirstImprovement>("fi");

    el::RunParameters run;
    el::config::parameter_set configuration;
    configuration.add("run", run);
    const std::array overrides{el::config::text_override{"run.target", "29"}};
    assert(configuration.apply(overrides));

    // From the initial tour, 29, First Improvement reaches 26; a target of 29
    // is reached at once, and the run keeps the tour.
    el::Session session{application, tutorial::five_cities(), 1};
    session.use_initial_solution();
    assert(session.evaluate() == 29.0);
    assert(session.run("fi", el::stop_at(session.read_cost(run.target))));
    assert(session.evaluate() == 29.0);
}

} // namespace

int main()
{
    numbers_and_brackets_are_read();
    a_problem_reads_its_own_costs();
    a_target_from_the_configuration_stops_a_run();
    return 0;
}
