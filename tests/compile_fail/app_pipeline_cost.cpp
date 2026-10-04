// A pipeline registered in an app must end with the app's cost: here its only
// stage works on the hard cost.
#include <easylocal/app/app.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/solvers/pipeline.hpp>

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

    [[nodiscard]]
    const Instance& input() const noexcept
    {
        return instance_;
    }

    [[nodiscard]]
    static bool is_valid(const Solution&) noexcept
    {
        return true;
    }

    [[nodiscard]]
    static Solution initial_solution()
    {
        return {};
    }

private:
    const Instance& instance_;
};

struct Hard
{
    [[nodiscard]]
    static int evaluate(const Solution& solution)
    {
        return solution.hard;
    }
};

struct Soft
{
    [[nodiscard]]
    static int evaluate(const Solution& solution)
    {
        return solution.soft;
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

    [[nodiscard]]
    const Instance& input() const noexcept
    {
        return sm_.input();
    }

    [[nodiscard]]
    static bool is_valid(const Solution&, const Move&) noexcept
    {
        return true;
    }

    static void make_move(Solution&, const Move&) noexcept {}

private:
    const SolutionManager& sm_;
};

struct Keep
{
    template<class Context>
    auto run(const Context& context, Solution solution) const
    {
        struct Outcome
        {
            Solution solution;
            typename Context::cost_type cost;
        };
        return Outcome{solution, context.evaluation().evaluate(solution).cost()};
    }
};

int main()
{
    namespace solvers = easylocal::solvers;
    auto sm = easylocal::solution_manager<SolutionManager>()
        | easylocal::cost::hard_soft(
            easylocal::component<Hard>(),
            easylocal::component<Soft>());
    auto nhe = easylocal::neighborhood<Neighborhood>();
    auto runner = easylocal::Runner{Keep{}} | sm | nhe;
    auto application = easylocal::app("hard only") | sm | nhe
        | easylocal::pipeline(
            "feasible",
            solvers::stage("feasible", runner) & solvers::until_feasible());
    static_cast<void>(application);
}
