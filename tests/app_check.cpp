#include "../examples/assignment/cost.hpp"
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/instance.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"
#include "support/assignment_capacity_delta.hpp"

#include <easylocal/app/check.hpp>
#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/hill_climbing.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#include <cassert>
#include <cstddef>

namespace
{

using namespace assignment;

[[nodiscard]] auto make_application()
{
    auto sm = easylocal::solution_manager<AssignmentSolutionManager>()
        | assignment::assignment_cost();

    auto nhe =
        easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::delta<
              CapacityCostComponent,
              ReassignCapacityDeltaEvaluator>();

    auto application =
        easylocal::app("assignment")
            .with_solution_manager(std::move(sm))
            .with_neighborhood(std::move(nhe))
            .with_runner<easylocal::runners::FirstImprovement>("fi")
            .with_runner<easylocal::runners::BestImprovement>("bi");

    application.runner_parameters<easylocal::runners::FirstImprovement>("fi")
        .max_evaluations = 100;
    application.runner_parameters<easylocal::runners::BestImprovement>("bi")
        .max_evaluations = 100;

    return application;
}

struct BrokenEqualityInput
{
};

struct BrokenEqualitySolution
{
    int value{};

    [[maybe_unused]] friend auto operator==(
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

    [[nodiscard]] static auto is_valid(const BrokenEqualitySolution&) noexcept
        -> bool
    {
        return true;
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
    using input_type = AssignmentInstance;
    using solution_type = AssignmentSolution;
    using move_type = ReassignJobMove;

    explicit BrokenNeighborhoodExplorer(AssignmentSolutionManager& manager)
        : manager_{manager}
    {
    }

    [[nodiscard]] auto input() const noexcept -> const input_type&
    {
        return manager_.input();
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

// A neighborhood with a parameter that declares no domain.
struct ProbeParameters
{
    int probes{1};

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"probes", &ProbeParameters::probes>("Probes"));
    }

    [[nodiscard]] easylocal::config::validation_result validate() const
    {
        return easylocal::config::validation_result::success();
    }
};

class ProbingNeighborhoodExplorer : public ReassignJobNeighborhoodExplorer
{
public:
    using parameters_type = ProbeParameters;

    ProbingNeighborhoodExplorer(
        const AssignmentSolutionManager& manager,
        const ProbeParameters&)
        : ReassignJobNeighborhoodExplorer{manager}
    {
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
}

void check_fails_on_a_broken_realized_graph()
{
    auto sm = easylocal::solution_manager<AssignmentSolutionManager>()
        | easylocal::cost::apply(
            assignment::CapacityHardCost{},
            easylocal::component<CapacityCostComponent>());

    auto application =
        easylocal::app("broken-assignment")
            .with_solution_manager(std::move(sm))
            .with_neighborhood(easylocal::neighborhood<BrokenNeighborhoodExplorer>())
            .with_runner<easylocal::runners::FirstImprovement>("fi");

    application.runner_parameters<easylocal::runners::FirstImprovement>("fi")
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

void check_fails_on_a_parameter_without_a_domain()
{
    auto application =
        easylocal::app("probing-assignment")
            .with_solution_manager(
                easylocal::solution_manager<AssignmentSolutionManager>()
                | assignment::assignment_cost())
            .with_neighborhood(easylocal::neighborhood<ProbingNeighborhoodExplorer>())
            .with_runner<easylocal::runners::FirstImprovement>("fi");

    const AssignmentInstance instance{
        .demand = {4, 4, 2},
        .capacity = {5, 5},
    };
    const auto report = easylocal::check(application, instance);
    assert(!report.passed());
    bool reported = false;
    for (const auto& failure : report.failures())
    {
        reported = reported
            || (failure.check == "parameter domain"
                && failure.message.starts_with("neighborhood.probes declares no domain"));
    }
    assert(reported);
}

void check_names_the_runner_with_invalid_parameters()
{
    auto application =
        easylocal::app("assignment")
            .with_solution_manager(
                easylocal::solution_manager<AssignmentSolutionManager>()
                | assignment::assignment_cost())
            .with_neighborhood(
                easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
                | easylocal::delta<
                    CapacityCostComponent,
                    ReassignCapacityDeltaEvaluator>())
            .with_runner<easylocal::runners::HillClimbing>("climb");
    application.runner_parameters<easylocal::runners::HillClimbing>("climb")
        .max_idle_iterations = 0;

    const AssignmentInstance instance{
        .demand = {4, 4, 2},
        .capacity = {5, 5},
    };
    const auto report = easylocal::check(application, instance);
    assert(!report.passed());
    bool reported = false;
    for (const auto& failure : report.failures())
    {
        reported = reported
            || (failure.check == "runner configuration"
                && failure.message.starts_with("runner climb: "));
    }
    assert(reported);
}

void check_reports_an_invalid_nested_group()
{
    using Annealing = easylocal::runners::SimulatedAnnealing<
        easylocal::runners::temperature::FixedLength>;
    auto application =
        easylocal::app("assignment")
            .with_solution_manager(
                easylocal::solution_manager<AssignmentSolutionManager>()
                | assignment::assignment_cost())
            .with_neighborhood(
                easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
                | easylocal::delta<
                    CapacityCostComponent,
                    ReassignCapacityDeltaEvaluator>())
            .with_runner<Annealing>("anneal");
    // An invalid temperature schedule, a group of the runner's parameters.
    application.runner_parameters<Annealing>("anneal").temperature.cooling_rate = 2.0;

    const AssignmentInstance instance{
        .demand = {4, 4, 2},
        .capacity = {5, 5},
    };
    const auto report = easylocal::check(application, instance);
    assert(!report.passed());
    bool reported = false;
    for (const auto& failure : report.failures())
    {
        reported = reported
            || (failure.check == "runner configuration"
                && failure.message.starts_with("runner anneal: "));
    }
    assert(reported);
}

} // namespace

int main()
{
    real_app_graph_is_checked_with_full_coverage();
    check_fails_on_a_broken_realized_graph();
    check_fails_on_a_parameter_without_a_domain();
    check_names_the_runner_with_invalid_parameters();
    check_reports_an_invalid_nested_group();
    return 0;
}
