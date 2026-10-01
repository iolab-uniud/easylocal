// Tools run every registered runner with an RNG they own: stochastic runners
// receive it, deterministic ones ignore it, and a seed reproduces the run.
#include "../examples/tutorial/tsp.hpp"

#include <easylocal/easylocal.hpp>

#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <random>

namespace
{

using namespace tutorial;
namespace el = easylocal;
namespace runners = easylocal::runners;
using Annealing = runners::SimulatedAnnealing<runners::temperature::Classic>;

[[nodiscard]] auto make_application()
{
    return el::app("tsp")
        | (el::solution_manager<TourManager>() | el::component<TourLength>())
        | (el::neighborhood<TwoOptExplorer>() | el::delta<TourLength, TwoOptLengthDelta>())
        | el::runner<runners::FirstImprovement>("fi")
        | el::runner<Annealing>("sa", {.samples_per_temperature = 20})
        | el::runner<RandomDescent>("descent", {.max_evaluations = 50});
}

[[nodiscard]] auto run_in_tester(const std::uint64_t seed, const char* runner) -> Tour
{
    el::Tester tester{make_application(), seed};
    tester.set_input(five_cities());
    tester.use_random_solution(tester.rng());
    if (!tester.run_runner(runner))
    {
        std::abort();
    }
    return tester.solution();
}

void simulated_annealing_is_registrable_with_its_policy_parameters()
{
    static_assert(std::same_as<
                  Annealing::parameters_type,
                  runners::temperature::ClassicParameters>);
    const auto application = make_application();
    assert(application.runner_config<Annealing>().samples_per_temperature == 20);
    assert(application.runner_config<Annealing>().validate());
}

void a_seed_reproduces_stochastic_runs()
{
    for (const auto* runner : {"sa", "descent", "fi"})
    {
        assert(run_in_tester(7, runner).order == run_in_tester(7, runner).order);
    }
}

void deterministic_runners_ignore_the_rng()
{
    const auto application = make_application();
    const Tour initial{{0, 1, 2, 3, 4}};
    std::mt19937_64 rng{1};
    const auto before = rng;
    const auto with_rng = application.run_at_with_rng<0>(five_cities(), initial, rng);
    const auto plain = application.run_at<0>(five_cities(), initial);
    assert(with_rng.cost == plain.cost);
    assert(rng == before);
}

void stochastic_runners_consume_the_rng()
{
    const auto application = make_application();
    const Tour initial{{0, 1, 2, 3, 4}};
    std::mt19937_64 rng{1};
    const auto before = rng;
    (void)application.run_at_with_rng<1>(five_cities(), initial, rng);
    assert(rng != before);
}

} // namespace

int main()
{
    simulated_annealing_is_registrable_with_its_policy_parameters();
    a_seed_reproduces_stochastic_runs();
    deterministic_runners_ignore_the_rng();
    stochastic_runners_consume_the_rng();
    return 0;
}
