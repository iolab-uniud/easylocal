#include "../examples/assignment/cost.hpp"
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"
#include "support/assignment_capacity_delta.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/utils/generator.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <stdexcept>
#include <stop_token>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{

using namespace assignment;

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

struct RandomOnlyValue
{
    [[nodiscard]]
    static auto evaluate(const RandomOnlySolution& solution) noexcept -> std::int64_t
    {
        return static_cast<std::int64_t>(solution.value);
    }
};

class RandomOnlySolutionManager
    : public easylocal::solution_manager_base<RandomOnlyInput, RandomOnlySolution>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]]
    static auto is_valid(const RandomOnlySolution&) noexcept -> bool
    {
        return true;
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
    static auto is_valid(const RandomOnlySolution&, const RandomOnlyMove&) noexcept
        -> bool
    {
        return true;
    }

    static void make_move(RandomOnlySolution&, const RandomOnlyMove&) noexcept {}
};

// A value lowered by steps, whose cost counts its full evaluations.
struct CountedInput
{
};

struct CountedSolution
{
    std::uint64_t value{};
};

struct CountedStep
{
    std::uint64_t step{};
};

struct CountedValue
{
    static inline std::size_t evaluations = 0;

    [[nodiscard]]
    static std::int64_t evaluate(const CountedSolution& solution) noexcept
    {
        ++evaluations;
        return static_cast<std::int64_t>(solution.value);
    }
};

class CountedSolutionManager
    : public easylocal::solution_manager_base<CountedInput, CountedSolution>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]]
    static bool is_valid(const CountedSolution&) noexcept
    {
        return true;
    }

    [[nodiscard]]
    static CountedSolution initial_solution() noexcept
    {
        return {.value = 10};
    }
};

class CountedNeighborhood
    : public easylocal::neighborhood_explorer_base<CountedSolutionManager, CountedStep>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]]
    static easylocal::generator<CountedStep> moves(const CountedSolution&)
    {
        for (std::uint64_t step = 1; step <= 3; ++step)
            co_yield CountedStep{.step = step};
    }

    [[nodiscard]]
    static bool is_valid(
        const CountedSolution& solution,
        const CountedStep& move) noexcept
    {
        return move.step <= solution.value;
    }

    static void make_move(CountedSolution& solution, const CountedStep& move) noexcept
    {
        solution.value -= move.step;
    }
};

[[nodiscard]]
auto make_counted_application()
{
    return easylocal::app("counted")
        .with_solution_manager(
            easylocal::solution_manager<CountedSolutionManager>()
            | easylocal::component<CountedValue>())
        .with_neighborhood(easylocal::neighborhood<CountedNeighborhood>())
        .with_runner<easylocal::runners::FirstImprovement>("fi");
}

[[nodiscard]]
auto make_random_only_application()
{
    auto application =
        easylocal::app("random-only")
            .with_solution_manager(
                easylocal::solution_manager<RandomOnlySolutionManager>()
                | easylocal::component<RandomOnlyValue>())
            .with_neighborhood(easylocal::neighborhood<RandomOnlyNeighborhood>())
            .with_runner<easylocal::runners::FirstImprovement>("fi");

    application.runner_config<easylocal::runners::FirstImprovement>().max_evaluations =
        100;

    return application;
}

[[nodiscard]]
auto make_application()
{
    auto sm = easylocal::solution_manager<AssignmentSolutionManager>()
        | assignment::assignment_cost();

    auto nhe = easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::delta<CapacityCostComponent, ReassignCapacityDeltaEvaluator>();

    auto application =
        easylocal::app("assignment")
            .with_solution_manager(std::move(sm))
            .with_neighborhood(std::move(nhe))
            .with_runner<easylocal::runners::FirstImprovement>("fi");

    application.runner_config<easylocal::runners::FirstImprovement>().max_evaluations =
        100;

    return application;
}

