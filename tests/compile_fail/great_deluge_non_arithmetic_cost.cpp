#include "structured_cost_fixture.hpp"

#include <easylocal/runners/great_deluge.hpp>

int main()
{
    structured_cost_fixture::run_on_structured_cost<easylocal::runners::GreatDeluge>();
}
