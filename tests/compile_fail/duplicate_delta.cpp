#include "service_composition_fixture.hpp"

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::delta;
    using easylocal::neighborhood;

    [[maybe_unused]] const auto recipe =
        neighborhood<Neighborhood>()
        | delta<ComponentA, DeltaA>()
        | delta<ComponentA, AnotherDeltaA>();
}
