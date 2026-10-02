#include "service_composition_fixture.hpp"

// The cost always comes from cost components: a SolutionManager recipe
// without a cost expression is rejected.
int main()
{
    using namespace compile_fail_fixture;
    using easylocal::Runner;
    using easylocal::solution_manager;

    [[maybe_unused]] auto runner =
        Runner{Algorithm{}} | solution_manager<BaseSolutionManager>();
}
