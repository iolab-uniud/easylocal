#include "../examples/assignment/capacity_delta.hpp"
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/instance.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"

#include <easylocal/check.hpp>
#include <easylocal/search/best_improvement.hpp>
#include <easylocal/search/first_improvement.hpp>

#include <cassert>
#include <cstddef>
#include <optional>
#include <random>

namespace
{

using namespace easylocal::mwe::assignment;

[[nodiscard]] auto make_application()
{
    auto sm =
        easylocal::solution_manager<AssignmentSolutionManager>()
        | easylocal::component<CapacityCostComponent>()
        | easylocal::component<LoadImbalanceCostComponent>()
        | easylocal::aggregator(AssignmentCostAggregator{});

    auto nhe =
        easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::delta<
              CapacityCostComponent,
              ReassignCapacityDeltaEvaluator>();

    auto application =
        easylocal::app("assignment")
            .solution_manager(std::move(sm))
            .neighborhood(std::move(nhe))
            .runner<easylocal::runner::first_improvement>("fi")
            .runner<easylocal::runner::best_improvement>("bi");

    application
        .runner_config<easylocal::runner::first_improvement>()
        .max_evaluations = 100;
    application
        .runner_config<easylocal::runner::best_improvement>()
        .max_evaluations = 100;

    return application;
}

struct BrokenEqualityInput
{
};

struct BrokenEqualitySolution
{
    int value{};

    friend auto operator==(
        const BrokenEqualitySolution&,
        const BrokenEqualitySolution&) noexcept -> bool
    {
        return false;
    }
};

struct BrokenEqualityMove
{
};

class BrokenEqualitySolutionManager
    : public easylocal::solution_manager_base<
          BrokenEqualityInput,
          BrokenEqualitySolution>
{
public:
    using solution_manager_base::solution_manager_base;
    using cost_type = int;

    [[nodiscard]] static auto is_valid(const BrokenEqualitySolution&) noexcept
        -> bool
    {
        return true;
    }

    [[nodiscard]] static auto evaluate(const BrokenEqualitySolution& solution) noexcept
        -> cost_type
    {
        return solution.value;
    }

    [[nodiscard]] static auto initial_solution() noexcept -> BrokenEqualitySolution
    {
        return {};
    }
};

class BrokenEqualityNeighborhood
    : public easylocal::neighborhood_explorer_base<
          BrokenEqualitySolutionManager,
          BrokenEqualityMove>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]] static auto is_valid(
        const BrokenEqualitySolution&,
        const BrokenEqualityMove&) noexcept -> bool
    {
        return true;
    }

    [[nodiscard]] static auto first_move(
        const BrokenEqualitySolution&,
        BrokenEqualityMove&) noexcept -> bool
    {
        return false;
    }

    [[nodiscard]] static auto next_move(
        const BrokenEqualitySolution&,
        BrokenEqualityMove&) noexcept -> bool
    {
        return false;
    }

    static void make_move(BrokenEqualitySolution&, const BrokenEqualityMove&) noexcept
    {
    }
};

class BrokenNeighborhoodExplorer
{
public:
    using instance_type = AssignmentInstance;
    using solution_type = AssignmentSolution;
    using move_type = ReassignJobMove;

    explicit BrokenNeighborhoodExplorer(AssignmentSolutionManager& manager)
        : manager_{manager}
    {
    }

    [[nodiscard]] auto instance() const noexcept -> const instance_type&
    {
        return manager_.instance();
    }

    [[nodiscard]] static auto is_valid(
        const solution_type&,
        const move_type&) noexcept -> bool
    {
        return true;
    }

    [[nodiscard]] static auto first_move(
        const solution_type& solution,
        move_type& move) noexcept -> bool
    {
        if (solution.assignment.empty())
            return false;
        move = move_type{.job = 0, .destination = 0};
        return true;
    }

    [[nodiscard]] static auto next_move(
        const solution_type&,
        move_type&) noexcept -> bool
    {
        return false;
    }

    static void make_move(solution_type& solution, const move_type&)
    {
        solution.assignment[0] = solution.assignment.size() + 100;
    }

private:
    AssignmentSolutionManager& manager_;
};

struct ProbeInput
{
};

struct ProbeSolution
{
    int value{};
    auto operator==(const ProbeSolution&) const -> bool = default;
};

struct ProbeMove
{
    int value{};
    auto operator==(const ProbeMove&) const -> bool = default;
};

class ProbeSolutionManager
    : public easylocal::solution_manager_base<ProbeInput, ProbeSolution>
{
public:
    using solution_manager_base::solution_manager_base;
    using cost_type = int;

    [[nodiscard]] static auto is_valid(const ProbeSolution&) noexcept -> bool
    {
        return true;
    }

    [[nodiscard]] static auto evaluate(const ProbeSolution& solution) noexcept
        -> cost_type
    {
        return solution.value;
    }

    [[nodiscard]] static auto initial_solution() noexcept -> ProbeSolution
    {
        return {};
    }
};

class NondeterministicNeighborhood
    : public easylocal::neighborhood_explorer_base<ProbeSolutionManager, ProbeMove>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]] static auto is_valid(const ProbeSolution&, const ProbeMove&) noexcept
        -> bool
    {
        return true;
    }

    [[nodiscard]] static auto first_move(const ProbeSolution&, ProbeMove& move) noexcept
        -> bool
    {
        move = ProbeMove{1};
        return true;
    }

    [[nodiscard]] static auto next_move(const ProbeSolution&, ProbeMove&) noexcept
        -> bool
    {
        return false;
    }

    void make_move(ProbeSolution& solution, const ProbeMove&) const noexcept
    {
        solution.value += flip_ ? 1 : 2;
        flip_ = !flip_;
    }

