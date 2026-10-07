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

    [[nodiscard]] const Instance& input() const noexcept
    {
        return instance_;
    }
    [[nodiscard]] static bool is_valid(const Solution&) noexcept
    {
        return true;
    }

private:
    const Instance& instance_;
};

struct Value
{
    [[nodiscard]] static int evaluate(const Solution& solution) noexcept
    {
        return solution.value;
    }
};

// Larger values are better, against the sign of cost::delta that the
// Metropolis criterion reads.
struct Maximize
{
    [[nodiscard]] constexpr int operator()(const int value) const noexcept
    {
        return value;
    }

    [[nodiscard]] constexpr std::partial_ordering compare(
        const int lhs,
        const int rhs) const noexcept
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

    [[nodiscard]] const Instance& input() const noexcept
    {
        return instance_;
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static std::optional<Move> random_move(const Solution&, RNG&)
    {
        return Move{.delta = -1};
    }

    [[nodiscard]] static bool is_valid(const Solution&, const Move&) noexcept
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
