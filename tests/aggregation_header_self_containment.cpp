#include <easylocal/aggregation.hpp>

#include <concepts>

int main()
{
    constexpr auto cost = easylocal::aggregation::hierarchical{}(1, 2);
    static_assert(std::three_way_comparable<decltype(cost)>);
    static_assert(cost.hard() == 1);
    static_assert(cost.soft() == 2);
    return 0;
}
