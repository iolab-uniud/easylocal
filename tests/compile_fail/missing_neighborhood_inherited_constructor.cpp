#include <easylocal/runners/runner.hpp>
#include <easylocal/helpers/service_base.hpp>

struct Instance {};
struct Solution {};
struct Move {};

class SolutionManager
    : public easylocal::solution_manager_base<Instance, Solution>
{
public:
    using solution_manager_base::solution_manager_base;
    [[nodiscard]] auto is_valid(const Solution&) const noexcept -> bool { return true; }
};

class BrokenNeighborhood
    : public easylocal::neighborhood_explorer_base<SolutionManager, Move>
{
public:
    void make_move(Solution&, const Move&) const noexcept {}
};

int main()
{
    const Instance instance{};
    SolutionManager manager{instance};
    const auto recipe = easylocal::neighborhood<BrokenNeighborhood>();
    (void)recipe.construct(manager);
}
