#include <easylocal/app/app.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/runners/first_improvement.hpp>

struct Input
{
};
struct Solution
{
};
struct Move
{
};

class SolutionManager : public easylocal::solution_manager_base<Input, Solution>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]]
    static bool is_valid(const Solution&) noexcept
    {
        return true;
    }
};

struct ZeroCost
{
    [[nodiscard]]
    static int evaluate(const Solution&) noexcept
    {
        return 0;
    }
};

class Neighborhood : public easylocal::neighborhood_explorer_base<SolutionManager, Move>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]]
    static bool is_valid(const Solution&, const Move&) noexcept
    {
        return true;
    }

    static void make_move(Solution&, const Move&) noexcept {}
};

int main()
{
    auto application =
        easylocal::app("missing-solution-factory")
            .with_solution_manager(
                easylocal::solution_manager<SolutionManager>()
                | easylocal::component<ZeroCost>())
            .with_neighborhood(easylocal::neighborhood<Neighborhood>())
            .with_runner<easylocal::runners::FirstImprovement>("fi");

    easylocal::Session session{std::move(application)};
    (void)session;
}
