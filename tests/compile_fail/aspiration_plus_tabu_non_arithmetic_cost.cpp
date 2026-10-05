#include "structured_cost_fixture.hpp"

#include <easylocal/runners/tabu_search.hpp>

int main()
{
    structured_cost_fixture::run_on_structured_cost<
        easylocal::runners::AspirationPlusTabuSearch<>>();
}
