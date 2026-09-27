#include <easylocal/app.hpp>
#include <easylocal/search/first_improvement.hpp>
#include <easylocal/service_base.hpp>
#include <easylocal/tester.hpp>

struct Input {};
struct Solution {};
struct Move {};

class SolutionManager
    : public easylocal::solution_manager_base<Input, Solution>
{
public:
    using solution_manager_base::solution_manager_base;
    using cost_type = int;

    [[nodiscard]]
    static auto is_valid(const Solution&) noexcept -> bool
    {
        return true;
    }

    [[nodiscard]]
    static auto evaluate(const Solution&) noexcept -> cost_type
    {
        return 0;
    }
};

class Neighborhood
    : public easylocal::neighborhood_explorer_base<SolutionManager, Move>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]]
    static auto is_valid(const Solution&, const Move&) noexcept -> bool
    {
        return true;
    }

    static void make_move(Solution&, const Move&) noexcept
    {
    }
};

int main()
{
    auto application = easylocal::app("missing-solution-factory")
        .solution_manager<SolutionManager>()
        .neighborhood<Neighborhood>()
        .runner<easylocal::runner::first_improvement>("fi");

    easylocal::Tester tester{std::move(application)};
    (void)tester;
}
