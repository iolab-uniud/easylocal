// The contracts of a Session that its other tests leave out: configure rolls
// back when rebuilding the services throws, a moved Session keeps its state,
// a null Input is rejected, a run does not start from an invalid solution, and
// RunParameters reads its target.
#include "../examples/tutorial/tsp.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/app/run_parameters.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/config/parameters.hpp>
#include <easylocal/runners/first_improvement.hpp>

#include <array>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{

using namespace tutorial;
namespace el = easylocal;

struct FragileParameters
{
    int mode{0};

    static consteval auto parameter_schema()
    {
        return el::config::fields(
            el::config::field<"mode", &FragileParameters::mode>(
                "0 builds, 1 throws when built",
                el::config::range(0, 1)));
    }

    [[nodiscard]] el::config::validation_result validate() const
    {
        return el::config::check_schema(*this);
    }
};

// A SolutionManager whose construction fails with mode 1, a valid value.
class FragileManager : public TourManager
{
public:
    using parameters_type = FragileParameters;

    FragileManager(const Tsp& input, const FragileParameters& parameters)
        : TourManager{input}
    {
        if (parameters.mode == 1)
            throw std::runtime_error{"cannot build"};
    }
};

[[nodiscard]] auto make_application()
{
    return el::app("fragile-tsp")
        | (el::solution_manager<FragileManager>() | el::component<TourLength>())
        | (el::neighborhood<TwoOptExplorer>()
            | el::delta<TourLength, TwoOptLengthDelta>())
        | el::runner<el::runners::FirstImprovement>("fi");
}

[[nodiscard]] std::string value_at(
    const el::config::parameter_set& set,
    const std::string_view path)
{
    for (const auto& parameter : set.parameters())
        if (parameter.path == path)
            return parameter.value;
    return "<absent>";
}

void configure_rolls_back_when_the_services_cannot_be_built()
{
    el::Session session{make_application(), five_cities(), 1};
    session.use_initial_solution();
    const std::array change{el::config::text_override{"solution_manager.mode", "1"}};
    bool threw = false;
    try
    {
        static_cast<void>(session.configure(change));
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    assert(threw);
    assert(value_at(session.configuration(), "solution_manager.mode") == "0");
    assert(session.evaluate() == 29.0);
}

void a_moved_session_keeps_its_state()
{
    el::Session session{make_application(), five_cities(), 1};
    session.use_initial_solution();
    el::Session moved{std::move(session)};
    assert(moved.has_input() && moved.has_solution());
    assert(moved.evaluate() == 29.0);
    assert(moved.run("fi"));
    assert(moved.evaluate() == 26.0);
}

void a_null_input_is_rejected()
{
    el::Session session{make_application(), 1};
    bool threw = false;
    try
    {
        session.set_input(std::shared_ptr<const Tsp>{});
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    assert(threw && !session.has_input());
}

void a_run_does_not_start_from_an_invalid_solution()
{
    el::Session session{make_application(), five_cities(), 1};
    session.set_solution(Tour{{0, 1, 2}}); // three cities of five
    assert(!session.is_valid());
    bool threw = false;
    try
    {
        static_cast<void>(session.run("fi"));
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    assert(threw);
    assert(session.solution().order.size() == 3);
}

void run_parameters_read_their_target()
{
    const auto tsp = five_cities();
    el::RunParameters run;
    assert(!run.target_cost<double>(tsp));
    run.target = " 26 ";
    assert(run.target_cost<double>(tsp) == 26.0);
    run.target = "short";
    bool threw = false;
    try
    {
        static_cast<void>(run.target_cost<double>(tsp));
    }
    catch (const std::invalid_argument& error)
    {
        threw = std::string{error.what()}.starts_with("target: ");
    }
    assert(threw);
    run.target.clear();
    run.timeout = "-1";
    assert(!run.validate());
}

} // namespace

int main()
{
    configure_rolls_back_when_the_services_cannot_be_built();
    a_moved_session_keeps_its_state();
    a_null_input_is_rejected();
    a_run_does_not_start_from_an_invalid_solution();
    run_parameters_read_their_target();
    return 0;
}
