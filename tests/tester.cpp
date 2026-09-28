#include "../examples/assignment/capacity_delta.hpp"
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"

#include <easylocal/app.hpp>
#include <easylocal/search/first_improvement.hpp>
#include <easylocal/tester.hpp>

#include <cassert>
#include <concepts>
#include <cstdint>
#include <random>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{

using namespace easylocal::mwe::assignment;


struct RandomOnlyInput
{
};

struct RandomOnlySolution
{
    std::uint64_t value{};
};

struct RandomOnlyMove
{
};

class RandomOnlySolutionManager
    : public easylocal::solution_manager_base<RandomOnlyInput, RandomOnlySolution>
{
public:
    using solution_manager_base::solution_manager_base;
    using cost_type = std::uint64_t;

    [[nodiscard]]
    static auto is_valid(const RandomOnlySolution&) noexcept -> bool
    {
        return true;
    }

    [[nodiscard]]
    static auto evaluate(const RandomOnlySolution& solution) noexcept -> cost_type
    {
        return solution.value;
    }

    template<class RNG>
    [[nodiscard]]
    auto random_solution(RNG& rng) const -> RandomOnlySolution
    {
        return {rng()};
    }
};

class RandomOnlyNeighborhood
    : public easylocal::neighborhood_explorer_base<
          RandomOnlySolutionManager,
          RandomOnlyMove>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]]
    static auto is_valid(
        const RandomOnlySolution&,
        const RandomOnlyMove&) noexcept -> bool
    {
        return true;
    }

    static void make_move(RandomOnlySolution&, const RandomOnlyMove&) noexcept
    {
    }
};

[[nodiscard]]
auto make_random_only_application()
{
    auto application = easylocal::app("random-only")
        .solution_manager<RandomOnlySolutionManager>()
        .neighborhood<RandomOnlyNeighborhood>()
        .runner<easylocal::runner::first_improvement>("fi");

    application
        .runner_config<easylocal::runner::first_improvement>()
        .max_evaluations = 100;

    return application;
}

[[nodiscard]]
auto make_application()
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

    auto application = easylocal::app("assignment")
        .solution_manager(std::move(sm))
        .neighborhood(std::move(nhe))
        .runner<easylocal::runner::first_improvement>("fi");

    application
        .runner_config<easylocal::runner::first_improvement>()
        .max_evaluations = 100;

    return application;
}

[[nodiscard]]
auto make_multi_runner_application()
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

    auto application = easylocal::app("assignment-multi-runner")
        .solution_manager(std::move(sm))
        .neighborhood(std::move(nhe))
        .runner<easylocal::runner::first_improvement>("quick")
        .runner<easylocal::runner::first_improvement>("deep");

    application
        .runner_config<easylocal::runner::first_improvement>("quick")
        .max_evaluations = 1;
    application
        .runner_config<easylocal::runner::first_improvement>("deep")
        .max_evaluations = 100;

    return application;
}

using app_type = decltype(make_application());

static_assert(std::copy_constructible<app_type>);
static_assert(std::is_copy_assignable_v<app_type>);
static_assert(std::move_constructible<app_type>);
static_assert(std::is_move_assignable_v<app_type>);
static_assert(std::constructible_from<easylocal::Tester<app_type>, app_type>);
static_assert(std::same_as<
    decltype(std::declval<easylocal::Tester<app_type>&>().input()),
    const AssignmentInstance&>);


[[nodiscard]]
auto make_input(const quantity_type demand) -> AssignmentInstance
{
    return AssignmentInstance{
        .demand = {demand, demand + 1},
        .capacity = {10, 20},
    };
}

void tester_owns_input_and_builds_instance_from_it()
{
    easylocal::Tester tester{make_application()};

    assert(!tester.has_input());

    tester.set_input(make_input(3));

    assert(tester.has_input());
    assert(tester.input().demand[0] == 3);
    assert(std::addressof(tester.instance().instance()) == std::addressof(tester.input()));
}

void replacing_input_rebuilds_the_app_instance()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(3));

    tester.set_input(make_input(7));

    assert(tester.input().demand[0] == 7);
    assert(tester.instance().instance().demand[0] == 7);
    assert(std::addressof(tester.instance().instance()) == std::addressof(tester.input()));
}

