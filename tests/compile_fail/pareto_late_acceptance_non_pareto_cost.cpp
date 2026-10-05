#include "structured_cost_fixture.hpp"

#include <easylocal/runners/pareto_late_acceptance_hill_climbing.hpp>

int main()
{
    structured_cost_fixture::run_on_structured_cost<
        easylocal::runners::ParetoLateAcceptanceHillClimbing>();
}
