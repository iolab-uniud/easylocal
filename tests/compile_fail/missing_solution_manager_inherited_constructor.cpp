#include <easylocal/cost.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/helpers/solution_manager.hpp>

struct Instance {};
struct Solution { int value{}; };

class BrokenSolutionManager
    : public easylocal::solution_manager_base<Instance, Solution>
{
public:
    [[nodiscard]] auto is_valid(const Solution&) const noexcept -> bool { return true; }
};

struct Component
{
    explicit Component(const Instance&) noexcept {}
    [[nodiscard]] auto evaluate(const Solution& solution) const noexcept -> int
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
