// make_move taking the Solution by value changes a copy, and a descent never
// moves: the recipe rejects it, with the signature to write.
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

class CopyingNeighborhood
    : public easylocal::neighborhood_explorer_base<SolutionManager, Move>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]] static bool is_valid(const Solution&, const Move&) noexcept
    {
        return true;
    }

    static void make_move(Solution solution, const Move&) noexcept
    {
        ++solution.value;
    }
};

int main()
{
    [[maybe_unused]] const auto recipe = easylocal::neighborhood<CopyingNeighborhood>();
}
