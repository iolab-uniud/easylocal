#pragma once

#include <easylocal/utils/detail/meta.hpp>
#include <easylocal/app/app.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/testing/check.hpp>

#include <concepts>
#include <cstddef>
#include <exception>
#include <memory>
#include <ostream>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal
{

struct app_check_coverage
{
    std::size_t solution_managers{};
    std::size_t cost_components{};
    std::size_t neighborhood_graphs{};
    std::size_t delta_bindings{};
    std::size_t runner_registrations{};
};

class app_check_report
{
public:
    explicit app_check_report(std::string_view subject)
        : report_{subject}
    {
    }

    [[nodiscard]] bool passed() const noexcept
    {
        return report_.passed();
    }
    [[nodiscard]] explicit operator bool() const noexcept { return passed(); }
    [[nodiscard]] std::size_t checks() const noexcept
    {
        return report_.checks();
    }
    [[nodiscard]] auto failures() const noexcept { return report_.failures(); }
    [[nodiscard]] app_check_coverage& coverage() noexcept
    {
        return coverage_;
    }
    [[nodiscard]] const app_check_coverage& coverage() const noexcept
    {
        return coverage_;
    }

    void check(bool condition, std::string_view name, std::string_view message)
    {
        report_.check(condition, name, message);
    }

private:
    template<class App, class Instance, class Solution>
    friend app_check_report check(const App&, const Instance&, Solution);

    friend void print_report(std::ostream&, const app_check_report&);

    testing::check_report report_;
    app_check_coverage coverage_{};
};

inline void print_report(std::ostream& out, const app_check_report& report)
{
    testing::print_report(out, report.report_);
    const auto& coverage = report.coverage_;
    out << "coverage: solution_managers=" << coverage.solution_managers
        << ", cost_components=" << coverage.cost_components
        << ", neighborhood_graphs=" << coverage.neighborhood_graphs
        << ", delta_bindings=" << coverage.delta_bindings
        << ", runner_registrations=" << coverage.runner_registrations << '\n';
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

template<class Report, class SM, class NHE, class Solution, class Range>
void check_app_moves(
    Report& report,
    const SM& solution_manager,
    const NHE& neighborhood,
    const Solution& solution,
    Range&& moves,
    std::size_t limit)
{
    using move_type = typename NHE::move_type;
    const runner_context<SM, NHE> context{solution_manager, neighborhood};
    const auto evaluation = context.evaluation();
    const auto current = evaluation.evaluate(solution);

    std::size_t seen = 0;
    for (auto&& raw_move : moves)
    {
        if (seen++ == limit)
            break;

        move_type move{raw_move};
        const auto valid = static_cast<bool>(neighborhood.is_valid(solution, move));
        report.check(
            valid,
            "neighborhood move validity",
            "an enumerated move does not satisfy NeighborhoodExplorer::is_valid");
        if (!valid)
            continue;

        auto candidate_solution = solution;
        neighborhood.make_move(candidate_solution, move);
        const auto valid_candidate =
            static_cast<bool>(solution_manager.is_valid(candidate_solution));
        report.check(
            valid_candidate,
            "neighborhood move application",
            "make_move produced an invalid Solution");
        if (!valid_candidate)
            continue;

        auto incremental_state = current;
        auto committed_solution = solution;
        auto candidate = evaluation.evaluate_move(solution, current, move);
        evaluation.commit(
            committed_solution,
            incremental_state,
            std::move(candidate));
        const auto full = evaluation.evaluate(committed_solution);

        if constexpr (requires {
                          { incremental_state.cost() == full.cost() }
                              -> std::convertible_to<bool>;
                      })
        {
            report.check(
                static_cast<bool>(incremental_state.cost() == full.cost()),
                "incremental evaluation",
                "incremental move evaluation does not match full recomputation");
        }
    }
}

} // namespace detail

template<class App, class Instance, class Solution>
[[nodiscard]] app_check_report check(
    const App& application,
    const Instance& instance,
    Solution solution)
    requires requires { application.bind(instance); }
{
    auto bound = application.bind(instance);
    using bound_type = decltype(bound);
    using solution_manager_type = typename bound_type::solution_manager_type;
    using neighborhood_type = typename bound_type::neighborhood_explorer_type;

    app_check_report report{application.name()};
    report.coverage().solution_managers = 1;
    report.coverage().cost_components =
        detail::app_cost_component_count_v<solution_manager_type>;
    report.coverage().neighborhood_graphs = 1;
    report.coverage().delta_bindings =
        detail::app_delta_binding_count_v<neighborhood_type>;
    report.coverage().runner_registrations = App::runner_count;

    const auto& solution_manager = bound.solution_manager();
    const auto& neighborhood = bound.neighborhood();

    report.check(
        std::addressof(bound.input()) == std::addressof(instance),
        "app input binding",
        "the materialized app does not refer to the supplied Input");
    report.check(
        std::addressof(solution_manager.input()) == std::addressof(instance),
        "solution manager input binding",
        "SolutionManager::instance() does not refer to the app Input");
    report.check(
        std::addressof(neighborhood.input()) == std::addressof(instance),
        "neighborhood input binding",
        "NeighborhoodExplorer::instance() does not refer to the app Input");

    const auto valid_solution = static_cast<bool>(solution_manager.is_valid(solution));
    report.check(
        valid_solution,
        "check solution",
        "the Solution supplied to check(app, input, solution) is invalid");
    if (!valid_solution)
        return report;

    if constexpr (requires { solution_manager.evaluate(solution); })
    {
        const auto first = solution_manager.evaluate(solution);
        const auto second = solution_manager.evaluate(solution);
        if constexpr (requires {
                          { first == second } -> std::convertible_to<bool>;
                      })
        {
            report.check(
                static_cast<bool>(first == second),
                "repeat evaluation",
                "evaluating the same Solution twice produced different costs");
        }
    }

    constexpr std::size_t max_moves = 128;
    if constexpr (deterministic_neighborhood_for<neighborhood_type, Solution>)
    {
        detail::check_app_moves(
            report,
            solution_manager,
            neighborhood,
            solution,
            easylocal::moves(neighborhood, solution),
            max_moves);
    }

    if constexpr (random_neighborhood_for<
                      neighborhood_type,
                      Solution,
                      testing::deterministic_rng>)
    {
        testing::deterministic_rng rng;
        for (std::size_t sample = 0; sample < 16; ++sample)
        {
            auto move = easylocal::random_move(neighborhood, solution, rng);
            if (!move)
                continue;

            const auto valid = static_cast<bool>(neighborhood.is_valid(solution, *move));
            report.check(
                valid,
                "random proposal",
                "random_move produced a move that does not satisfy is_valid");
            if (!valid)
                continue;

            auto candidate = solution;
            neighborhood.make_move(candidate, *move);
            report.check(
                static_cast<bool>(solution_manager.is_valid(candidate)),
                "random proposal application",
                "random_move followed by make_move produced an invalid Solution");
        }
    }

    application.for_each_runner_registration(
        [&]<class Algorithm>(std::string_view name, const auto& config) {
            if constexpr (requires { config.validate(); })
            {
                const auto validation = config.validate();
                report.check(
                    static_cast<bool>(validation),
                    "runner configuration",
                    "a registered runner configuration is invalid");
            }

            try
            {
                [[maybe_unused]] Algorithm algorithm{config};
                report.check(
                    true,
                    "runner construction",
                    "registered runner can be constructed");
            }
            catch (const std::exception&)
            {
                report.check(
                    false,
                    "runner construction",
                    "a registered runner could not be constructed");
            }
            (void)name;
        });

    return report;
}

template<class App, class Instance>
[[nodiscard]] auto check(const App& application, const Instance& instance)
    requires requires {
        application.bind(instance).solution_manager().initial_solution();
    }
{
    auto bound = application.bind(instance);
    return check(application, instance, bound.solution_manager().initial_solution());
}

} // namespace easylocal