void tester_exposes_initial_solution_as_an_explicit_choice()
{
    easylocal::Tester tester{make_application()};

    static_assert(decltype(tester)::supports_initial_solution);
    static_assert(!decltype(tester)::supports_random_solution);

    tester.set_input(make_input(3));
    assert(!tester.has_solution());

    tester.use_initial_solution();

    assert(tester.has_solution());
    assert(tester.solution().assignment.size() == tester.input().demand.size());
    assert(tester.solution().assignment[0] == 0);
    assert(tester.solution().assignment[1] == 1);
}

void tester_exposes_random_solution_as_an_explicit_choice()
{
    easylocal::Tester tester{make_random_only_application()};

    static_assert(!decltype(tester)::supports_initial_solution);
    static_assert(decltype(tester)::supports_random_solution);

    tester.set_input(RandomOnlyInput{});
    std::mt19937_64 rng{1234};
    std::mt19937_64 reference{1234};

    tester.use_random_solution(rng);

    assert(tester.has_solution());
    assert(tester.solution().value == reference());
}

void tester_exposes_deterministic_and_random_move_capabilities()
{
    easylocal::Tester tester{make_application()};

    static_assert(decltype(tester)::supports_deterministic_moves);
    static_assert(decltype(tester)::supports_random_moves);
}

void tester_selects_first_and_next_moves_deterministically()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(3));
    tester.use_initial_solution();

    assert(!tester.has_move());
    const auto has_first = tester.use_first_move();
    assert(has_first);
    assert(tester.has_move());
    assert(tester.move().job == 0);
    assert(tester.move().destination == 1);
    assert(tester.move_is_valid());

    const auto has_next = tester.use_next_move();
    assert(has_next);
    assert(tester.move().job == 1);
    assert(tester.move().destination == 0);
    assert(tester.move_is_valid());

    const auto has_third = tester.use_next_move();
    assert(!has_third);
    assert(tester.has_move());
    assert(tester.move().job == 1);
    assert(tester.move().destination == 0);
}

void tester_selects_first_improving_and_best_moves()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(3));
    tester.set_solution(AssignmentSolution{.assignment = {0, 0}});

    static_assert(decltype(tester)::supports_improvement_selection);
    assert(tester.evaluate().soft() == 7);

    const auto improving = tester.use_first_improving_move();
    assert(improving);
    assert(tester.has_move());
    assert(tester.move().job == 0);
    assert(tester.move().destination == 1);
    assert(tester.evaluate_move().soft() == 1);

    const auto best = tester.use_best_move();
    assert(best);
    assert(tester.has_move());
    assert(tester.evaluate_move().soft() == 1);
}

void tester_selects_random_moves_with_an_explicit_rng()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(3));
    tester.use_initial_solution();

    std::mt19937_64 rng{1234};
    std::mt19937_64 reference_rng{1234};
    const auto expected = easylocal::random_move(
        tester.instance().neighborhood(),
        tester.solution(),
        reference_rng);

    assert(expected);
    const auto selected = tester.use_random_move(rng);
    assert(selected);
    assert(tester.has_move());
    assert(tester.move().job == expected->job);
    assert(tester.move().destination == expected->destination);
    assert(tester.move_is_valid());
}

void tester_compares_move_evaluation_with_full_recomputation()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(3));
    tester.use_initial_solution();
    const auto selected = tester.use_first_move();
    assert(selected);

    const auto incremental = tester.evaluate_move();
    const auto full = tester.evaluate_move_fully();

    assert(incremental.hard().template get<0>() == 0);
    assert(incremental.hard().template get<1>() == 0);
    assert(incremental.soft() == 7);
    assert(full.hard().template get<0>() == 0);
    assert(full.hard().template get<1>() == 0);
    assert(full.soft() == 7);
    assert(tester.move_evaluation_matches_full());
}

void applying_a_move_updates_the_solution_and_clears_move_state()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(3));
    tester.use_initial_solution();
    const auto selected = tester.use_first_move();
    assert(selected);

    tester.apply_move();

    assert(tester.solution().assignment[0] == 1);
    assert(tester.solution().assignment[1] == 1);
    assert(!tester.has_move());
    assert(tester.evaluate().soft() == 7);
}

