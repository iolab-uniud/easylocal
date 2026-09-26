#pragma once

#include <easylocal/cursor_moves.hpp>
#include <easylocal/neighborhood_concepts.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <ostream>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::testing
{

struct contract_failure
{
    std::string contract;
    std::string message;
};

class contract_report
{
public:
    [[nodiscard]] auto passed() const noexcept -> bool
    {
        return failures_.empty();
    }

    [[nodiscard]] auto checks() const noexcept -> std::size_t
    {
        return checks_;
    }

    [[nodiscard]] auto failures() const noexcept
        -> std::span<const contract_failure>
    {
        return failures_;
    }

    void check(
        const bool condition,
        std::string_view contract,
        std::string_view message)
    {
        ++checks_;
        if (!condition)
        {
            failures_.push_back(contract_failure{
                .contract = std::string{contract},
                .message = std::string{message},
            });
        }
    }

private:
    std::size_t checks_{};
    std::vector<contract_failure> failures_;
};

inline void print_report(
    std::ostream& out,
    const contract_report& report)
{
    if (report.passed())
    {
        out << "EasyLocal contract: " << report.checks()
            << " checks passed\n";
        return;
    }

    out << "EasyLocal contract: " << report.failures().size()
        << " failure(s) in " << report.checks() << " checks\n";

    for (const auto& failure : report.failures())
    {
        out << "[fail] " << failure.contract << "\n"
            << "       " << failure.message << "\n";
    }
}

class deterministic_rng
{
public:
    using result_type = std::uint64_t;

    deterministic_rng()
        : deterministic_rng({
              0x9e3779b97f4a7c15ULL,
              0xbf58476d1ce4e5b9ULL,
              0x94d049bb133111ebULL,
              0x123456789abcdef0ULL,
          })
    {
    }

    deterministic_rng(std::initializer_list<result_type> values)
        : values_{values}
    {
        if (values_.empty())
        {
            values_.push_back(0);
        }
    }

    [[nodiscard]]
    static constexpr auto min() noexcept -> result_type
    {
        return std::numeric_limits<result_type>::min();
    }

    [[nodiscard]]
    static constexpr auto max() noexcept -> result_type
    {
        return std::numeric_limits<result_type>::max();
    }

    auto operator()() noexcept -> result_type
    {
        const auto value = values_[index_];
        index_ = (index_ + 1) % values_.size();
        return value;
    }

    void reset() noexcept
    {
        index_ = 0;
    }

private:
    std::vector<result_type> values_;
    std::size_t index_{};
};

static_assert(std::uniform_random_bit_generator<deterministic_rng>);

namespace detail
{

template<class Test, class Neighborhood, bool HasBoundSM = requires {
    typename Neighborhood::solution_manager_type;
}>
struct contract_solution_manager;

template<class Test, class Neighborhood>
struct contract_solution_manager<Test, Neighborhood, true>
{
    using type = typename Neighborhood::solution_manager_type;
};

template<class Test, class Neighborhood>
struct contract_solution_manager<Test, Neighborhood, false>
{
    using type = typename Test::solution_manager;
};

template<class Test, class Neighborhood>
using contract_solution_manager_t =
    typename contract_solution_manager<Test, Neighborhood>::type;

template<class Test>
inline constexpr std::size_t max_enumerated_moves_v = [] {
    if constexpr (requires { Test::max_enumerated_moves; })
    {
        return static_cast<std::size_t>(Test::max_enumerated_moves);
    }
    else
    {
        return std::size_t{1024};
    }
}();

template<class Test>
inline constexpr std::size_t random_samples_v = [] {
    if constexpr (requires { Test::random_samples; })
    {
        return static_cast<std::size_t>(Test::random_samples);
    }
    else
    {
        return std::size_t{32};
    }
}();

template<class Test, class Instance, class SM>
[[nodiscard]] auto make_solution_manager(Instance& instance)
{
    if constexpr (requires { Test::make_solution_manager(instance); })
    {
        auto manager = Test::make_solution_manager(instance);
        static_assert(std::same_as<std::remove_cvref_t<decltype(manager)>, SM>);
        return manager;
    }
    else
    {
        static_assert(
            std::constructible_from<SM, const Instance&>,
            "neighborhood contract cannot construct the SolutionManager; "
            "provide Test::make_solution_manager(instance)");
        return SM{instance};
    }
}

template<class Test, class SM, class NHE>
[[nodiscard]] auto make_neighborhood(SM& solution_manager)
{
    if constexpr (requires { Test::make_neighborhood(solution_manager); })
    {
        auto neighborhood = Test::make_neighborhood(solution_manager);
        static_assert(std::same_as<std::remove_cvref_t<decltype(neighborhood)>, NHE>);
        return neighborhood;
    }
    else
    {
        static_assert(
            std::constructible_from<NHE, SM&> ||
            std::constructible_from<NHE, const SM&>,
            "neighborhood contract cannot construct the NeighborhoodExplorer; "
            "provide Test::make_neighborhood(solution_manager)");
        return NHE{solution_manager};
    }
}

template<class SM, class NHE, class Solution, class Range>
void check_moves(
    contract_report& report,
    std::string_view contract,
    const SM& solution_manager,
    const NHE& neighborhood,
    const Solution& solution,
    Range&& range,
    const std::size_t limit)
{
    using move_type = typename NHE::move_type;
    std::size_t seen = 0;

    for (auto&& raw_move : range)
    {
        if (seen == limit)
        {
            break;
        }
        ++seen;

        move_type move{raw_move};
        const auto valid = static_cast<bool>(
            neighborhood.is_valid(solution, move));
        report.check(
            valid,
            contract,
            "enumerated move does not satisfy NeighborhoodExplorer::is_valid");

        if (!valid)
        {
            continue;
        }

        auto candidate = solution;
        neighborhood.make_move(candidate, move);
        report.check(
            static_cast<bool>(solution_manager.is_valid(candidate)),
            contract,
            "make_move produced an invalid Solution");
    }
}

} // namespace detail

template<class Test>
[[nodiscard]] auto neighborhood_contract() -> contract_report
{
    using neighborhood_type = typename Test::neighborhood;
    using solution_manager_type =
        detail::contract_solution_manager_t<Test, neighborhood_type>;
    using instance_type = typename solution_manager_type::instance_type;
    using solution_type = typename solution_manager_type::solution_type;

    static_assert(
        neighborhood_explorer_for<neighborhood_type, solution_manager_type>,
        "Test::neighborhood does not satisfy the EasyLocal NeighborhoodExplorer core contract");

    auto instance = Test::instance();
    static_assert(std::same_as<std::remove_cvref_t<decltype(instance)>, instance_type>);

    auto solution_manager = detail::make_solution_manager<
        Test, instance_type, solution_manager_type>(instance);
    auto neighborhood = detail::make_neighborhood<
        Test, solution_manager_type, neighborhood_type>(solution_manager);
    auto solution = Test::solution(instance);
    static_assert(std::same_as<std::remove_cvref_t<decltype(solution)>, solution_type>);

    contract_report report;

    const auto valid_solution = static_cast<bool>(
        solution_manager.is_valid(solution));
    report.check(
        valid_solution,
        "fixture",
        "Test::solution(instance) must return a valid Solution");

    if (!valid_solution)
    {
        return report;
    }

    constexpr auto max_moves = detail::max_enumerated_moves_v<Test>;

    if constexpr (cursor_neighborhood_for<neighborhood_type, solution_type>)
    {
        detail::check_moves(
            report,
            "cursor traversal",
            solution_manager,
            neighborhood,
            solution,
            easylocal::cursor_moves(neighborhood, solution),
            max_moves);
    }

    if constexpr (native_moves_neighborhood_for<neighborhood_type, solution_type>)
    {
        detail::check_moves(
            report,
            "native moves traversal",
            solution_manager,
            neighborhood,
            solution,
            neighborhood.moves(solution),
            max_moves);
    }

    if constexpr (random_neighborhood_for<
                      neighborhood_type,
                      solution_type,
                      deterministic_rng>)
    {
        deterministic_rng rng;
        for (std::size_t sample = 0;
             sample < detail::random_samples_v<Test>;
             ++sample)
        {
            auto move = easylocal::random_move(neighborhood, solution, rng);
            if (!move)
            {
                continue;
            }

            const auto valid = static_cast<bool>(
                neighborhood.is_valid(solution, *move));
            report.check(
                valid,
                "random proposal",
                "random_move produced a move that does not satisfy is_valid");

            if (!valid)
            {
                continue;
            }

            auto candidate = solution;
            neighborhood.make_move(candidate, *move);
            report.check(
                static_cast<bool>(solution_manager.is_valid(candidate)),
                "random proposal",
                "random move produced an invalid Solution");
        }
    }

    return report;
}

template<class Test>
auto run_contract(std::ostream& out = std::cerr) -> int
{
    const auto report = neighborhood_contract<Test>();
    print_report(out, report);
    return report.passed() ? EXIT_SUCCESS : EXIT_FAILURE;
}

template<class... Tests>
auto run_contracts(std::ostream& out = std::cerr) -> int
{
    bool passed = true;
    ((passed = (run_contract<Tests>(out) == EXIT_SUCCESS) && passed), ...);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace easylocal::testing
