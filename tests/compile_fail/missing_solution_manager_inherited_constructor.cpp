#include <easylocal/cost.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/helpers/solution_manager.hpp>

struct Instance {};
struct Solution { int value{}; };

class BrokenSolutionManager
    : public easylocal::solution_manager_base<Instance, Solution>
{
public:
    [[nodiscard]] bool is_valid(const Solution&) const noexcept
    {
        return true;
    }
};

struct Component
{
    explicit Component(const Instance&) noexcept {}
    [[nodiscard]] int evaluate(const Solution& solution) const noexcept
    {
        return solution.value;
    }
};

int main()
{
    const Instance instance{};
    const auto recipe =
        easylocal::solution_manager<BrokenSolutionManager>()
        | easylocal::component<Component>();
    (void)recipe.construct(instance);
}
