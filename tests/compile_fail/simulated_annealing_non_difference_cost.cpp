#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

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
struct StructuredCost
{
    int hard{};
    int soft{};

    friend bool operator<(const StructuredCost& lhs, const StructuredCost& rhs) noexcept
    {
        return lhs.hard < rhs.hard;
    }
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

struct StructuredValue
{
    [[nodiscard]] static StructuredCost evaluate(const Solution& solution) noexcept
    {
        return {.hard = solution.value, .soft = 0};
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
    auto runner =
        easylocal::make_runner<SimulatedAnnealing<temperature::FixedLength>>(
            {.temperature =
                    temperature::FixedLengthParameters{
                        .initial_temperature = 2.0,
                        .final_temperature = 1.0,
                        .cooling_rate = 0.5,
                        .allowed_iterations = 1,
                    }})
        | (solution_manager<SolutionManager>() | component<StructuredValue>())
        | neighborhood<Neighborhood>();

    std::mt19937 rng{1U};
    (void)runner.bind(instance).run(Solution{}, rng);
}
