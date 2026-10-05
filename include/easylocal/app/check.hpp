#pragma once

/// \file
/// check(app, input, solution): the contract checks of easylocal::testing run
/// on every component an app composes (SolutionManager, cost components,
/// neighborhoods, delta cost components, runners), with the composition they
/// ran on.

#include <easylocal/app/app.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost/semantics.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/testing/check.hpp>
#include <easylocal/testing/detail/support.hpp>
#include <easylocal/testing/fixture.hpp>
#include <easylocal/utils/detail/meta.hpp>

#include <concepts>
#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <ostream>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal
{

/// The composition of an app that check(app, input, solution) ran on: how
/// many parts of each kind it composes.
///
/// It counts the parts the app declares, whether or not a check failed
/// before reaching them.
struct app_check_composition
{
    /// SolutionManagers.
    std::size_t solution_managers{};
    /// Cost components of the SolutionManager.
    std::size_t cost_components{};
    /// Neighborhoods: the app's, and those of the runners that have their own.
    std::size_t neighborhoods{};
    /// Delta cost bindings of the neighborhoods.
    std::size_t delta_bindings{};
    /// Runner registrations.
    std::size_t runner_registrations{};
};

/// The report of check(app, input, solution): the checks that passed and
/// failed, and the composition they ran on.
///
/// It converts to true when every check passed; print_report writes it, and
/// testing::run_checks takes it with the reports of the component checks.
class app_check_report
{
public:
    /// An empty report on subject, the name of the app.
    explicit app_check_report(std::string_view subject)
        : report_{subject}
    {
    }

    /// Whether every check passed.
    [[nodiscard]] bool passed() const noexcept
    {
        return report_.passed();
    }
    /// Whether every check passed.
    [[nodiscard]] explicit operator bool() const noexcept { return passed(); }
    /// The number of checks made.
    [[nodiscard]] std::size_t checks() const noexcept
    {
        return report_.checks();
    }
    /// The checks that failed.
    [[nodiscard]] std::span<const testing::check_failure> failures() const noexcept
    {
        return report_.failures();
    }
    /// The composition of the app the checks ran on.
    template<class Self>
    [[nodiscard]] auto& composition(this Self&& self) noexcept
    {
        return self.composition_;
    }

    /// Records a check named name, failed with message when condition is false.
    void check(bool condition, std::string_view name, std::string_view message)
    {
        report_.check(condition, name, message);
    }

private:
    friend void print_report(std::ostream&, const app_check_report&);

    // The checks of components, as the component checks of testing record
    // them.
    friend testing::check_report& checks_of(app_check_report& report) noexcept
    {
        return report.report_;
    }

    testing::check_report report_;
    app_check_composition composition_{};
};

/// Writes the checks of a report to out, then a line with the composition
/// they ran on.
inline void print_report(std::ostream& out, const app_check_report& report)
{
    testing::print_report(out, report.report_);
    const auto& composition = report.composition_;
    out << "composition: solution_managers=" << composition.solution_managers
        << ", cost_components=" << composition.cost_components << ", neighborhoods="
        << composition.neighborhoods << ", delta_bindings=" << composition.delta_bindings
        << ", runner_registrations=" << composition.runner_registrations << '\n';
}

namespace detail
{

template<class SM>
inline constexpr std::size_t app_cost_component_count_v = [] {
    if constexpr (requires { typename SM::component_types; })
        return tuple_size_or_zero_v<typename SM::component_types>;
    else
        return std::size_t{0};
}();

template<class NHE>
inline constexpr std::size_t app_delta_binding_count_v = [] {
    if constexpr (requires { typename NHE::delta_bindings_type; })
        return tuple_size_or_zero_v<typename NHE::delta_bindings_type>;
    else
        return std::size_t{0};
}();

// With a compare at the root of the cost expression, that the sign of
// cost::delta agrees with it, as the delta-based algorithms assume: a
// candidate better than the current solution has a negative delta, a worse one
// a positive delta.
template<class Report, class SM, class Cost>
void check_delta_sign(
    Report& report,
    const SM& solution_manager,
    const Cost& candidate,
    const Cost& current)
{
    // cost::approximately orders as the cost does, up to a tolerance that the
    // sign of a delta does not know.
    constexpr bool tolerant = requires {
        solution_manager.cost_expression().tolerance();
    };
    if constexpr (cost::detail::custom_compare<SM> && cost::has_delta<Cost> && !tolerant)
    {
        using cost::delta;
        const auto difference = static_cast<long double>(delta(candidate, current));
        const bool agrees = cost::better(solution_manager, candidate, current)
            ? difference < 0
            : !cost::better(solution_manager, current, candidate) || difference > 0;
        report.check(
            agrees,
            "delta sign",
            "cost::delta(candidate, current) disagrees with the compare of the cost "
            "expression: Simulated Annealing, Great Deluge and the aspiration levels "
            "of Tabu Search take a negative delta as an improvement");
    }
}

// Whether two costs are equal within the tolerance of the checks, when their
// type can be compared so.
template<class Cost>
[[nodiscard]]
bool within_tolerance(
    const testing::approximately& tolerance,
    const Cost& lhs,
    const Cost& rhs)
{
    if constexpr (cost::approximately_equality_comparable<Cost, Cost>)
        return tolerance(lhs, rhs);
    else
        return false;
}

// The checks of one move of solution, whose evaluation is current: valid, it
// leads to a valid solution, and its incremental evaluation agrees with the
// full one, as the search compares costs (with the cost expression's
// equivalence when it defines one, or within the tolerance, which forgives
// the rounding errors of a floating-point cost updated by deltas, as the
// Session does), with a delta whose sign agrees with a root compare.
template<class SM, class NHE, class Evaluation, class Solution, class Current>
void check_app_move(
    testing::check_report& report,
    const SM& solution_manager,
    const NHE& neighborhood,
    const Evaluation& evaluation,
    const Solution& solution,
    const Current& current,
    const std::string& label,
    const typename NHE::move_type& move,
    const testing::approximately& tolerance)
{
    testing::detail::guarded(report, "incremental evaluation", label, [&] {
        const auto valid = static_cast<bool>(neighborhood.is_valid(solution, move));
        report.check(
            valid,
            "neighborhood move validity",
            label + ": an enumerated move does not satisfy is_valid");
        if (!valid)
            return;

        auto candidate_solution = solution;
        neighborhood.make_move(candidate_solution, move);
        const auto valid_candidate =
            static_cast<bool>(solution_manager.is_valid(candidate_solution));
        report.check(
            valid_candidate,
            "neighborhood move application",
            label + ": make_move produced an invalid Solution");
        if (!valid_candidate)
            return;

        auto incremental_state = current;
        auto committed_solution = solution;
        auto candidate = evaluation.evaluate_move(solution, current, move);
        check_delta_sign(report, solution_manager, candidate.cost(), current.cost());
        evaluation.commit(committed_solution, incremental_state, std::move(candidate));
        const auto full = evaluation.evaluate(committed_solution);

        if constexpr (cost::has_equivalent<SM>)
        {
            const auto& incremental = incremental_state.cost();
            report.check(
                cost::equivalent(solution_manager, incremental, full.cost())
                    || detail::within_tolerance(tolerance, incremental, full.cost()),
                "incremental evaluation",
                label + ": the incremental evaluation is "
                    + testing::detail::value_text(incremental)
                    + ", the full evaluation after the move "
                    + testing::detail::value_text(full.cost()));
        }
    });
}

// The checks of a neighborhood from solution and from the random solutions of
// options: its enumerated moves (a sample of max_enumerated_moves) and
// random_samples random moves are valid, lead to valid solutions and are
// evaluated incrementally as in full; the random moves are moves of the
// enumeration.
template<class SM, class NHE, class Solution>
void check_app_neighborhood(
    testing::check_report& report,
    const SM& solution_manager,
    const NHE& neighborhood,
    const Solution& solution,
    const testing::check_options& options)
{
    using move_type = typename NHE::move_type;
    const runner_context<SM, NHE> context{solution_manager, neighborhood};
    const auto evaluation = context.evaluation();
    std::mt19937_64 rng{options.seed};

    testing::detail::for_each_start(
        solution_manager,
        neighborhood,
        solution,
        "the Solution",
        options,
        rng,
        [&](const Solution& start, const std::string& from) {
            using evaluation_type =
                std::remove_cvref_t<decltype(evaluation.evaluate(start))>;
            std::optional<evaluation_type> current;
            if (!testing::detail::guarded(
                    report,
                    "evaluation",
                    "evaluating " + from,
                    [&] { current.emplace(evaluation.evaluate(start)); }))
                return;

            if constexpr (deterministic_neighborhood_for<NHE, Solution>)
            {
                std::vector<move_type> moves;
                if (testing::detail::guarded(
                        report,
                        "neighborhood move validity",
                        "enumerating the moves of " + from,
                        [&] {
                            moves = testing::detail::sample_moves(
                                neighborhood,
                                start,
                                options.max_enumerated_moves,
                                rng);
                        }))
                {
                    for (std::size_t index = 0; index < moves.size(); ++index)
                        check_app_move(
                            report,
                            solution_manager,
                            neighborhood,
                            evaluation,
                            start,
                            *current,
                            testing::detail::move_label(index, moves[index]) + " from "
                                + from,
                            moves[index],
                            options.tolerance);
                }
            }

            if constexpr (random_neighborhood_for<NHE, Solution, std::mt19937_64>)
            {
                for (std::size_t sample = 0; sample < options.random_samples; ++sample)
                {
                    std::optional<move_type> move;
                    if (!testing::detail::guarded(
                            report,
                            "random proposal",
                            "draw " + std::to_string(sample) + " from " + from,
                            [&] {
                                move = easylocal::random_move(neighborhood, start, rng);
                            }))
                        break;
                    if (!move)
                        continue;
                    const auto label = testing::detail::move_label(sample, *move)
                        + " drawn from " + from;
                    testing::detail::guarded(report, "random proposal", label, [&] {
                        const auto valid =
                            static_cast<bool>(neighborhood.is_valid(start, *move));
                        report.check(
                            valid,
                            "random proposal",
                            label
                                + ": random_move produced a move that does not satisfy "
                                  "is_valid");
                        if (!valid)
                            return;
                        auto candidate = start;
                        neighborhood.make_move(candidate, *move);
                        report.check(
                            static_cast<bool>(solution_manager.is_valid(candidate)),
                            "random proposal application",
                            label + ": make_move produced an invalid Solution");
                    });
                }
            }
        });

    testing::detail::check_random_moves_against_enumeration(
        report,
        neighborhood,
        solution,
        options);
}

// The check of the registration names, which binding the app requires: false
// when they are not valid.
template<class App>
bool check_registration_names(const App& application, app_check_report& report)
{
    try
    {
        application.check_registration_names();
    }
    catch (const std::invalid_argument& error)
    {
        report.check(false, "registration names", error.what());
        return false;
    }
    report.check(true, "registration names", {});
    return true;
}

// The checks of the registered runners: their parameters are configurable (a
// parameter block, unless there are none), valid, and construct the runner.
// False when some parameters are invalid: a runner asserts that they are
// valid, and binding the app constructs its runners.
template<class App>
bool check_runners(const App& application, app_check_report& report)
{
    bool valid = true;
    app_access::for_each_runner(
        application,
        [&]<class Algorithm>(std::string_view name, const auto& config) {
            const auto runner = "runner " + std::string{name} + ": ";
            using parameters_type = std::remove_cvref_t<decltype(config)>;
            if constexpr (!config::parameter_block<parameters_type>
                && !std::is_empty_v<parameters_type>)
            {
                report.check(
                    false,
                    "runner parameters",
                    runner
                        + "its parameters_type is not a parameter block, so no "
                          "frontend can change its parameters: give it a "
                          "parameter_schema() and a validate()");
            }
            if constexpr (requires { config.validate(); })
            {
                const auto validation = config.validate();
                report.check(
                    static_cast<bool>(validation),
                    "runner configuration",
                    runner + std::string{validation.message});
                if (!validation)
                {
                    valid = false;
                    return;
                }
            }

            try
            {
                [[maybe_unused]] Algorithm algorithm{config};
                report.check(true, "runner construction", {});
            }
            catch (const std::exception& error)
            {
                report.check(false, "runner construction", runner + error.what());
            }
        });
    return valid;
}

// The check of the parameters of the whole app, among them those of its
// pipelines' stages: their values, one failed check per invalid block with
// its path, and stage names that are distinct and non-empty. False when some
// are invalid, which binding the app rejects.
template<class App>
bool check_configuration(const App& application, app_check_report& report)
{
    try
    {
        const auto validation = config::validate(application.configuration());
        for (const auto& diagnostic : validation.diagnostics)
            report.check(
                false,
                "app configuration",
                (diagnostic.path.empty() ? std::string{} : diagnostic.path + ": ")
                    + diagnostic.message);
        if (!validation)
            return false;
        application.check_configuration();
    }
    catch (const std::invalid_argument& error)
    {
        report.check(false, "app configuration", error.what());
        return false;
    }
    report.check(true, "app configuration", {});
    return true;
}

// The composition an app declares, the neighborhoods of its runners left to
// the checks that bind it.
template<class App>
void count_composition(app_check_report& report)
{
    report.composition().runner_registrations = App::runner_count;
}

// The checks that do not need the app bound: the registration names, the
// runners and the parameters of the app; false when the app cannot be bound.
template<class App>
bool check_unbound(const App& application, app_check_report& report)
{
    count_composition<App>(report);
    return check_registration_names(application, report)
        && check_runners(application, report) && check_configuration(application, report);
}

// The checks of the bound app from solution.
template<class Bound, class Instance, class Solution>
void check_bound(
    const Bound& bound,
    const Instance& instance,
    const Solution& solution,
    const testing::check_options& options,
    app_check_report& report)
{
    using solution_manager_type = typename Bound::solution_manager_type;
    using neighborhood_type = typename Bound::neighborhood_explorer_type;
    auto& checks = checks_of(report);

    auto& composition = report.composition();
    composition.solution_managers = 1;
    composition.cost_components = app_cost_component_count_v<solution_manager_type>;
    composition.neighborhoods = 1;
    composition.delta_bindings = app_delta_binding_count_v<neighborhood_type>;

    const auto& solution_manager = bound.solution_manager();
    const auto& neighborhood = bound.neighborhood();

    report.check(
        std::addressof(bound.input()) == std::addressof(instance),
        "app input binding",
        "the materialized app does not refer to the supplied Input");
    report.check(
        std::addressof(solution_manager.input()) == std::addressof(instance),
        "solution manager input binding",
        "SolutionManager::input() does not refer to the app Input");
    report.check(
        std::addressof(neighborhood.input()) == std::addressof(instance),
        "neighborhood input binding",
        "NeighborhoodExplorer::input() does not refer to the app Input");

    bool valid_solution = false;
    testing::detail::guarded(checks, "check solution", "is_valid(solution)", [&] {
        valid_solution = static_cast<bool>(solution_manager.is_valid(solution));
    });
    report.check(
        valid_solution,
        "check solution",
        "the Solution supplied to check(app, input, solution) is invalid");
    if (!valid_solution)
        return;

    if constexpr (requires { solution_manager.evaluate(solution); })
    {
        testing::detail::guarded(checks, "repeat evaluation", "evaluate(solution)", [&] {
            const auto first = solution_manager.evaluate(solution);
            const auto second = solution_manager.evaluate(solution);
            bool same = true;
            if constexpr (cost::has_equivalent<solution_manager_type>)
                same = cost::equivalent(solution_manager, first, second);
            else if constexpr (requires {
                                   { first == second } -> std::convertible_to<bool>;
                               })
                same = static_cast<bool>(first == second);
            report.check(
                same,
                "repeat evaluation",
                "evaluating the same Solution twice gave "
                    + testing::detail::value_text(first) + " and then "
                    + testing::detail::value_text(second));
        });
    }

    if constexpr (has_random_solution<solution_manager_type, std::mt19937_64>)
    {
        std::mt19937_64 rng{options.seed};
        for (std::size_t sample = 0; sample < options.random_samples; ++sample)
        {
            const auto label = "random solution " + std::to_string(sample);
            if (!testing::detail::guarded(checks, "random solution", label, [&] {
                    const auto random = solution_manager.random_solution(rng);
                    report.check(
                        static_cast<bool>(solution_manager.is_valid(random)),
                        "random solution",
                        label + ": random_solution(rng) returned an invalid Solution");
                }))
                break;
        }
    }

    // The app's neighborhood, then those the runners have of their own.
    check_app_neighborhood(checks, solution_manager, neighborhood, solution, options);
    app_access::for_each_own_neighborhood(
        bound,
        [&](std::string_view, const auto& own_neighborhood) {
            ++composition.neighborhoods;
            composition.delta_bindings += app_delta_binding_count_v<
                std::remove_cvref_t<decltype(own_neighborhood)>>;
            check_app_neighborhood(
                checks,
                solution_manager,
                own_neighborhood,
                solution,
                options);
        });
}

// Every parameter declares its domain, for validation and tuning: a range,
// one_of, or easylocal::unlimited for any value.
template<class App>
void check_domains(const App& application, app_check_report& report)
{
    try
    {
        const auto undeclared = config::undeclared_domains(application.configuration());
        for (const auto& path : undeclared)
        {
            report.check(
                false,
                "parameter domain",
                path
                    + " declares no domain: give it a range, one_of, or "
                      "easylocal::unlimited for any value");
        }
        if (undeclared.empty())
            report.check(true, "parameter domain", "every parameter declares its domain");
    }
    catch (const std::invalid_argument&)
    {
        // Reported as an invalid app configuration.
    }
}

} // namespace detail

/// Runs the contract checks of easylocal::testing on the components of an app
/// bound to instance, from solution and from random solutions, with the
/// options of the checks, and returns their report.
///
/// It checks that the services refer to instance, that solution is valid and
/// evaluates twice to an equivalent cost, that random solutions are valid,
/// and that the moves of each neighborhood (the app's, and those of the
/// runners that have their own) from solution and from the random solutions
/// of the options (`random_solutions` solutions walked by `walk_length` random
/// moves) are valid and lead to valid solutions: a sample of
/// `max_enumerated_moves` enumerated moves and `random_samples` random ones,
/// whose incremental evaluation matches the full one (by the cost's
/// equivalence or within the tolerance of the options, when the cost defines
/// equivalence), with the sign of cost::delta agreeing with a root compare of
/// the cost, and random moves among the enumerated ones. It checks that each
/// registered runner's parameters are a parameter block (unless it has
/// none), are valid and construct it. The runners and the app's parameters
/// are checked first: with invalid ones the app is not bound, since binding it
/// rejects them. A failure names the move and the solution it starts from; an
/// exception of a hook is a failure.
template<class App, class Instance, class Solution>
[[nodiscard]] app_check_report check(
    const App& application,
    const Instance& instance,
    Solution solution,
    const testing::check_options& options = {})
    requires requires {
        application.bind(instance).solution_manager().is_valid(solution);
    }
{
    app_check_report report{application.name()};
    if (!detail::check_unbound(application, report))
        return report;
    auto bound = application.bind(instance);
    detail::check_bound(bound, instance, solution, options, report);
    detail::check_domains(application, report);
    return report;
}

/// Runs check(app, instance, solution, options) from the SolutionManager's
/// initial_solution(), binding the app once.
template<class App, class Instance>
[[nodiscard]] app_check_report check(
    const App& application,
    const Instance& instance,
    const testing::check_options& options = {})
    requires requires {
        application.bind(instance).solution_manager().initial_solution();
    }
{
    app_check_report report{application.name()};
    if (!detail::check_unbound(application, report))
        return report;
    auto bound = application.bind(instance);
    std::optional<
        std::remove_cvref_t<decltype(bound.solution_manager().initial_solution())>>
        initial;
    if (!testing::detail::guarded(
            checks_of(report),
            "check solution",
            "initial_solution()",
            [&] { initial.emplace(bound.solution_manager().initial_solution()); }))
        return report;
    detail::check_bound(bound, instance, *initial, options, report);
    detail::check_domains(application, report);
    return report;
}

} // namespace easylocal