[[nodiscard]]
auto make_multi_runner_application()
{
    auto sm = easylocal::solution_manager<AssignmentSolutionManager>()
        | assignment::assignment_cost();

    auto nhe = easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::delta<CapacityCostComponent, ReassignCapacityDeltaEvaluator>();

    auto application =
        easylocal::app("assignment-multi-runner")
            .with_solution_manager(std::move(sm))
            .with_neighborhood(std::move(nhe))
            .with_runner<easylocal::runners::FirstImprovement>("quick")
            .with_runner<easylocal::runners::FirstImprovement>("deep");

    application.runner_config<easylocal::runners::FirstImprovement>("quick")
        .max_evaluations = 1;
    application.runner_config<easylocal::runners::FirstImprovement>("deep")
        .max_evaluations = 100;

    return application;
}

using app_type = decltype(make_application());

static_assert(std::copy_constructible<app_type>);
static_assert(std::is_copy_assignable_v<app_type>);
static_assert(std::move_constructible<app_type>);
static_assert(std::is_move_assignable_v<app_type>);
static_assert(std::constructible_from<easylocal::Session<app_type>, app_type>);
// A const session gives its app and bound app read-only.
static_assert(std::is_const_v<std::remove_reference_t<
        decltype(std::declval<const easylocal::Session<app_type>&>().app())>>);
static_assert(std::is_const_v<std::remove_reference_t<
        decltype(std::declval<const easylocal::Session<app_type>&>().bound_app())>>);
static_assert(!std::is_const_v<std::remove_reference_t<
        decltype(std::declval<easylocal::Session<app_type>&>().bound_app())>>);
static_assert(std::same_as<
    decltype(std::declval<easylocal::Session<app_type>&>().input()),
    const AssignmentInstance&>);

[[nodiscard]]
auto make_input(const quantity_type demand) -> AssignmentInstance
{
    return AssignmentInstance{
        .demand = {demand, demand + 1},
        .capacity = {10, 20},
    };
}

void session_takes_its_input_and_seed_at_construction()
{
    easylocal::Session session{make_application(), make_input(4), 7};

    assert(session.has_input());
    assert(session.input().demand[0] == 4);
    assert(
        std::addressof(session.bound_app().input()) == std::addressof(session.input()));

    // The seed is the RNG's: the same seed draws the same numbers.
    std::mt19937_64 expected{7};
    assert(session.rng()() == expected());
}

void session_shares_an_input_it_does_not_copy()
{
    const auto input = std::make_shared<const AssignmentInstance>(make_input(5));
    easylocal::Session session{make_application(), input, 1};

    assert(std::addressof(session.input()) == input.get());
    assert(session.input_handle() == input);
}

void session_runs_pass_options_to_the_runner()
{
    easylocal::Session session{make_multi_runner_application(), make_input(3), 1};
    session.use_initial_solution();
    const auto initial = session.solution();

    // A run stopped before it starts keeps the current solution.
    std::stop_source stop;
    stop.request_stop();
    const easylocal::run_control control{stop.get_token()};
    const auto ran = session.run("deep", easylocal::with(control));
    assert(ran);
    assert(session.solution() == initial);
}

void session_owns_input_and_builds_instance_from_it()
{
    easylocal::Session session{make_application()};

    assert(!session.has_input());

    session.set_input(make_input(3));

    assert(session.has_input());
    assert(session.input().demand[0] == 3);
    const auto input_handle = session.input_handle();
    assert(input_handle);
    assert(input_handle.get() == std::addressof(session.input()));
    assert(
        std::addressof(session.bound_app().input()) == std::addressof(session.input()));
}

void replacing_input_rebuilds_the_bound_app()
{
    easylocal::Session session{make_application()};
    session.set_input(make_input(3));

    session.set_input(make_input(7));

    assert(session.input().demand[0] == 7);
    assert(session.bound_app().input().demand[0] == 7);
    assert(
        std::addressof(session.bound_app().input()) == std::addressof(session.input()));
}

