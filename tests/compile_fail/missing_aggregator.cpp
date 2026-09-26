#include "service_composition_fixture.hpp"

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::Runner;
    using easylocal::component;
    using easylocal::solution_manager;

    [[maybe_unused]] auto runner =
        Runner{Algorithm{}}
        | (solution_manager<BaseSolutionManager>() | component<ComponentA>());
}
