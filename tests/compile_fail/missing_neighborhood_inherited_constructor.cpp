#include <easylocal/runners/runner.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>

struct Instance {};
struct Solution {};
struct Move {};

class SolutionManager
    : public easylocal::solution_manager_base<Instance, Solution>
{
public:
    using solution_manager_base::solution_manager_base;
    [[nodiscard]] bool is_valid(const Solution&) const noexcept
    {
        return true;
    }
};

class BrokenNeighborhood
    : public easylocal::neighborhood_explorer_base<SolutionManager, Move>
{
public:
    [[nodiscard]] static bool is_valid(const Solution&, const Move&) noexcept
    {
        return true;
    }
    void make_move(Solution&, const Move&) const noexcept {}
};

int main()
{
    const Instance instance{};
    SolutionManager manager{instance};
    const auto recipe = easylocal::neighborhood<BrokenNeighborhood>();
    (void)recipe.construct(manager);
}