void session_exposes_initial_solution_as_an_explicit_choice()
{
    easylocal::Session session{make_application()};

    static_assert(decltype(session)::supports_initial_solution);
    static_assert(!decltype(session)::supports_random_solution);

    session.set_input(make_input(3));
    assert(!session.has_solution());

    session.use_initial_solution();

    assert(session.has_solution());
    assert(session.solution().assignment.size() == session.input().demand.size());
    assert(session.solution().assignment[0] == 0);
    assert(session.solution().assignment[1] == 1);
}

void session_exposes_random_solution_as_an_explicit_choice()
{
    easylocal::Session session{make_random_only_application()};

    static_assert(!decltype(session)::supports_initial_solution);
    static_assert(decltype(session)::supports_random_solution);

    session.set_input(RandomOnlyInput{});
    std::mt19937_64 rng{1234};
    std::mt19937_64 reference{1234};

    session.use_random_solution(rng);

    assert(session.has_solution());
    assert(session.solution().value == reference());
}

void session_exposes_deterministic_and_random_move_capabilities()
{
    easylocal::Session session{make_application()};

    static_assert(decltype(session)::supports_deterministic_moves);
    static_assert(decltype(session)::supports_random_moves);
}

void session_selects_first_and_next_moves_deterministically()
{
    easylocal::Session session{make_application()};
    session.set_input(make_input(3));
    session.use_initial_solution();

    assert(!session.has_move());
    const auto has_first = session.use_first_move();
    assert(has_first);
    assert(session.has_move());
    assert(session.move().job == 0);
    assert(session.move().destination == 1);
    assert(session.move_is_valid());

    const auto has_next = session.use_next_move();
    assert(has_next);
    assert(session.move().job == 1);
    assert(session.move().destination == 0);
    assert(session.move_is_valid());

    const auto has_third = session.use_next_move();
    assert(!has_third);
    assert(session.has_move());
    assert(session.move().job == 1);
    assert(session.move().destination == 0);
}

void session_selects_first_improving_and_best_moves()
{
    easylocal::Session session{make_application()};
    session.set_input(make_input(3));
    session.set_solution(AssignmentSolution{.assignment = {0, 0}});

    static_assert(decltype(session)::supports_improvement_selection);
    assert(session.evaluate().soft() == 7);

    const auto improving = session.use_first_improving_move();
    assert(improving);
    assert(session.has_move());
    assert(session.move().job == 0);
    assert(session.move().destination == 1);
    assert(session.evaluate_move().soft() == 1);

    const auto best = session.use_best_move();
    assert(best);
    assert(session.has_move());
    assert(session.evaluate_move().soft() == 1);
}

void session_evaluates_the_current_solution_once_per_scan()
{
    easylocal::Session session{make_counted_application()};
    session.set_input(CountedInput{});
    session.use_initial_solution();

    CountedValue::evaluations = 0;
    const auto best = session.use_best_move();
    assert(best);
    assert(session.move().step == 3);
    // The current solution once, then each of the three candidates.
    assert(CountedValue::evaluations == 4);

    CountedValue::evaluations = 0;
    const auto improving = session.use_first_improving_move();
    assert(improving);
    assert(session.move().step == 1);
    assert(CountedValue::evaluations == 2);
}

void session_selects_random_moves_with_an_explicit_rng()
{
    easylocal::Session session{make_application()};
    session.set_input(make_input(3));
    session.use_initial_solution();

    std::mt19937_64 rng{1234};
    std::mt19937_64 reference_rng{1234};
    const auto expected = easylocal::random_move(
        session.bound_app().neighborhood(),
        session.solution(),
        reference_rng);

    assert(expected);
    const auto selected = session.use_random_move(rng);
    assert(selected);
    assert(session.has_move());
    assert(session.move().job == expected->job);
    assert(session.move().destination == expected->destination);
    assert(session.move_is_valid());
}

