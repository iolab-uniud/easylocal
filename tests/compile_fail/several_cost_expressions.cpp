#include "service_composition_fixture.hpp"

// A SolutionManager recipe has one cost expression: several components are
// combined explicitly, never by chaining them.
int main()
{
    using namespace compile_fail_fixture;
    using easylocal::Runner;
    using easylocal::component;
    using easylocal::solution_manager;

    [[maybe_unused]] auto runner =
        Runner{Algorithm{}}
        | (solution_manager<BaseSolutionManager>()
           | component<ComponentA>()
           | component<ComponentB>());
}
