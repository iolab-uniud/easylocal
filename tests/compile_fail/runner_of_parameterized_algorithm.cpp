// A Runner holds the parameters of a parameterized algorithm, not the
// algorithm: Runner{algorithm} is rejected with the factory to use.
#include "int_cost_fixture.hpp"

#include <easylocal/runners/first_improvement.hpp>

int main()
{
    auto runner = easylocal::Runner{easylocal::runners::FirstImprovement{
        easylocal::runners::FirstImprovementParameters{}}};
    static_cast<void>(runner);
}