void session_compares_move_evaluation_with_full_recomputation()
{
    easylocal::Session session{make_application()};
    session.set_input(make_input(3));
    session.use_initial_solution();
    const auto selected = session.use_first_move();
    assert(selected);

    const auto incremental = session.evaluate_move();
    const auto full = session.evaluate_move_fully();

    assert(incremental.hard().template get<0>() == 0);
    assert(incremental.hard().template get<1>() == 0);
    assert(incremental.soft() == 7);
    assert(full.hard().template get<0>() == 0);
    assert(full.hard().template get<1>() == 0);
    assert(full.soft() == 7);
    assert(session.move_evaluation_matches_full());
}

void applying_a_move_updates_the_solution_and_clears_move_state()
{
    easylocal::Session session{make_application()};
    session.set_input(make_input(3));
    session.use_initial_solution();
    const auto selected = session.use_first_move();
    assert(selected);

    session.apply_move();

    assert(session.solution().assignment[0] == 1);
    assert(session.solution().assignment[1] == 1);
    assert(!session.has_move());
    assert(session.evaluate().soft() == 7);
}

void replacing_the_solution_clears_move_state()
{
    easylocal::Session session{make_application()};
    session.set_input(make_input(3));
    session.use_initial_solution();
    const auto selected = session.use_first_move();
    assert(selected);
    assert(session.has_move());

    session.set_solution(AssignmentSolution{.assignment = {0, 0}});

    assert(!session.has_move());
}

void session_can_inspect_an_explicit_invalid_move()
{
    easylocal::Session session{make_application()};
    session.set_input(make_input(3));
    session.use_initial_solution();

    session.set_move(ReassignJobMove{.job = 0, .destination = 0});

    assert(session.has_move());
    assert(!session.move_is_valid());
}

void session_reports_when_the_current_solution_has_no_moves()
{
    easylocal::Session session{make_application()};
    session.set_input(
        AssignmentInstance{
            .demand = {},
            .capacity = {10, 20},
        });
    session.use_initial_solution();

    const auto has_first = session.use_first_move();
    assert(!has_first);
    assert(!session.has_move());

    std::mt19937_64 rng{1234};
    const auto has_random = session.use_random_move(rng);
    assert(!has_random);
    assert(!session.has_move());
}

void session_accepts_and_validates_an_explicit_solution()
{
    easylocal::Session session{make_application()};
    session.set_input(make_input(3));

    session.set_solution(AssignmentSolution{.assignment = {0, 0}});

    assert(session.has_solution());
    assert(session.is_valid());
    assert(session.solution().assignment[0] == 0);
    assert(session.solution().assignment[1] == 0);

    session.set_solution(AssignmentSolution{.assignment = {0, 2}});
    assert(!session.is_valid());
}

void session_evaluates_the_current_solution()
{
    easylocal::Session session{make_application()};
    session.set_input(make_input(3));
    session.use_initial_solution();

    const auto cost = session.evaluate();

    assert(cost.hard().template get<0>() == 0);
    assert(cost.hard().template get<1>() == 0);
    assert(cost.soft() == 1);
}

// The assignment's components have neither name() nor describe(solution):
// the report gives their position and their value only.
void session_reports_each_cost_component()
{
    easylocal::Session session{make_application()};
    session.set_input(make_input(3));
    session.use_initial_solution();

    const auto report = session.cost_report();

    assert(report.size() == 2);
    assert(report[0].name == "#1");
    assert(report[1].name == "#2");
    assert(report[1].value == "1"); // the load imbalance, the soft cost
    assert(report[0].description.empty());
    assert(report[1].description.empty());
}

void session_runs_app_check_on_the_current_solution()
{
    easylocal::Session session{make_application()};
    session.set_input(make_input(3));
    session.use_initial_solution();

    const auto report = session.check();

    assert(report.passed());
    assert(report.checks() > 0);
    assert(report.coverage().solution_managers == 1);
    assert(report.coverage().neighborhood_graphs == 1);
    assert(report.coverage().runner_registrations == 1);
}

