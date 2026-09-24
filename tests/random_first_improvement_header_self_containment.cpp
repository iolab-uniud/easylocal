#include <easylocal/search/random_first_improvement.hpp>

#include <concepts>

int main()
{
    using algorithm_type = easylocal::search::RandomFirstImprovement;
    static_assert(std::constructible_from<
        algorithm_type,
        easylocal::search::RandomFirstImprovementParameters>);
    return 0;
}
