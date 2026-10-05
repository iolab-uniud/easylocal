// Simulated Annealing accepts by the sign of cost::delta: a compare that
// maximizes is rejected when the compiler can evaluate it.
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#include <compare>
#include <optional>
#include <random>

struct Instance
{
};
struct Solution
{
    int value{};
};
struct Move
{
    int delta{};
};
class SolutionManager
{
public:
    using input_type = Instance;
    using solution_type = Solution;

    explicit SolutionManager(const Instance& instance) noexcept : instance_{instance} {}

    [[nodiscard]] auto input() const noexcept -> const Instance&
    {
        return instance_;
    }
    [[nodiscard]] static auto is_valid(const Solution&) noexcept -> bool
    {
        return true;
    }

private:
    const Instance& instance_;
};

struct Value
{
    [[nodiscard]] static auto evaluate(const Solution& solution) noexcept -> int
    {
        return solution.value;
    }
};

// Larger values are better, against the sign of cost::delta that the
// Metropolis criterion reads.
struct Maximize
{
    [[nodiscard]] constexpr auto operator()(const int value) const noexcept -> int
    {
        return value;
    }

    [[nodiscard]] constexpr auto compare(const int lhs, const int rhs) const noexcept
        -> std::partial_ordering
    {
        return rhs <=> lhs;
    }
};

class Neighborhood
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    explicit Neighborhood(const SolutionManager& manager) noexcept
        : instance_{manager.input()}
    {
    }

    [[nodiscard]] auto input() const noexcept -> const Instance&
    {
        return instance_;
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Solution&, RNG&) -> std::optional<Move>
    {
        return Move{.delta = -1};
    }

    [[nodiscard]] static auto is_valid(const Solution&, const Move&) noexcept -> bool
    {
        return true;
    }
    static void make_move(Solution& solution, const Move& move) noexcept
    {
        solution.value += move.delta;
    }

private:
    const Instance& instance_;
};

int main()
{
    using namespace easylocal;
    using namespace easylocal::runners;

    const Instance instance;
    auto runner = easylocal::make_runner<SimulatedAnnealing<>>()
        | (solution_manager<SolutionManager>()
            | cost::apply(Maximize{}, component<Value>()))
        | neighborhood<Neighborhood>();

    std::mt19937 rng{1U};
    (void)runner.bind(instance).run(Solution{}, rng);
}
