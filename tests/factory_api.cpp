#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/solvers.hpp>

#include <cassert>
#include <concepts>
#include <cstdint>
#include <optional>
#include <random>
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

    explicit SolutionManager(const Instance& instance) : instance_{instance} {}

    [[nodiscard]] auto input() const noexcept -> const Instance& { return instance_; }
    [[nodiscard]] auto is_valid(const Solution&) const noexcept -> bool { return true; }
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

struct MoveDelta
{
    int value{};
};

[[maybe_unused]] [[nodiscard]] constexpr auto operator+(const int value, const MoveDelta delta) noexcept -> int
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

// A parameterized algorithm that cannot be copied: a Runner holds its
// parameters, so a const runner still binds.
struct MoveOnlyAlgorithm
{
    using parameters_type = easylocal::runners::FirstImprovementParameters;

    explicit MoveOnlyAlgorithm(const parameters_type&) {}
    MoveOnlyAlgorithm(const MoveOnlyAlgorithm&) = delete;
    MoveOnlyAlgorithm(MoveOnlyAlgorithm&&) = default;
    MoveOnlyAlgorithm& operator=(const MoveOnlyAlgorithm&) = delete;
    MoveOnlyAlgorithm& operator=(MoveOnlyAlgorithm&&) = default;
    ~MoveOnlyAlgorithm() = default;

    template<class Context>
    [[nodiscard]] auto run(
        const Context& context,
        typename Context::solution_type solution) const
    {
        return IdentityAlgorithm{}.run(context, std::move(solution));
    }
};

// Components whose recipe arguments are doubles, given as ints: the recipes
// construct with parentheses, as std::constructible_from checks, so an int
// converts to a double instead of being rejected as narrowing.
class ScaledSolutionManager
{
public:
    using input_type = Instance;
    using solution_type = Solution;

    ScaledSolutionManager(const Instance& instance, double scale)
        : instance_{instance}, scale_{scale}
    {
    }

    const Instance& input() const noexcept
    {
        return instance_;
    }
    bool is_valid(const Solution&) const noexcept
    {
        return scale_ > 0.0;
    }
    Solution initial_solution() const
    {
        return {};
    }

private:
    const Instance& instance_;
    double scale_;
};

struct ScaledMove
{
    explicit ScaledMove(double amount) : amount{amount} {}

    double amount;
};

class ScaledNeighborhoodExplorer
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = ScaledMove;

    ScaledNeighborhoodExplorer(ScaledSolutionManager& sm, double step)
        : instance_{sm.input()}, step_{step}
    {
    }

    const Instance& input() const noexcept
    {
        return instance_;
    }
    auto moves(const Solution&) const
    {
        return std::views::single(ScaledMove{step_});
    }
    // An int, from which the move is built.
    std::optional<int> random_move(const Solution&, std::mt19937_64&) const
    {
        return static_cast<int>(step_);
    }
    static bool is_valid(const Solution&, const ScaledMove&) noexcept
    {
        return true;
    }
    void make_move(Solution&, const ScaledMove&) const {}

private:
    const Instance& instance_;
    double step_;
};

class ScaledComponent
{
public:
    ScaledComponent(const Instance&, double weight) : weight_{weight} {}

    int evaluate(const Solution& solution) const noexcept
    {
        return static_cast<int>(weight_) * solution.value;
    }

private:
    double weight_;
};

class ScaledDeltaEvaluator
{
public:
    explicit ScaledDeltaEvaluator(double weight) : weight_{weight} {}

    MoveDelta delta_evaluate(const Solution&, const ScaledMove&) const noexcept
    {
        return {static_cast<int>(weight_)};
    }

private:
    double weight_;
};

