#include <easylocal/search/first_improvement.hpp>

#include <concepts>

int main()
{
    using algorithm_type = easylocal::search::FirstImprovement;
    static_assert(std::constructible_from<
        algorithm_type,
        easylocal::search::FirstImprovementParameters>);
    return 0;
}
