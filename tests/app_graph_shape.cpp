#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/instance.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"
#include "easylocal/app/app.hpp"
#include "easylocal/runners/first_improvement.hpp"

#include <utility>

namespace
{
using namespace assignment;

template<class Builder>
concept can_add_assignment_solution_manager = requires(Builder builder) {
    std::move(builder).with_solution_manager(
        easylocal::solution_manager<AssignmentSolutionManager>()
        | easylocal::component<LoadImbalanceCostComponent>());
};

template<class Builder>
concept can_add_reassign_neighborhood = requires(Builder builder) {
    std::move(builder).with_neighborhood(easylocal::neighborhood<ReassignJobNeighborhoodExplorer>());
};

template<class Builder>
concept can_pipe_reassign_neighborhood = requires(Builder builder) {
    std::move(builder) | easylocal::neighborhood<ReassignJobNeighborhoodExplorer>();
};

template<class Builder>
concept can_add_first_improvement = requires(Builder builder) {
    std::move(builder).template with_runner<easylocal::runners::FirstImprovement>("fi");
};

template<class Builder>
concept can_pipe_first_improvement = requires(Builder builder) {
    std::move(builder) | easylocal::runner<easylocal::runners::FirstImprovement>("fi");
};

using EmptyApp = decltype(easylocal::app("shape"));
using AppWithSolutionManager = decltype(
    easylocal::app("shape")
        .with_solution_manager(
            easylocal::solution_manager<AssignmentSolutionManager>()
            | easylocal::component<LoadImbalanceCostComponent>()));
using AppWithNeighborhood = decltype(easylocal::app("shape")
        .with_solution_manager(
            easylocal::solution_manager<AssignmentSolutionManager>()
            | easylocal::component<LoadImbalanceCostComponent>())
        .with_neighborhood(easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()));
using CompleteGraphApp = decltype(
    easylocal::app("shape")
        .with_solution_manager(
            easylocal::solution_manager<AssignmentSolutionManager>()
            | easylocal::component<LoadImbalanceCostComponent>())
        .with_neighborhood(easylocal::neighborhood<ReassignJobNeighborhoodExplorer>())
        .with_runner<easylocal::runners::FirstImprovement>("fi"));

static_assert(!EmptyApp::has_solution_manager);
static_assert(!EmptyApp::has_neighborhood);
static_assert(EmptyApp::registration_count == 0);
static_assert(can_add_assignment_solution_manager<EmptyApp>);
// The order is SolutionManager, neighborhood, runners: a neighborhood needs
// the SolutionManager, a runner both.
static_assert(!can_add_reassign_neighborhood<EmptyApp>);
static_assert(!can_pipe_reassign_neighborhood<EmptyApp>);
static_assert(!can_add_first_improvement<EmptyApp>);
static_assert(!can_pipe_first_improvement<EmptyApp>);

static_assert(AppWithSolutionManager::has_solution_manager);
static_assert(!AppWithSolutionManager::has_neighborhood);
static_assert(!can_add_assignment_solution_manager<AppWithSolutionManager>);
static_assert(can_add_reassign_neighborhood<AppWithSolutionManager>);
static_assert(can_pipe_reassign_neighborhood<AppWithSolutionManager>);
static_assert(!can_add_first_improvement<AppWithSolutionManager>);
static_assert(!can_pipe_first_improvement<AppWithSolutionManager>);

static_assert(can_add_first_improvement<AppWithNeighborhood>);
static_assert(can_pipe_first_improvement<AppWithNeighborhood>);
static_assert(!can_add_reassign_neighborhood<AppWithNeighborhood>);
static_assert(!can_pipe_reassign_neighborhood<AppWithNeighborhood>);

// A runner is not needed to bind: the app is built up one component at a
// time, and a tool on one without runners has none to run.
template<class Application>
concept can_bind =
    requires(const Application& application, const AssignmentInstance& input) {
        application.bind(input);
    };

static_assert(!can_bind<EmptyApp>);
static_assert(!can_bind<AppWithSolutionManager>);
static_assert(can_bind<AppWithNeighborhood>);
static_assert(can_bind<CompleteGraphApp>);

static_assert(CompleteGraphApp::has_solution_manager);
static_assert(CompleteGraphApp::has_neighborhood);
static_assert(CompleteGraphApp::registration_count == 1);
static_assert(!can_add_assignment_solution_manager<CompleteGraphApp>);
static_assert(!can_add_reassign_neighborhood<CompleteGraphApp>);
}

int main()
{
    return 0;
}
