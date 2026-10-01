#include <easylocal/solvers.hpp>

#include <concepts>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace
{
struct Instance { int initial{11}; };
struct Solution { std::uint64_t value{}; };
struct Move {};

struct ValueCost
{
    static auto evaluate(const Solution& s) -> std::uint64_t { return s.value; }
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
    static_assert(DeterministicSolver::supports(initialization::Mode::initial));
    static_assert(!DeterministicSolver::supports(initialization::Mode::random));
    static_assert(std::constructible_from<
        DeterministicSolver,
        DeterministicRunner,
        initialization::Initial,
        std::mt19937_64>);
    static_assert(!std::constructible_from<
        DeterministicSolver,
        DeterministicRunner,
        initialization::Random,
        std::mt19937_64>);

    auto deterministic_solver = make_solver<solvers::LocalSearch>(
        deterministic_runner,
        solvers::LocalSearchConfig<initialization::Initial>{
            .initialization = initialization::initial,
            .seed = 42});
    const auto deterministic = deterministic_solver.solve(instance);
    ok &= expect(
        deterministic.solution.value == 11,
        "deterministic initialization is delegated through the bound SolutionManager");

    bool rejected_unsupported_runtime_mode = false;
    try
    {
        [[maybe_unused]] solvers::LocalSearch runtime_selected{
            deterministic_runner,
            initialization::Mode::random,
            std::mt19937_64{42}};
    }
    catch (const std::invalid_argument&)
    {
        rejected_unsupported_runtime_mode = true;
    }
    ok &= expect(
        rejected_unsupported_runtime_mode,
        "runtime initialization rejects a mode unsupported by the SolutionManager");

    DeterministicSolver deterministic_runtime_selected{
        deterministic_runner,
        initialization::Mode::initial,
        std::mt19937_64{42}};
    bool rejected_unsupported_runtime_setter = false;
    try
    {
        deterministic_runtime_selected.initialization_mode(
            initialization::Mode::random);
    }
    catch (const std::invalid_argument&)
    {
        rejected_unsupported_runtime_setter = true;
    }
    ok &= expect(
        rejected_unsupported_runtime_setter &&
            deterministic_runtime_selected.initialization_mode() ==
                initialization::Mode::initial,
        "runtime initialization setter rejects unsupported modes without changing state");

    auto random_runner = Runner{RandomAlgorithm{}}
        | (solution_manager<RandomSM>() | component<ValueCost>())
        | neighborhood<EmptyNeighborhood<RandomSM>>();
    using RandomRunner = decltype(random_runner);
    using RandomSolver = solvers::LocalSearch<RandomRunner>;
    static_assert(!RandomSolver::supports_initial);
    static_assert(RandomSolver::supports_random);

    auto random_solver = make_solver<solvers::LocalSearch>(
        random_runner,
        solvers::LocalSearchConfig<initialization::Random>{
            .initialization = initialization::random,
            .seed = 1234});

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

    solvers::LocalSearch selectable_solver{
        selectable_runner,
        initialization::Mode::initial,
        std::mt19937_64{7}};
    ok &= expect(
        selectable_solver.initialization_mode() == initialization::Mode::initial,
        "runtime-selected Solver reports its initialization mode");
    ok &= expect(
        selectable_solver.solve(instance).solution.value == 11,
        "runtime-selected deterministic initialization is used");

    selectable_solver.initialization_mode(initialization::Mode::random);
    std::mt19937_64 selectable_reference{7};
    ok &= expect(
        selectable_solver.solve(instance).solution.value == selectable_reference(),
        "runtime initialization can be changed to another supported mode");

    return ok ? 0 : 1;
}
