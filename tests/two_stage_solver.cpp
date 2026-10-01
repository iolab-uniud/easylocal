#include <easylocal/cost.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/solvers.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#include <concepts>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <string_view>
#include <utility>

namespace
{

struct Instance
{
};

struct Solution
{
    int hard{};
    int soft{};
};

class SolutionManager
{
public:
    using input_type = Instance;
    using solution_type = Solution;

    explicit SolutionManager(const Instance& instance) : instance_{instance} {}

    [[nodiscard]] auto input() const noexcept -> const Instance& { return instance_; }
    [[nodiscard]] auto is_valid(const Solution&) const noexcept -> bool { return true; }
    [[nodiscard]] auto initial_solution() const -> Solution { return {5, 9}; }
private:
    const Instance& instance_;
};

// A single component with a structured value: the implicit identity aggregator
// makes its hierarchical value the cost.
struct HierarchicalValue
{
    [[nodiscard]] static auto evaluate(const Solution& solution)
        -> easylocal::cost::hierarchical<int, int>
    {
        return easylocal::cost::hierarchical{solution.hard, solution.soft};
    }
};

struct Move
{
};

class Neighborhood
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    explicit Neighborhood(const SolutionManager& sm) : sm_{sm} {}

    [[nodiscard]] auto input() const noexcept -> const Instance& { return sm_.input(); }
    [[nodiscard]] static auto is_valid(const Solution&, const Move&) noexcept -> bool { return true; }
    void make_move(Solution&, const Move&) const noexcept {}

private:
    const SolutionManager& sm_;
};

template<class Cost>
struct Result
{
    Solution solution;
    Cost cost;
};

struct HardStage
{
    template<class Context>
    [[nodiscard]] auto run(const Context& context, Solution solution) const
    {
        static_assert(std::same_as<typename Context::cost_type, int>);
        solution.hard = 0;
        return Result<int>{solution, context.evaluation().evaluate(solution).cost()};
    }
};

struct FullStage
{
    template<class Context>
    [[nodiscard]] auto run(const Context& context, Solution solution) const
    {
        static_assert(easylocal::cost::hierarchical_type<
            typename Context::cost_type>);
        solution.soft = 1;
        return Result<typename Context::cost_type>{
            solution,
            context.evaluation().evaluate(solution).cost()};
    }
};

struct HardMove
{
};

class HardNeighborhood
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = HardMove;

    explicit HardNeighborhood(const SolutionManager& sm) : sm_{sm} {}

    [[nodiscard]] auto input() const noexcept -> const Instance& { return sm_.input(); }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Solution& solution, RNG&)
        -> std::optional<move_type>
    {
        return solution.hard > 0
            ? std::optional<move_type>{move_type{}}
            : std::nullopt;
    }

    [[nodiscard]] static auto is_valid(const Solution&, const move_type&) noexcept -> bool { return true; }
    static void make_move(Solution& solution, const move_type&) noexcept
    {
        --solution.hard;
    }

private:
    const SolutionManager& sm_;
};

struct SoftMove
{
};

class SoftNeighborhood
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = SoftMove;

    explicit SoftNeighborhood(const SolutionManager& sm) : sm_{sm} {}

    [[nodiscard]] auto input() const noexcept -> const Instance& { return sm_.input(); }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Solution& solution, RNG&)
        -> std::optional<move_type>
    {
        return solution.hard == 0 && solution.soft > 0
            ? std::optional<move_type>{move_type{}}
            : std::nullopt;
    }

    [[nodiscard]] static auto is_valid(const Solution&, const move_type&) noexcept -> bool { return true; }
    static void make_move(Solution& solution, const move_type&) noexcept
    {
        --solution.soft;
    }

private:
    const SolutionManager& sm_;
};

auto expect(bool condition, std::string_view message) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

} // namespace

int main()
{
    using namespace easylocal;

    const auto sm = make_solution_manager<SolutionManager>() | component<HierarchicalValue>();
    const auto nhe = make_neighborhood_explorer<Neighborhood>();

    auto first_runner = Runner{HardStage{}} | sm | nhe;
    auto second_runner = Runner{FullStage{}} | sm | nhe;

    using HardRunner = decltype(first_runner.with_hard_cost());
    using HardSM = typename HardRunner::solution_manager_type;
    static_assert(std::same_as<typename HardSM::cost_type, int>);
    static_assert(detail::hierarchical_solution_manager<
        typename decltype(second_runner)::solution_manager_type>);

    auto solver = make_solver<solvers::TwoStage>(
        std::move(first_runner),
        std::move(second_runner),
        solvers::TwoStageConfig<initialization::Initial>{
            .initialization = initialization::initial,
            .seed = 42,
        });

    static_assert(decltype(solver)::supports_initial);

    auto shared_runner = Runner{HardStage{}} | sm | nhe;
    auto shared_solver = make_solver<solvers::TwoStage>(
        std::move(shared_runner),
        solvers::TwoStageConfig<initialization::Initial>{
            .initialization = initialization::initial,
            .seed = 42,
        });
    static_assert(decltype(shared_solver)::supports_initial);

    const Instance instance{};
    const auto result = solver.solve(instance);

    bool ok = true;
    ok &= expect(result.solution.hard == 0, "stage 1 optimizes the hard branch");
    ok &= expect(result.solution.soft == 1, "stage 2 receives the stage-1 solution");
    ok &= expect(result.cost.hard() == 0, "final result keeps hierarchical hard cost");
    ok &= expect(result.cost.soft() == 1, "final result keeps hierarchical soft cost");

    auto hard_sa_runner =
        Runner{runners::SimulatedAnnealing{
            runners::temperature::FixedLength{
                runners::temperature::FixedLengthParameters{
                    .initial_temperature = 2.0,
                    .final_temperature = 0.5,
                    .cooling_rate = 0.5,
                    .max_iterations = 32,
                }}}}
        | sm
        | make_neighborhood_explorer<HardNeighborhood>();

    auto full_sa_runner =
        Runner{runners::SimulatedAnnealing{
            runners::temperature::FixedLength{
                runners::temperature::FixedLengthParameters{
                    .initial_temperature = 2.0,
                    .final_temperature = 0.5,
                    .cooling_rate = 0.5,
                    .max_iterations = 32,
                }}}}
        | sm
        | make_neighborhood_explorer<SoftNeighborhood>();

    auto sa_solver = make_solver<solvers::TwoStage>(
        std::move(hard_sa_runner),
        std::move(full_sa_runner),
        solvers::TwoStageConfig<initialization::Initial>{
            .initialization = initialization::initial,
            .seed = 17,
        });

    const auto sa_result = sa_solver.solve(instance);
    ok &= expect(
        sa_result.solution.hard == 0 && sa_result.solution.soft == 0,
        "two-stage SA optimizes the numeric hard branch before the soft branch");
    ok &= expect(
        sa_result.cost.hard() == 0 && sa_result.cost.soft() == 0,
        "second-stage SA returns the full hierarchical cost");

    return ok ? 0 : 1;
}
