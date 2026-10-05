#include "service_composition_fixture.hpp"

// The function of a cost::apply defines the cost semantics with one compare:
// a better() alone would mix with the default <= and == silently.
namespace
{

struct Maximize
{
    [[nodiscard]]
    int operator()(const int value) const noexcept
    {
        return value;
    }

    [[nodiscard]]
    static bool better(const int candidate, const int reference) noexcept
    {
        return candidate > reference;
    }
};

} // namespace

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::component;
    using easylocal::solution_manager;

    [[maybe_unused]] const auto recipe = solution_manager<BaseSolutionManager>()
        | easylocal::cost::apply(Maximize{}, component<ComponentA>());
}
