#include <easylocal/cost.hpp>

static_assert(easylocal::delta_cost<int>);
static_assert(easylocal::delta(7, 3) == 4);

int main() {}
