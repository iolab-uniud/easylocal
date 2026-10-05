// The target of the run options must convert to the runner's cost.
#include "int_cost_fixture.hpp"

#include <string>

int main()
{
    using namespace int_cost_fixture;
    const Instance instance;
    auto bound = runner<1>().bind(instance);
    static_cast<void>(bound.run(Solution<1>{}, easylocal::stop_at(std::string{"low"})));
}