private:
    mutable bool flip_{};
};

class RandomOutsideEnumerationNeighborhood
    : public easylocal::neighborhood_explorer_base<ProbeSolutionManager, ProbeMove>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]] static auto is_valid(const ProbeSolution&, const ProbeMove& move) noexcept
        -> bool
    {
        return move.value == 1 || move.value == 2;
    }

    [[nodiscard]] static auto first_move(const ProbeSolution&, ProbeMove& move) noexcept
        -> bool
    {
        move = ProbeMove{1};
        return true;
    }

    [[nodiscard]] static auto next_move(const ProbeSolution&, ProbeMove&) noexcept
        -> bool
    {
        return false;
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const ProbeSolution&, RNG&)
        -> std::optional<ProbeMove>
    {
        return ProbeMove{2};
    }

    static void make_move(ProbeSolution& solution, const ProbeMove& move) noexcept
    {
        solution.value += move.value;
    }
};

void real_app_graph_is_checked_with_full_coverage()
{
    const AssignmentInstance instance{
        .demand = {4, 4, 2},
        .capacity = {5, 5},
    };

    auto application = make_application();
    const auto report = easylocal::check(application, instance);

    assert(report.passed());
    assert(report.checks() > 0);
    assert(report.coverage().solution_managers == 1);
    assert(report.coverage().cost_components == 2);
    assert(report.coverage().neighborhood_graphs == 1);
    assert(report.coverage().delta_bindings == 1);
    assert(report.coverage().runner_registrations == 2);
    assert(report.coverage().deterministic_moves_checked > 0);
    assert(report.coverage().random_move_samples == 16);
}

void check_fails_on_a_broken_realized_graph()
{
    auto sm =
        easylocal::solution_manager<AssignmentSolutionManager>()
        | easylocal::component<CapacityCostComponent>()
        | easylocal::aggregator([](const CapacityValue& capacity) {
              return AssignmentCostAggregator{}.hard(capacity);
          });

    auto application =
        easylocal::app("broken-assignment")
            .solution_manager(std::move(sm))
            .neighborhood<BrokenNeighborhoodExplorer>()
            .runner<easylocal::runner::first_improvement>("fi");

    application
        .runner_config<easylocal::runner::first_improvement>()
        .max_evaluations = 10;

    const AssignmentInstance instance{
        .demand = {1},
        .capacity = {2},
    };
    const AssignmentSolution solution{
        .assignment = {0},
    };

    const auto report = easylocal::check(application, instance, solution);
    assert(!report.passed());

    bool attributed_to_move_application = false;
    for (const auto& failure : report.failures())
    {
        if (failure.check == "neighborhood move application")
        {
            attributed_to_move_application = true;
        }
    }
    assert(attributed_to_move_application);
}

void check_validates_solution_equality_when_available()
{
    auto application =
        easylocal::app("broken-equality")
            .solution_manager<BrokenEqualitySolutionManager>()
            .neighborhood<BrokenEqualityNeighborhood>()
            .runner<easylocal::runner::first_improvement>("fi");
    application
        .runner_config<easylocal::runner::first_improvement>()
        .max_evaluations = 1;

    const BrokenEqualityInput input{};
    const BrokenEqualitySolution solution{};
    const auto report = easylocal::check(application, input, solution);

    assert(!report.passed());
    bool found_equality_failure = false;
    for (const auto& failure : report.failures())
    {
        if (failure.check == "solution equality reflexivity")
        {
            found_equality_failure = true;
        }
    }
    assert(found_equality_failure);
}

void check_detects_nondeterministic_move_application()
{
    auto application =
        easylocal::app("nondeterministic-move")
            .solution_manager<ProbeSolutionManager>()
            .neighborhood<NondeterministicNeighborhood>()
            .runner<easylocal::runner::first_improvement>("fi");
    application.runner_config<easylocal::runner::first_improvement>().max_evaluations = 1;

    const auto report = easylocal::check(application, ProbeInput{}, ProbeSolution{});
    assert(!report.passed());

    bool found = false;
    for (const auto& failure : report.failures())
        found = found || failure.check == "move determinism";
    assert(found);
}

void check_detects_random_moves_outside_the_enumerated_neighborhood()
{
    auto application =
        easylocal::app("random-membership")
            .solution_manager<ProbeSolutionManager>()
            .neighborhood<RandomOutsideEnumerationNeighborhood>()
            .runner<easylocal::runner::first_improvement>("fi");
    application.runner_config<easylocal::runner::first_improvement>().max_evaluations = 1;

    const auto report = easylocal::check(application, ProbeInput{}, ProbeSolution{});
    assert(!report.passed());

    bool found = false;
    for (const auto& failure : report.failures())
        found = found || failure.check == "random proposal membership";
    assert(found);
}

} // namespace

int main()
{
    real_app_graph_is_checked_with_full_coverage();
    check_fails_on_a_broken_realized_graph();
    check_validates_solution_equality_when_available();
    check_detects_nondeterministic_move_application();
    check_detects_random_moves_outside_the_enumerated_neighborhood();
    return 0;
}
