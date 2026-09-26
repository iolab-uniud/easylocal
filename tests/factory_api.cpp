#include <easylocal/runner.hpp>
#include <easylocal/solver.hpp>
#include <easylocal/search/best_improvement.hpp>
#include <easylocal/search/first_improvement.hpp>
#include <easylocal/search/simulated_annealing.hpp>
#include <easylocal/search/temperature_policy.hpp>

#include <cassert>
#include <concepts>
#include <cstdint>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

struct Instance
{
};

struct Solution
{
    int value{};
};

struct SolutionManager
{
    using instance_type = Instance;
    using solution_type = Solution;
    using cost_type = int;

    explicit SolutionManager(const Instance& instance) : instance_{instance} {}

    [[nodiscard]] auto instance() const noexcept -> const Instance& { return instance_; }
    [[nodiscard]] auto is_valid(const Solution&) const noexcept -> bool { return true; }
    [[nodiscard]] auto evaluate(const Solution& solution) const noexcept -> int { return solution.value; }
    [[nodiscard]] auto initial_solution() const -> Solution { return {}; }

private:
    const Instance& instance_;
};

struct Move
{
};

struct NeighborhoodExplorer
{
    using instance_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    explicit NeighborhoodExplorer(SolutionManager& sm) : instance_{sm.instance()} {}

    [[nodiscard]] auto instance() const noexcept -> const Instance& { return instance_; }
    [[nodiscard]] auto moves(const Solution&) const { return std::views::empty<Move>; }
    [[nodiscard]] static auto is_valid(const Solution&, const Move&) noexcept -> bool { return true; }
    void make_move(Solution&, const Move&) const {}

private:
    const Instance& instance_;
};

struct IdentityAlgorithm
{
    template<class Context>
    [[nodiscard]] auto run(const Context& context, typename Context::solution_type solution) const
    {
        struct Result
        {
            typename Context::solution_type solution;
            typename Context::cost_type cost;
        };
        return Result{solution, context.evaluation().evaluate(solution).cost()};
    }
};

struct identity_runner
{
    [[nodiscard]] static auto make() { return IdentityAlgorithm{}; }
};

} // namespace

int main()
{
    using namespace easylocal;

    auto first = make_runner<runner::first_improvement>(
        search::FirstImprovementParameters{.max_evaluations = 10});
    static_assert(std::same_as<
        decltype(first),
        Runner<search::FirstImprovement>>);

    auto best = make_runner<runner::best_improvement>(
        search::BestImprovementParameters{.max_evaluations = 10});
    static_assert(std::same_as<
        decltype(best),
        Runner<search::BestImprovement>>);

    runner::SimulatedAnnealingConfig sa_config{
        .temperature_policy = search::temperature::Classic{
            search::temperature::ClassicParameters{
                .initial_temperature = 10.0,
                .final_temperature = 1.0,
                .cooling_rate = 0.9,
                .samples_per_temperature = 4}},
    };
    auto sa = make_runner<runner::simulated_annealing>(std::move(sa_config));
    static_assert(std::same_as<
        decltype(sa),
        Runner<search::SimulatedAnnealing<search::temperature::Classic>>>);

    const auto sm_recipe = make_solution_manager<SolutionManager>();
    const auto nhe_recipe = make_neighborhood_explorer<NeighborhoodExplorer>();
    static_assert(detail::is_solution_manager_spec_v<std::remove_cvref_t<decltype(sm_recipe)>>);
    static_assert(detail::is_neighborhood_spec_v<std::remove_cvref_t<decltype(nhe_recipe)>>);

    auto configured_runner =
        make_runner<identity_runner>()
        | make_solution_manager<SolutionManager>()
        | make_neighborhood_explorer<NeighborhoodExplorer>();

    auto local_solver = make_solver<solver::local_search>(
        configured_runner,
        solver::LocalSearchConfig<initialization::Initial>{
            .initialization = initialization::initial,
            .seed = 17});

    const Instance instance{};
    const auto local_result = local_solver.solve(instance);
    assert(local_result.solution.value == 0);
    assert(local_result.cost == 0);

    auto multistart_solver = make_solver<solver::multistart>(
        configured_runner,
        solver::MultiStartConfig<initialization::Initial>{
            .parameters = {.starts = 3},
            .initialization = initialization::initial,
            .seed = 17});
    const auto multistart_result = multistart_solver.solve(instance);
    assert(multistart_result.solution.value == 0);
    assert(multistart_result.cost == 0);

    return 0;
}
