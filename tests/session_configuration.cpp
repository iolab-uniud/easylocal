// A Session changes the parameters of its app's problem (the cost weights here)
// and rebuilds its services, so the costs it reports follow the new values.
#include "../examples/tutorial/tsp.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost/expression.hpp>
#include <easylocal/helpers/neighborhood_union.hpp>
#include <easylocal/runners/first_improvement.hpp>

#include <array>
#include <cassert>
#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>

namespace
{

using namespace tutorial;
namespace el = easylocal;

[[nodiscard]] auto make_application()
{
    return el::app("weighted-tsp")
        | (el::solution_manager<TourManager>()
            | el::cost::sum(el::component<TourLength>(), el::component<MaxEdge>() * 10.0))
        | (el::neighborhood<TwoOptExplorer>()
            | el::delta<TourLength, TwoOptLengthDelta>())
        | el::runner<el::runners::FirstImprovement>("fi");
}

[[nodiscard]] std::string value_at(
    const el::config::parameter_set& set,
    std::string_view path)
{
    for (const auto& parameter : set.parameters())
        if (parameter.path == path)
            return parameter.value;
    return "<absent>";
}

void a_union_s_biases_are_app_parameters()
{
    auto application = el::app("union-tsp")
        | (el::solution_manager<TourManager>() | el::component<TourLength>())
        | (el::neighborhood_union(
               el::neighborhood<TwoOptExplorer>()
                   | el::delta<TourLength, TwoOptLengthDelta>(),
               el::neighborhood<SwapExplorer>())
            | el::random_biases(3.0, 1.0))
        | el::runner<el::runners::FirstImprovement>("fi");

    el::Session session{application, five_cities(), 1};
    assert(value_at(session.configuration(), "neighborhood.random_biases") == "[3, 1]");
    const std::array biases{
        el::config::text_override{"neighborhood.random_biases", "[1, 0]"}};
    assert(session.configure(biases));
    assert(value_at(session.configuration(), "neighborhood.random_biases") == "[1, 0]");
}

void the_session_lists_the_app_parameters()
{
    const el::Session session{make_application(), five_cities(), 1};
    const auto parameters = session.configuration();
    assert(value_at(parameters, "cost.weights") == "[1, 10]");
    assert(value_at(parameters, "runners.fi.max_evaluations") == "0");
}

void runner_parameters_apply_from_the_next_run()
{
    el::Session session{make_application(), five_cities(), 1};
    session.use_initial_solution();

    // A budget of one evaluation: the run stops at the initial tour.
    const std::array budget{el::config::text_override{"runners.fi.max_evaluations", "1"}};
    assert(session.configure(budget));
    const auto before = session.solution();
    assert(session.run("fi"));
    assert(session.solution() == before);
}

void configuring_the_cost_rebuilds_the_session_services()
{
    el::Session session{make_application(), five_cities(), 1};
    session.use_initial_solution();
    // The tour 0 1 2 3 4: length 29, longest edge 8.
    assert(session.evaluate() == 29.0 + 10.0 * 8.0);

    const std::array overrides{el::config::text_override{"cost.weights", "[1, 0]"}};
    const auto applied = session.configure(overrides);
    assert(applied);
    assert(session.evaluate() == 29.0);
    assert(!session.has_move());
}

void an_invalid_configuration_changes_nothing()
{
    el::Session session{make_application(), five_cities(), 1};
    session.use_initial_solution();

    const std::array malformed{el::config::text_override{"cost.weights", "[1]"}};
    assert(!session.configure(malformed));
    const std::array unknown{el::config::text_override{"cost.wieghts", "[1, 0]"}};
    assert(!session.configure(unknown));
    assert(session.evaluate() == 29.0 + 10.0 * 8.0);
}

} // namespace

int main()
{
    a_union_s_biases_are_app_parameters();
    the_session_lists_the_app_parameters();
    runner_parameters_apply_from_the_next_run();
    configuring_the_cost_rebuilds_the_session_services();
    an_invalid_configuration_changes_nothing();
    return 0;
}
