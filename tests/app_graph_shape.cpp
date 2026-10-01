#include "easylocal/app.hpp"
#include "easylocal/search/first_improvement.hpp"
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"

#include <utility>

namespace
{
using namespace easylocal::mwe::assignment;

template<class Builder>
concept can_add_assignment_solution_manager = requires(Builder builder) {
    std::move(builder).template solution_manager<AssignmentSolutionManager>();
};

template<class Builder>
concept can_add_reassign_neighborhood = requires(Builder builder) {
    std::move(builder).template neighborhood<ReassignJobNeighborhoodExplorer>();
};

using EmptyApp = decltype(easylocal::app("shape"));
using AppWithSolutionManager = decltype(
    easylocal::app("shape")
        .solution_manager<AssignmentSolutionManager>());
using CompleteGraphApp = decltype(
    easylocal::app("shape")
        .solution_manager<AssignmentSolutionManager>()
        .neighborhood<ReassignJobNeighborhoodExplorer>()
        .runner<easylocal::search::FirstImprovement>("fi"));

static_assert(!EmptyApp::has_solution_manager);
static_assert(!EmptyApp::has_neighborhood);
static_assert(EmptyApp::runner_count == 0);
static_assert(can_add_assignment_solution_manager<EmptyApp>);

static_assert(AppWithSolutionManager::has_solution_manager);
static_assert(!AppWithSolutionManager::has_neighborhood);
static_assert(!can_add_assignment_solution_manager<AppWithSolutionManager>);
static_assert(can_add_reassign_neighborhood<AppWithSolutionManager>);

static_assert(CompleteGraphApp::has_solution_manager);
static_assert(CompleteGraphApp::has_neighborhood);
static_assert(CompleteGraphApp::runner_count == 1);
static_assert(!can_add_assignment_solution_manager<CompleteGraphApp>);
static_assert(!can_add_reassign_neighborhood<CompleteGraphApp>);
}

int main()
{
    return 0;
}