void replacing_the_solution_clears_move_state()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(3));
    tester.use_initial_solution();
    const auto selected = tester.use_first_move();
    assert(selected);
    assert(tester.has_move());

    tester.set_solution(AssignmentSolution{.assignment = {0, 0}});

    assert(!tester.has_move());
}

void tester_can_inspect_an_explicit_invalid_move()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(3));
    tester.use_initial_solution();

    tester.set_move(ReassignJobMove{.job = 0, .destination = 0});

    assert(tester.has_move());
    assert(!tester.move_is_valid());
}

void tester_reports_when_the_current_solution_has_no_moves()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(AssignmentInstance{
        .demand = {},
        .capacity = {10, 20},
    });
    tester.use_initial_solution();

    const auto has_first = tester.use_first_move();
    assert(!has_first);
    assert(!tester.has_move());

    std::mt19937_64 rng{1234};
    const auto has_random = tester.use_random_move(rng);
    assert(!has_random);
    assert(!tester.has_move());
}


void tester_accepts_and_validates_an_explicit_solution()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(3));

    tester.set_solution(AssignmentSolution{.assignment = {0, 0}});

    assert(tester.has_solution());
    assert(tester.is_valid());
    assert(tester.solution().assignment[0] == 0);
    assert(tester.solution().assignment[1] == 0);

    tester.set_solution(AssignmentSolution{.assignment = {0, 2}});
    assert(!tester.is_valid());
}

void tester_evaluates_the_current_solution()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(3));
    tester.use_initial_solution();

    const auto cost = tester.evaluate();

    assert(cost.hard().template get<0>() == 0);
    assert(cost.hard().template get<1>() == 0);
    assert(cost.soft() == 1);
}

void tester_runs_app_check_on_the_current_solution()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(3));
    tester.use_initial_solution();

    const auto report = tester.check();

    assert(report.passed());
    assert(report.checks() > 0);
    assert(report.coverage().solution_managers == 1);
    assert(report.coverage().neighborhood_graphs == 1);
    assert(report.coverage().runner_registrations == 1);
}

void tester_check_reports_an_invalid_current_solution()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(3));
    tester.set_solution(AssignmentSolution{.assignment = {0, 2}});

    const auto report = tester.check();

    assert(!report.passed());
    assert(report.failures().size() == 1);
    assert(report.failures().front().check == "check solution");
}

void replacing_input_clears_the_current_solution()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(3));
    tester.use_initial_solution();
    assert(tester.has_solution());

    tester.set_input(make_input(7));

    assert(!tester.has_solution());
}

void tester_lists_registered_runners_in_app_order()
{
    easylocal::Tester tester{make_multi_runner_application()};

    const auto names = tester.runner_names();

    assert(names.size() == 2);
    assert(names[0] == std::string_view{"quick"});
    assert(names[1] == std::string_view{"deep"});
}

void tester_runs_a_named_runner_on_the_current_solution()
{
    easylocal::Tester tester{make_multi_runner_application()};
    tester.set_input(make_input(3));
    tester.set_solution(AssignmentSolution{.assignment = {0, 0}});

    const auto before = tester.evaluate();
    assert(before.soft() == 7);

    const auto ran = tester.run_runner("deep");

    assert(ran);
    assert(tester.has_solution());
    assert(tester.is_valid());
    assert(tester.evaluate().soft() == 1);
    assert(tester.solution().assignment[0] == 1);
    assert(tester.solution().assignment[1] == 0);
}

void tester_distinguishes_same_tag_runners_by_name()
{
    easylocal::Tester tester{make_multi_runner_application()};
    tester.set_input(make_input(3));
    tester.set_solution(AssignmentSolution{.assignment = {0, 0}});

    const auto ran_quick = tester.run_runner("quick");

    assert(ran_quick);
    assert(tester.evaluate().soft() == 7);

    const auto ran_deep = tester.run_runner("deep");

    assert(ran_deep);
    assert(tester.evaluate().soft() == 1);
}

void tester_reports_unknown_runner_without_changing_solution()
{
    easylocal::Tester tester{make_multi_runner_application()};
    tester.set_input(make_input(3));
    tester.set_solution(AssignmentSolution{.assignment = {0, 0}});

    const auto ran = tester.run_runner("missing");

    assert(!ran);
    assert(tester.evaluate().soft() == 7);
}

