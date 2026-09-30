#pragma once

#include <easylocal/runner.hpp>

#include <compare>
#include <ranges>

namespace compile_fail_fixture
{

struct Instance
{
};

struct Solution
{
    int value{};
};

struct Move
{
    int delta{1};
};

struct Cost
{
    int value{};

    auto operator<=>(const Cost&) const = default;
};

class BaseSolutionManager
{
public:
    using input_type = Instance;
    using solution_type = Solution;

    explicit BaseSolutionManager(const Instance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto input() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]]
    auto is_valid(const Solution&) const noexcept -> bool
    {
        return true;
    }

private:
    const Instance& instance_;
};

struct CostAggregator
{
    [[nodiscard]]
    auto operator()(const int first) const noexcept -> Cost
    {
        return Cost{first};
    }

    [[nodiscard]]
    auto operator()(const int first, const int second) const noexcept -> Cost
    {
        return Cost{first + second};
    }
};

struct ComponentA
{
    using value_type = int;

    explicit ComponentA(const Instance&) noexcept
    {
    }

    [[nodiscard]]
    auto evaluate(const Solution& solution) const noexcept -> int
    {
        return solution.value;
    }
};

struct ComponentB
{
    using value_type = int;

    explicit ComponentB(const Instance&) noexcept
    {
    }

    [[nodiscard]]
    auto evaluate(const Solution& solution) const noexcept -> int
    {
        return solution.value;
    }
};

class Neighborhood
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    explicit Neighborhood(const BaseSolutionManager& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto input() const noexcept -> const Instance&
    {
        return solution_manager_.input();
    }

    [[nodiscard]]
    auto moves(const Solution&) const
    {
        return std::views::single(Move{});
    }

    [[nodiscard]] static auto is_valid(const Solution&, const Move&) noexcept -> bool { return true; }

    void make_move(Solution& solution, const Move& move) const noexcept
    {
        solution.value += move.delta;
    }

private:
    const BaseSolutionManager& solution_manager_;
};

class AnotherNeighborhood : public Neighborhood
{
public:
    using Neighborhood::Neighborhood;
};

struct DeltaA
{
    explicit DeltaA(const Instance&) noexcept
    {
    }

    [[nodiscard]]
    auto delta_evaluate(const Solution&, const Move& move) const noexcept -> int
    {
        return move.delta;
    }
};

struct AnotherDeltaA
{
    explicit AnotherDeltaA(const Instance&) noexcept
    {
    }

    [[nodiscard]]
    auto delta_evaluate(const Solution&, const Move& move) const noexcept -> int
    {
        return move.delta;
    }
};

struct DeltaB
{
    explicit DeltaB(const Instance&) noexcept
    {
    }

    [[nodiscard]]
    auto delta_evaluate(const Solution&, const Move& move) const noexcept -> int
    {
        return move.delta;
    }
};

struct MalformedDeltaA
{
    explicit MalformedDeltaA(const Instance&) noexcept
    {
    }

    [[nodiscard]]
    auto delta_evaluate(const Solution&, const Move&) const noexcept
        -> const char*
    {
        return "not applicable";
    }
};

struct MissingDeltaEvaluateA
{
    explicit MissingDeltaEvaluateA(const Instance&) noexcept
    {
    }
};

struct NonConstructibleDeltaA
{
    NonConstructibleDeltaA() = delete;

    [[nodiscard]]
    auto delta_evaluate(const Solution&, const Move& move) const noexcept -> int
    {
        return move.delta;
    }
};

struct NonConstructibleComponent
{
    using value_type = int;

    NonConstructibleComponent() = delete;

    [[nodiscard]]
    auto evaluate(const Solution& solution) const noexcept -> int
    {
        return solution.value;
    }
};

struct Algorithm
{
};

} // namespace compile_fail_fixture
