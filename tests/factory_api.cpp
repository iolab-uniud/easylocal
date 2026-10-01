#include <easylocal/runners/runner.hpp>
#include <easylocal/solvers/solver.hpp>
#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

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
    using input_type = Instance;
    using solution_type = Solution;
    using cost_type = int;

    explicit SolutionManager(const Instance& instance) : instance_{instance} {}

    [[nodiscard]] auto input() const noexcept -> const Instance& { return instance_; }
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
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    explicit NeighborhoodExplorer(SolutionManager& sm) : instance_{sm.input()} {}

    [[nodiscard]] auto input() const noexcept -> const Instance& { return instance_; }
    [[nodiscard]] auto moves(const Solution&) const { return std::views::empty<Move>; }
    [[nodiscard]] static auto is_valid(const Solution&, const Move&) noexcept -> bool { return true; }
    void make_move(Solution&, const Move&) const {}

private:
    const Instance& instance_;
};


struct CostComponent
{
    [[nodiscard]] static auto evaluate(const Solution& solution) noexcept -> int
    {
        return solution.value;
    }
};

struct CostAggregator
{
    [[nodiscard]] static auto operator()(const int value) noexcept -> int
    {
        return value;
    }
};

struct MoveDelta
{
    int value{};
};

[[nodiscard]] constexpr auto operator+(const int value, const MoveDelta delta) noexcept -> int
{
    return value + delta.value;
}

struct DeltaEvaluator
{
    [[nodiscard]] static auto delta_evaluate(const Solution&, const Move&) noexcept -> MoveDelta
    {
        return {};
    }
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

struct ConfiguredAlgorithm
{
    explicit ConfiguredAlgorithm(int token) : token_{token} {}

    template<class Context>
    [[nodiscard]] auto run(const Context& context, typename Context::solution_type solution) const
    {
        struct Result
        {
            typename Context::solution_type solution;
            typename Context::cost_type cost;
        };
        return Result{solution, context.evaluation().evaluate(solution).cost() + token_ - token_};
    }

private:
    int token_{};
};


} // namespace

int main()
{
    using namespace easylocal;

    auto first = make_runner<runners::FirstImprovement>(
        runners::FirstImprovementParameters{.max_evaluations = 10});
    static_assert(std::same_as<
        decltype(first),
        Runner<runners::FirstImprovement>>);

    auto best = make_runner<runners::BestImprovement>(
        runners::BestImprovementParameters{.max_evaluations = 10});
    static_assert(std::same_as<
        decltype(best),
        Runner<runners::BestImprovement>>);

    auto configured = make_runner<ConfiguredAlgorithm>(7);
    static_assert(std::same_as<
        decltype(configured),
        Runner<ConfiguredAlgorithm>>);

    auto sa = Runner{runners::SimulatedAnnealing{runners::temperature::Classic{
        runners::temperature::ClassicParameters{
            .initial_temperature = 10.0,
            .final_temperature = 1.0,
            .cooling_rate = 0.9,
            .samples_per_temperature = 4}}}};
    static_assert(std::same_as<
        decltype(sa),
        Runner<runners::SimulatedAnnealing<runners::temperature::Classic>>>);

    const auto sm_pipe =
        solution_manager<SolutionManager>()
        | component<CostComponent>()
        | aggregator(CostAggregator{});
    const auto sm_fluent =
        make_solution_manager<SolutionManager>()
            .with_component<CostComponent>()
            .with_aggregator(CostAggregator{});
    static_assert(std::same_as<
        std::remove_cvref_t<decltype(sm_pipe)>,
        std::remove_cvref_t<decltype(sm_fluent)>>);

    const auto nhe_pipe =
        neighborhood<NeighborhoodExplorer>()
        | delta<CostComponent, DeltaEvaluator>();
    const auto nhe_fluent =
        make_neighborhood_explorer<NeighborhoodExplorer>()
            .with_delta<CostComponent, DeltaEvaluator>();
    static_assert(std::same_as<
        std::remove_cvref_t<decltype(nhe_pipe)>,
        std::remove_cvref_t<decltype(nhe_fluent)>>);

    const auto sm_recipe = make_solution_manager<SolutionManager>();
    const auto nhe_recipe = make_neighborhood_explorer<NeighborhoodExplorer>();
    static_assert(detail::is_solution_manager_spec_v<std::remove_cvref_t<decltype(sm_recipe)>>);
    static_assert(detail::is_neighborhood_spec_v<std::remove_cvref_t<decltype(nhe_recipe)>>);

    auto configured_runner =
        make_runner<IdentityAlgorithm>()
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
