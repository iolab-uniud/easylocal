// The PFSP example: the makespan by hand on the small instance, the solution
// identity, the two inverse definitions, and tabu search improving a random
// schedule.
#include "instance_io.hpp"
#include "makespan_component.hpp"
#include "solution_manager.hpp"
#include "swap_neighborhood_explorer.hpp"

#include <easylocal/cost.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/tabu_search.hpp>

#include <iostream>
#include <random>
#include <string_view>

#ifndef EASYLOCAL_PFSP_SMALL_INSTANCE
#error "EASYLOCAL_PFSP_SMALL_INSTANCE must name the small instance"
#endif
#ifndef EASYLOCAL_PFSP_MEDIUM_INSTANCE
#error "EASYLOCAL_PFSP_MEDIUM_INSTANCE must name the medium instance"
#endif

namespace
{

using namespace pfsp;

static_assert(easylocal::has_solution_hash<PfspSolutionManager>);
static_assert(
    easylocal::inverse_neighborhood_for<SwapJobsNeighborhoodExplorer, Schedule>);
static_assert(easylocal::has_tabu_attribute_member<SwapJobsNeighborhoodExplorer>);

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }
    return true;
}

template<class Algorithm, class Explorer>
[[nodiscard]]
auto tabu_runner(const typename Algorithm::parameters_type& parameters)
{
    return easylocal::make_runner<Algorithm>(parameters)
        | (easylocal::solution_manager<PfspSolutionManager>()
            | easylocal::component<MakespanComponent>())
        | easylocal::neighborhood<Explorer>();
}

} // namespace

int main()
{
    bool ok = true;

    const auto small = load_instance(EASYLOCAL_PFSP_SMALL_INSTANCE);
    const PfspSolutionManager manager{small};
    const MakespanComponent makespan{small};
    const auto identity = manager.initial_solution();
    ok &= expect(
        manager.is_valid(identity) && makespan.evaluate(identity) == 396,
        "the makespan of the identity schedule matches the hand computation");
    ok &= expect(
        !manager.is_valid(Schedule{.order = {0, 1, 2, 3, 4, 4}}),
        "a schedule repeating a job is not valid");

    auto reversed = identity;
    std::ranges::reverse(reversed.order);
    ok &= expect(
        easylocal::solution_hash(manager, identity)
                == easylocal::solution_hash(manager, manager.initial_solution())
            && easylocal::solution_hash(manager, identity)
                != easylocal::solution_hash(manager, reversed),
        "the solution identity is the order of the jobs");

    // The tabu move swapped jobs 1 and 0; a swap of 0 and 1 again, and one of
    // 0 and 2.
    const SwapJobsNeighborhoodExplorer in1{manager};
    const SwapEitherJobNeighborhoodExplorer in2{manager};
    const SwapJobsMove tabu{0, 2, 1, 0};
    const SwapJobsMove same_jobs{1, 3, 0, 1};
    const SwapJobsMove one_job{0, 3, 0, 2};
    ok &= expect(
        in1.inverse(identity, same_jobs, tabu) && !in1.inverse(identity, one_job, tabu)
            && in2.inverse(identity, same_jobs, tabu)
            && in2.inverse(identity, one_job, tabu),
        "IN1 forbids the same pair of jobs, IN2 either job");
    ok &= expect(
        SwapJobsNeighborhoodExplorer::tabu_attribute(same_jobs)
                == SwapJobsNeighborhoodExplorer::tabu_attribute(tabu)
            && SwapJobsNeighborhoodExplorer::tabu_attribute(one_job)
                != SwapJobsNeighborhoodExplorer::tabu_attribute(tabu),
        "the tabu attribute is the pair of jobs");

    const auto medium = load_instance(EASYLOCAL_PFSP_MEDIUM_INSTANCE);
    const auto improves = [&]<class Explorer>() {
        auto runner = tabu_runner<easylocal::runners::TabuSearch<>, Explorer>(
            {.max_idle_iterations = 50, .tabu_list = {.tenure = 7}});
        auto search = runner.bind(medium);
        std::mt19937_64 rng{2026U};
        const auto initial = search.random_solution(rng);
        const MakespanComponent medium_makespan{medium};
        const auto result = search.run(initial, rng);
        return PfspSolutionManager{medium}.is_valid(result.solution)
            && result.cost == medium_makespan.evaluate(result.solution)
            && result.cost < medium_makespan.evaluate(initial)
            && result.termination == easylocal::termination_reason::idle_limit_reached;
    };
    ok &= expect(
        improves.template operator()<SwapJobsNeighborhoodExplorer>()
            && improves.template operator()<SwapEitherJobNeighborhoodExplorer>(),
        "tabu search improves a random schedule with either inverse");

    return ok ? 0 : 1;
}