void recipe_arguments_convert_as_constructors_take_them()
{
    using namespace easylocal;
    auto runner = make_runner<IdentityAlgorithm>()
        | (solution_manager<ScaledSolutionManager>(2) | component<ScaledComponent>(3))
        | (neighborhood<ScaledNeighborhoodExplorer>(4)
            | delta<ScaledComponent, ScaledDeltaEvaluator>(5));
    auto solver =
        make_solver<solvers::LocalSearch>(runner)
            .initialization(initialization::initial)
            .seed(17);
    const Instance instance{};
    assert(solver.solve(instance).cost == 0);

    ScaledSolutionManager sm{instance, 2.0};
    const ScaledNeighborhoodExplorer explorer{sm, 4.0};
    std::mt19937_64 rng{17};
    const auto move = easylocal::random_move(explorer, Solution{}, rng);
    assert(move && move->amount == 4.0);
}

} // namespace

int main()
{
    using namespace easylocal;

    struct SolutionOnly
    {
        Solution solution;
    };
    static_assert(search_result_for<search_result<Solution, int>, Solution, int>);
    static_assert(!search_result_for<SolutionOnly, Solution, int>);

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

    auto sa = easylocal::make_runner<
        runners::SimulatedAnnealing<runners::temperature::Classic>>(
        {.temperature = runners::temperature::ClassicParameters{
             .initial_temperature = 10.0,
             .final_temperature = 1.0,
             .cooling_rate = 0.9,
             .samples_per_temperature = 4}});
    static_assert(std::same_as<
        decltype(sa),
        Runner<runners::SimulatedAnnealing<runners::temperature::Classic>>>);

    const auto sm_pipe =
        solution_manager<SolutionManager>()
        | easylocal::cost::sum(component<CostComponent>());
    const auto sm_fluent =
        solution_manager<SolutionManager>()
            .with_cost(easylocal::cost::sum(component<CostComponent>()));
    static_assert(std::same_as<
        std::remove_cvref_t<decltype(sm_pipe)>,
        std::remove_cvref_t<decltype(sm_fluent)>>);

    const auto nhe_pipe =
        neighborhood<NeighborhoodExplorer>()
        | delta<CostComponent, DeltaEvaluator>();
    const auto nhe_fluent =
        neighborhood<NeighborhoodExplorer>()
            .with_delta<CostComponent, DeltaEvaluator>();
    static_assert(std::same_as<
        std::remove_cvref_t<decltype(nhe_pipe)>,
        std::remove_cvref_t<decltype(nhe_fluent)>>);

    const auto sm_recipe = solution_manager<SolutionManager>();
    const auto nhe_recipe = neighborhood<NeighborhoodExplorer>();
    static_assert(detail::is_solution_manager_spec_v<std::remove_cvref_t<decltype(sm_recipe)>>);
    static_assert(detail::is_neighborhood_spec_v<std::remove_cvref_t<decltype(nhe_recipe)>>);

    auto configured_runner =
        make_runner<IdentityAlgorithm>()
        | (solution_manager<SolutionManager>() | component<CostComponent>())
        | neighborhood<NeighborhoodExplorer>();

    auto local_solver =
        make_solver<solvers::LocalSearch>(configured_runner)
            .initialization(initialization::initial)
            .seed(17);

    const Instance instance{};
    const auto move_only = make_runner<MoveOnlyAlgorithm>()
        | (solution_manager<SolutionManager>() | component<CostComponent>())
        | neighborhood<NeighborhoodExplorer>();
    static_assert(std::same_as<
        typename std::remove_cvref_t<decltype(move_only)>::cost_type,
        typename decltype(configured_runner)::cost_type>);
    auto move_only_bound = move_only.bind(instance);
    assert(move_only_bound.run(Solution{}).cost == 0);

    const auto local_result = local_solver.solve(instance);
    assert(local_result.solution.value == 0);
    assert(local_result.cost == 0);

    auto multistart_solver =
        make_solver<solvers::MultiStart>(
            configured_runner,
            solvers::MultiStartParameters{.starts = 3})
            .initialization(initialization::initial)
            .seed(17);
    const auto multistart_result = multistart_solver.solve(instance);
    assert(multistart_result.solution.value == 0);
    assert(multistart_result.cost == 0);

    recipe_arguments_convert_as_constructors_take_them();
    return 0;
}
