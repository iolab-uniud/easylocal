// A runner registered in an app must run on the app's neighborhood: First
// Improvement on an explorer without moves() is rejected when it is
// registered, not when a tool first runs it.
#include "service_composition_fixture.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/runners/first_improvement.hpp>

#include <optional>

class RandomOnlyNeighborhood
{
public:
    using input_type = compile_fail_fixture::Instance;
    using solution_type = compile_fail_fixture::Solution;
    using move_type = compile_fail_fixture::Move;

    explicit RandomOnlyNeighborhood(
        const compile_fail_fixture::BaseSolutionManager& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]] const input_type& input() const noexcept
    {
        return solution_manager_.input();
    }

    [[nodiscard]] static bool is_valid(const solution_type&, const move_type&) noexcept
    {
        return true;
    }

    static void make_move(solution_type& solution, const move_type& move) noexcept
    {
        solution.value += move.delta;
    }

    template<class RNG>
    [[nodiscard]] static std::optional<move_type> random_move(const solution_type&, RNG&)
    {
        return move_type{};
    }

private:
    const compile_fail_fixture::BaseSolutionManager& solution_manager_;
};

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::component;
    using easylocal::neighborhood;
    using easylocal::solution_manager;

    [[maybe_unused]] auto application = easylocal::app("random-only")
        | (solution_manager<BaseSolutionManager>()
            | easylocal::cost::apply(CostFunction{}, component<ComponentA>()))
        | neighborhood<RandomOnlyNeighborhood>()
        | easylocal::runner<easylocal::runners::FirstImprovement>("fi");
}