void app_copy_preserves_graph_configuration()
{
    auto application = make_application();
    application
        .runner_config<easylocal::runner::first_improvement>()
        .max_evaluations = 17;

    const auto copy = application;

    assert(copy.name() == std::string_view{"assignment"});
    assert(
        copy.runner_name<easylocal::runner::first_improvement>() ==
        std::string_view{"fi"});
    assert(
        copy.runner_config<easylocal::runner::first_improvement>()
            .max_evaluations == 17);
}

void tester_can_copy_an_lvalue_app()
{
    auto application = make_application();
    easylocal::Tester tester{application};

    assert(tester.app().name() == std::string_view{"assignment"});
    assert(application.name() == std::string_view{"assignment"});

    tester
        .app()
        .runner_config<easylocal::runner::first_improvement>()
        .max_evaluations = 3;

    assert(
        application
            .runner_config<easylocal::runner::first_improvement>()
            .max_evaluations != 3);
}

void tester_can_take_ownership_of_an_rvalue_app()
{
    auto application = make_application();
    application
        .runner_config<easylocal::runner::first_improvement>()
        .max_evaluations = 29;

    easylocal::Tester tester{std::move(application)};

    assert(tester.app().name() == std::string_view{"assignment"});
    assert(
        tester
            .app()
            .runner_config<easylocal::runner::first_improvement>()
            .max_evaluations == 29);
}

} // namespace


void tester_reports_neighborhood_diagnostics()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(3));
    tester.set_solution(AssignmentSolution{.assignment = {0, 0}});

    static_assert(decltype(tester)::supports_cost_consistency_check);
    static_assert(decltype(tester)::supports_move_independence_check);
    static_assert(decltype(tester)::supports_random_distribution_check);

    const auto preview = tester.neighborhood_preview(1);
    assert(preview.moves == 2);
    assert(preview.entries.size() == 1);

    const auto statistics = tester.neighborhood_statistics();
    assert(statistics.moves == 2);
    assert(statistics.invalid == 0);
    assert(statistics.improving + statistics.sideways + statistics.worsening == 2);

    const auto costs = tester.check_neighborhood_costs();
    assert(costs.moves == 2);
    assert(costs.invalid == 0);
    assert(costs.mismatches == 0);

    const auto independence = tester.check_move_independence();
    assert(independence.moves == 2);
    assert(independence.invalid == 0);
    assert(independence.null_moves == 0);
    assert(independence.repeated_states == 0);

    std::mt19937_64 rng{1234};
    const auto distribution = tester.check_random_move_distribution(rng, 8);
    assert(distribution.neighborhood_size == 2);
    assert(distribution.samples == 16);
    assert(distribution.out_of_neighborhood == 0);
    assert(distribution.unseen == 0);
    assert(distribution.min_frequency <= distribution.max_frequency);
}

int main()
{
    app_copy_preserves_graph_configuration();
    tester_can_copy_an_lvalue_app();
    tester_can_take_ownership_of_an_rvalue_app();
    tester_owns_input_and_builds_instance_from_it();
    replacing_input_rebuilds_the_app_instance();
    tester_exposes_initial_solution_as_an_explicit_choice();
    tester_exposes_random_solution_as_an_explicit_choice();
    tester_exposes_deterministic_and_random_move_capabilities();
    tester_selects_first_and_next_moves_deterministically();
    tester_selects_first_improving_and_best_moves();
    tester_selects_random_moves_with_an_explicit_rng();
    tester_compares_move_evaluation_with_full_recomputation();
    tester_reports_neighborhood_diagnostics();
    applying_a_move_updates_the_solution_and_clears_move_state();
    replacing_the_solution_clears_move_state();
    tester_can_inspect_an_explicit_invalid_move();
    tester_reports_when_the_current_solution_has_no_moves();
    tester_accepts_and_validates_an_explicit_solution();
    tester_evaluates_the_current_solution();
    tester_runs_app_check_on_the_current_solution();
    tester_check_reports_an_invalid_current_solution();
    replacing_input_clears_the_current_solution();
    tester_lists_registered_runners_in_app_order();
    tester_runs_a_named_runner_on_the_current_solution();
    tester_distinguishes_same_tag_runners_by_name();
    tester_reports_unknown_runner_without_changing_solution();
}
