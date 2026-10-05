// The explorer contract is checked member by member when the recipe is
// written: a make_move that is not const is named, not reported deep inside
// the library.
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/recipes.hpp>
#include <easylocal/helpers/solution_manager.hpp>

struct Instance
{
};

struct Solution
{
    int value{};
};

struct Move
{
};

class SolutionManager : public easylocal::solution_manager_base<Instance, Solution>
{
public:
    using solution_manager_base::solution_manager_base;
};

class MutableNeighborhood
    : public easylocal::neighborhood_explorer_base<SolutionManager, Move>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]] static bool is_valid(const Solution&, const Move&) noexcept
    {
        return true;
    }

    void make_move(Solution& solution, const Move&)
    {
        ++solution.value;
        ++applied_;
    }

private:
    int applied_{};
};

int main()
{
    [[maybe_unused]] const auto recipe = easylocal::neighborhood<MutableNeighborhood>();
}