void session_check_reports_an_invalid_current_solution()
{
    easylocal::Session session{make_application()};
    session.set_input(make_input(3));
    session.set_solution(AssignmentSolution{.assignment = {0, 2}});

    const auto report = session.check();

    assert(!report.passed());
    assert(report.failures().size() == 1);
    assert(report.failures().front().check == "check solution");
}

void replacing_input_clears_the_current_solution()
{
    easylocal::Session session{make_application()};
    session.set_input(make_input(3));
    session.use_initial_solution();
    assert(session.has_solution());

    session.set_input(make_input(7));

    assert(!session.has_solution());
}

void session_lists_registered_runners_in_app_order()
{
    easylocal::Session session{make_multi_runner_application()};

    const auto names = session.runner_names();

    assert(names.size() == 2);
    assert(names[0] == std::string_view{"quick"});
    assert(names[1] == std::string_view{"deep"});
}

void session_runs_a_named_runner_on_the_current_solution()
{
    easylocal::Session session{make_multi_runner_application()};
    session.set_input(make_input(3));
    session.set_solution(AssignmentSolution{.assignment = {0, 0}});

    const auto before = session.evaluate();
    assert(before.soft() == 7);
    assert(!session.last_run_effort()); // no run yet

    const auto ran = session.run("deep");

    assert(ran);
    // First Improvement reports its effort: one committed move, then a
    // local optimum.
    const auto& effort = session.last_run_effort();
    assert(effort);
    assert(effort->iterations >= 1);
    assert(effort->evaluations > effort->iterations);
    assert(effort->termination == easylocal::termination_reason::local_optimum);
    assert(session.has_solution());
    assert(session.is_valid());
    assert(session.evaluate().soft() == 1);
    assert(session.solution().assignment[0] == 1);
    assert(session.solution().assignment[1] == 0);
}

void session_distinguishes_same_tag_runners_by_name()
{
    easylocal::Session session{make_multi_runner_application()};
    session.set_input(make_input(3));
    session.set_solution(AssignmentSolution{.assignment = {0, 0}});

    const auto ran_quick = session.run("quick");

    assert(ran_quick);
    assert(session.evaluate().soft() == 7);

    const auto ran_deep = session.run("deep");

    assert(ran_deep);
    assert(session.evaluate().soft() == 1);
}

void session_reports_unknown_runner_without_changing_solution()
{
    easylocal::Session session{make_multi_runner_application()};
    session.set_input(make_input(3));
    session.set_solution(AssignmentSolution{.assignment = {0, 0}});

    const auto ran = session.run("missing");

    assert(!ran);
    assert(session.evaluate().soft() == 7);
}

// An outside solution may be invalid (the session lets one inspect it), but a
// run does not start from it: it throws and changes nothing.
void session_rejects_a_run_from_an_invalid_solution()
{
    easylocal::Session session{make_multi_runner_application()};
    session.set_input(make_input(3));
    session.set_solution(AssignmentSolution{.assignment = {0, 2}});

    bool rejected = false;
    try
    {
        static_cast<void>(session.run("deep"));
    }
    catch (const std::invalid_argument& error)
    {
        rejected =
            std::string_view{error.what()}.find("not valid") != std::string_view::npos;
    }

    assert(rejected);
    assert(session.has_solution());
    assert(session.solution().assignment[1] == 2);
    assert(!session.last_run_effort());
}

void app_copy_preserves_graph_configuration()
{
    auto application = make_application();
    application.runner_config<easylocal::runners::FirstImprovement>().max_evaluations =
        17;

    const auto copy = application;

    assert(copy.name() == std::string_view{"assignment"});
    assert(
        copy.runner_name<easylocal::runners::FirstImprovement>()
        == std::string_view{"fi"});
    assert(
        copy.runner_config<easylocal::runners::FirstImprovement>().max_evaluations == 17);
}

