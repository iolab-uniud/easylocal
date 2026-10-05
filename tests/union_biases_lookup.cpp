// `neighborhood_union(...) | random_biases(...)` is found for explorers that
// derive from no EasyLocal base, in a namespace that does not use easylocal.
#include <easylocal/helpers/neighborhood_union.hpp>
#include <easylocal/helpers/recipes.hpp>

#include <array>

namespace plain
{

struct First
{
};

struct Second
{
};

} // namespace plain

int main()
{
    const auto spec =
        easylocal::neighborhood_union(
            easylocal::neighborhood<plain::First>(),
            easylocal::neighborhood<plain::Second>())
        | easylocal::random_biases(2.0, 1.0);
    return spec.parameters().random_biases == std::array{2.0, 1.0} ? 0 : 1;
}
