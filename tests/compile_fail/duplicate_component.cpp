#include "service_composition_fixture.hpp"

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::component;
    using easylocal::solution_manager;

    [[maybe_unused]] const auto recipe =
        solution_manager<BaseSolutionManager>()
        | component<ComponentA>()
        | component<ComponentA>();
}
