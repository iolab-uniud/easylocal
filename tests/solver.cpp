#include <easylocal/solvers.hpp>

#include <concepts>
#include <cstdint>
#include <iostream>
#include <random>
#include <string_view>
#include <utility>

namespace
{
struct Instance { int initial{11}; };
struct Solution { std::uint64_t value{}; };
struct Move {};

struct ValueCost
{
    static auto evaluate(const Solution& s) -> std::int64_t
    {
        return static_cast<std::int64_t>(s.value);
    }
};

class DeterministicSM
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    explicit DeterministicSM(const Instance& instance) : instance_{instance} {}
    auto input() const -> const Instance& { return instance_; }
    static auto is_valid(const Solution&) -> bool { return true; }
    auto initial_solution() const -> Solution
    {
        return {static_cast<std::uint64_t>(instance_.initial)};
    }
private:
    const Instance& instance_;
};

class RandomSM
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    explicit RandomSM(const Instance& instance) : instance_{instance} {}
    auto input() const -> const Instance& { return instance_; }
    static auto is_valid(const Solution&) -> bool { return true; }
    template<class RNG>
    auto random_solution(RNG& rng) const -> Solution { return {rng()}; }
private:
    const Instance& instance_;
};

class SelectableSM
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    explicit SelectableSM(const Instance& instance) : instance_{instance} {}
    auto input() const -> const Instance& { return instance_; }
    static auto is_valid(const Solution&) -> bool { return true; }
    auto initial_solution() const -> Solution
    {
        return {static_cast<std::uint64_t>(instance_.initial)};
    }
    template<class RNG>
    auto random_solution(RNG& rng) const -> Solution { return {rng()}; }
private:
    const Instance& instance_;
};

template<class SM>
class EmptyNeighborhood
{
public:
    using input_type = typename SM::input_type;
    using solution_type = typename SM::solution_type;
    using move_type = Move;
    explicit EmptyNeighborhood(const SM& sm) : instance_{sm.input()} {}
    auto input() const -> const input_type& { return instance_; }
    [[nodiscard]] static auto is_valid(const solution_type&, const move_type&) noexcept -> bool { return true; }
    static void make_move(solution_type&, const move_type&) {}
private:
    const input_type& instance_;
};

struct PlainResult { Solution solution; };
struct PlainAlgorithm
{
    template<class Context>
    auto run(const Context&, typename Context::solution_type solution) const
        -> PlainResult
    {
        return {std::move(solution)};
    }
};

struct RandomResult { Solution solution; std::uint64_t draw; };
struct RandomAlgorithm
{
    template<class Context, class RNG>
    auto run(
        const Context&,
        typename Context::solution_type solution,
        RNG& rng) const -> RandomResult
    {
        return {std::move(solution), rng()};
    }
};

// Whether the solver's initialization() takes the tag Initialization.
template<class Solver, class Initialization>
concept accepts = requires(Solver& solver) { solver.initialization(Initialization{}); };

bool expect(bool condition, std::string_view message)
{
    if (!condition) std::cerr << "FAILED: " << message << '\n';
    return condition;
}
}

int main()
{
    using namespace easylocal;
    bool ok = true;
    const Instance instance{};

    auto deterministic_runner = Runner{PlainAlgorithm{}}
        | (solution_manager<DeterministicSM>() | component<ValueCost>())
        | neighborhood<EmptyNeighborhood<DeterministicSM>>();
    using DeterministicRunner = decltype(deterministic_runner);
    using DeterministicSolver = solvers::LocalSearch<DeterministicRunner>;
    static_assert(DeterministicSolver::supports_initial);
    static_assert(!DeterministicSolver::supports_random);
    // A tag the SolutionManager does not support is rejected at compile time.
    static_assert(accepts<DeterministicSolver, initialization::Initial>);
    static_assert(accepts<DeterministicSolver, initialization::Automatic>);
    static_assert(!accepts<DeterministicSolver, initialization::Random>);

    auto deterministic_solver =
        make_solver<solvers::LocalSearch>(deterministic_runner)
            .initialization(initialization::initial)
            .seed(42);
    const auto deterministic = deterministic_solver.solve(instance);
    ok &= expect(
        deterministic.solution.value == 11,
        "deterministic initialization is delegated through the bound SolutionManager");

    // By default, the only initialization it supports.
    ok &= expect(
        make_solver<solvers::LocalSearch>(deterministic_runner)
                .solve(instance)
                .solution.value
            == 11,
        "automatic initialization falls back on the initial solution");

    auto random_runner = Runner{RandomAlgorithm{}}
        | (solution_manager<RandomSM>() | component<ValueCost>())
        | neighborhood<EmptyNeighborhood<RandomSM>>();
    using RandomRunner = decltype(random_runner);
    using RandomSolver = solvers::LocalSearch<RandomRunner>;
    static_assert(!RandomSolver::supports_initial);
    static_assert(RandomSolver::supports_random);

    auto random_solver =
        make_solver<solvers::LocalSearch>(random_runner)
            .initialization(initialization::random)
            .seed(1234);

    std::mt19937_64 reference{1234};
    const auto expected_initial = reference();
    const auto expected_algorithm = reference();
    const auto random = random_solver.solve(instance);
    ok &= expect(
        random.solution.value == expected_initial,
        "random initialization consumes the Solver-owned RNG");
    ok &= expect(
        random.draw == expected_algorithm,
        "random-aware Runner continues the same Solver-owned RNG stream");

    const auto second = random_solver.solve(instance);
    ok &= expect(
        second.solution.value == reference() && second.draw == reference(),
        "Solver RNG state persists across solve calls");

    auto selectable_runner = Runner{PlainAlgorithm{}}
        | (solution_manager<SelectableSM>() | component<ValueCost>())
        | neighborhood<EmptyNeighborhood<SelectableSM>>();
    using SelectableRunner = decltype(selectable_runner);
    using SelectableSolver = solvers::LocalSearch<SelectableRunner>;
    static_assert(SelectableSolver::supports_initial);
    static_assert(SelectableSolver::supports_random);

    solvers::LocalSearch selectable_solver{selectable_runner, std::mt19937_64{7}};
    std::mt19937_64 selectable_reference{7};
    ok &= expect(
        selectable_solver.solve(instance).solution.value == selectable_reference(),
        "automatic initialization prefers a random solution");

    selectable_solver.initialization(initialization::initial);
    ok &= expect(
        selectable_solver.solve(instance).solution.value == 11,
        "the initialization chosen on an lvalue is used");

    // The builders return the solver itself on an lvalue, a new one on a
    // temporary.
    static_assert(
        std::same_as<decltype(selectable_solver.seed(1)), decltype(selectable_solver)&>);
    static_assert(std::same_as<
        decltype(std::move(selectable_solver).initialization(initialization::random)),
        decltype(selectable_solver)>);
    ok &= expect(
        &selectable_solver.seed(3).initialization(initialization::random)
            == &selectable_solver,
        "the builders chain on an lvalue");

    return ok ? 0 : 1;
}
