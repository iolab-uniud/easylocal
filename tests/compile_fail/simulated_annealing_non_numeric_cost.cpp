#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#include <optional>
#include <random>

struct Instance {};
struct Solution { int value{}; };
struct Move { int delta{}; };
struct StructuredCost
{
    int hard{};
    int soft{};

    friend auto operator<(const StructuredCost& lhs, const StructuredCost& rhs) noexcept
        -> bool
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

    [[nodiscard]] auto input() const noexcept -> const Instance& { return instance_; }
    [[nodiscard]] static auto is_valid(const Solution&) noexcept -> bool { return true; }
private:
    const Instance& instance_;
};

struct StructuredValue
{
    [[nodiscard]] static auto evaluate(const Solution& solution) noexcept -> StructuredCost
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

    explicit Neighborhood(const SolutionManager& manager) noexcept : instance_{manager.input()} {}

    [[nodiscard]] auto input() const noexcept -> const Instance& { return instance_; }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Solution&, RNG&) -> std::optional<Move>
    {
        return Move{.delta = -1};
    }

    [[nodiscard]] static auto is_valid(const Solution&, const Move&) noexcept -> bool { return true; }
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
                        .max_iterations = 1,
                    }})
        | (solution_manager<SolutionManager>() | component<StructuredValue>())
        | neighborhood<Neighborhood>();

    std::mt19937 rng{1U};
    (void)runner.bind(instance).run(Solution{}, rng);
}
