#pragma once

// A problem with an integer cost, Solution<Tag> and its runner of Keep, an
// algorithm that returns the solution it is given; for the compile-fail tests
// of runners.

#include <easylocal/cost.hpp>
#include <easylocal/runners/runner.hpp>

namespace int_cost_fixture
{

struct Instance
{
};

template<int Tag>
struct Solution
{
    int value{};
};

template<int Tag>
class SolutionManager
{
public:
    using input_type = Instance;
    using solution_type = Solution<Tag>;

    explicit SolutionManager(const Instance& instance) : instance_{instance} {}

    [[nodiscard]]
    const Instance& input() const noexcept
    {
        return instance_;
    }

    [[nodiscard]]
    static bool is_valid(const Solution<Tag>&) noexcept
    {
        return true;
    }

    [[nodiscard]]
    static Solution<Tag> initial_solution()
    {
        return {};
    }

private:
    const Instance& instance_;
};

template<int Tag>
struct Value
{
    [[nodiscard]]
    static int evaluate(const Solution<Tag>& solution)
    {
        return solution.value;
    }
};

struct Move
{
};

template<int Tag>
class Neighborhood
{
public:
    using input_type = Instance;
    using solution_type = Solution<Tag>;
    using move_type = Move;

    explicit Neighborhood(const SolutionManager<Tag>& sm) : sm_{sm} {}

    [[nodiscard]]
    const Instance& input() const noexcept
    {
        return sm_.input();
    }

    [[nodiscard]]
    static bool is_valid(const Solution<Tag>&, const Move&) noexcept
    {
        return true;
    }

    static void make_move(Solution<Tag>&, const Move&) noexcept {}

private:
    const SolutionManager<Tag>& sm_;
};

struct Keep
{
    template<class Context, class S>
    auto run(const Context& context, S solution) const
    {
        struct Outcome
        {
            S solution;
            typename Context::cost_type cost;
        };
        return Outcome{solution, context.evaluation().evaluate(solution).cost()};
    }
};

template<int Tag>
auto runner()
{
    auto sm = easylocal::solution_manager<SolutionManager<Tag>>()
        | easylocal::component<Value<Tag>>();
    auto nhe = easylocal::neighborhood<Neighborhood<Tag>>();
    return easylocal::Runner{Keep{}} | sm | nhe;
}

} // namespace int_cost_fixture