void session_can_copy_an_lvalue_app()
{
    auto application = make_application();
    easylocal::Session session{application};

    assert(session.app().name() == std::string_view{"assignment"});
    assert(application.name() == std::string_view{"assignment"});

    session.app().runner_config<easylocal::runners::FirstImprovement>().max_evaluations =
        3;

    assert(
        application.runner_config<easylocal::runners::FirstImprovement>().max_evaluations
        != 3);
}

void session_can_take_ownership_of_an_rvalue_app()
{
    auto application = make_application();
    application.runner_config<easylocal::runners::FirstImprovement>().max_evaluations =
        29;

    easylocal::Session session{std::move(application)};

    assert(session.app().name() == std::string_view{"assignment"});
    assert(
        session.app()
            .runner_config<easylocal::runners::FirstImprovement>()
            .max_evaluations
        == 29);
}

} // namespace

void session_reports_neighborhood_diagnostics()
{
    easylocal::Session session{make_application()};
    session.set_input(make_input(3));
    session.set_solution(AssignmentSolution{.assignment = {0, 0}});

    static_assert(decltype(session)::supports_cost_consistency_check);
    static_assert(decltype(session)::supports_move_independence_check);
    static_assert(decltype(session)::supports_random_distribution_check);

    const auto preview = session.neighborhood_preview(1);
    assert(preview.moves == 2);
    assert(preview.entries.size() == 1);

    const auto statistics = session.neighborhood_statistics();
    assert(statistics.moves == 2);
    assert(statistics.invalid == 0);
    assert(statistics.improving + statistics.sideways + statistics.worsening == 2);

    const auto costs = session.check_neighborhood_costs();
    assert(costs.moves == 2);
    assert(costs.invalid == 0);
    assert(costs.mismatches == 0);

    const auto independence = session.check_move_independence();
    assert(independence.moves == 2);
    assert(independence.invalid == 0);
    assert(independence.null_moves == 0);
    assert(independence.repeated_states == 0);

    std::mt19937_64 rng{1234};
    const auto distribution = session.check_random_move_distribution(rng, 8);
    assert(distribution.neighborhood_size == 2);
    assert(distribution.samples == 16);
    assert(distribution.out_of_neighborhood == 0);
    assert(distribution.unseen == 0);
    assert(distribution.min_frequency <= distribution.max_frequency);
}

int main()
{
    app_copy_preserves_graph_configuration();
    session_can_copy_an_lvalue_app();
    session_can_take_ownership_of_an_rvalue_app();
    session_takes_its_input_and_seed_at_construction();
    session_shares_an_input_it_does_not_copy();
    session_runs_pass_options_to_the_runner();
    session_owns_input_and_builds_instance_from_it();
    replacing_input_rebuilds_the_bound_app();
    session_exposes_initial_solution_as_an_explicit_choice();
    session_exposes_random_solution_as_an_explicit_choice();
    session_exposes_deterministic_and_random_move_capabilities();
    session_selects_first_and_next_moves_deterministically();
    session_selects_first_improving_and_best_moves();
    session_evaluates_the_current_solution_once_per_scan();
    session_selects_random_moves_with_an_explicit_rng();
    session_compares_move_evaluation_with_full_recomputation();
    session_reports_neighborhood_diagnostics();
    applying_a_move_updates_the_solution_and_clears_move_state();
    replacing_the_solution_clears_move_state();
    session_can_inspect_an_explicit_invalid_move();
    session_reports_when_the_current_solution_has_no_moves();
    session_accepts_and_validates_an_explicit_solution();
    session_evaluates_the_current_solution();
    session_reports_each_cost_component();
    session_runs_app_check_on_the_current_solution();
    session_check_reports_an_invalid_current_solution();
    replacing_input_clears_the_current_solution();
    session_lists_registered_runners_in_app_order();
    session_runs_a_named_runner_on_the_current_solution();
    session_distinguishes_same_tag_runners_by_name();
    session_reports_unknown_runner_without_changing_solution();
    session_rejects_a_run_from_an_invalid_solution();
}
