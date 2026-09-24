#include <easylocal/search/best_improvement.hpp>

#include <concepts>

int main()
{
    using algorithm_type = easylocal::search::BestImprovement;
    static_assert(std::constructible_from<
        algorithm_type,
        easylocal::search::BestImprovementParameters>);
    return 0;
}
