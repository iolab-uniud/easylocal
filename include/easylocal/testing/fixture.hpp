#pragma once

#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/testing/check.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

namespace easylocal::testing
{

struct check_options
{
    std::size_t random_samples{32};         // random moves and solutions drawn per check
    std::size_t max_enumerated_moves{1024}; // enumerated moves visited per traversal
};

// The data every component check runs on: an Input, a valid Solution and the
// SolutionManager built on the Input. Values are compared with Equivalent
// (operator== by default; pass a tolerance-based comparison for floating
// point costs that accumulate rounding errors).
//
// The fixture owns the Input the SolutionManager refers to, so it can be
// neither copied nor moved: construct it in place.
template<easylocal::base_solution_manager SM, class Equivalent = std::equal_to<>>
class fixture
{
public:
    using solution_manager_type = SM;
    using input_type = typename SM::input_type;
    using solution_type = typename SM::solution_type;

    fixture(input_type input, solution_type solution, check_options options = {})
        : input_{std::move(input)},
          solution_manager_{input_},
          solution_{std::move(solution)},
          options_{options}
    {
    }

    // The Solution defaults to the SolutionManager's initial solution.
    explicit fixture(input_type input, check_options options = {})
        requires easylocal::has_initial_solution<SM>
        : input_{std::move(input)},
          solution_manager_{input_},
          solution_{solution_manager_.initial_solution()},
          options_{options}
    {
    }

    fixture(const fixture&) = delete;
    fixture& operator=(const fixture&) = delete;

    [[nodiscard]] const input_type& input() const noexcept
    {
        return input_;
    }

    [[nodiscard]] const SM& solution_manager() const noexcept
    {
        return solution_manager_;
    }

    [[nodiscard]] const solution_type& solution() const noexcept
    {
        return solution_;
    }

    [[nodiscard]] const check_options& options() const noexcept
    {
        return options_;
    }

    template<class Left, class Right>
        requires std::predicate<const Equivalent&, const Left&, const Right&>
    [[nodiscard]] bool equivalent(const Left& lhs, const Right& rhs) const
    {
        return static_cast<bool>(equivalent_(lhs, rhs));
    }

private:
    input_type input_;
    SM solution_manager_;
    solution_type solution_;
    check_options options_;
    [[no_unique_address]] Equivalent equivalent_{};
};

namespace detail
{

template<class T>
inline constexpr bool is_fixture_v = false;

template<class SM, class Equivalent>
inline constexpr bool is_fixture_v<fixture<SM, Equivalent>> = true;

} // namespace detail

template<class T>
concept check_fixture = detail::is_fixture_v<std::remove_cvref_t<T>>;

namespace detail
{

// Builds a component or a delta evaluator the way an app does: from the Input
// when it takes one, default-constructed otherwise.
template<class T, class Input>
[[nodiscard]] T make_from_input(const Input& input)
{
    if constexpr (std::constructible_from<T, const Input&>)
    {
        return T{input};
    }
    else
    {
        static_assert(
            std::default_initializable<T>,
            "EasyLocal testing cannot construct this type from the Input; "
            "construct it yourself and pass the object to the check");
        return T{};
    }
}

template<class Fixture>
bool check_fixture_solution(check_report& report, const Fixture& fixture)
{
    const auto valid =
        static_cast<bool>(fixture.solution_manager().is_valid(fixture.solution()));
    report.check(valid, "fixture solution", "the fixture Solution is not valid");
    return valid;
}

template<class NHE, class SM>
[[nodiscard]] NHE make_neighborhood(const SM& solution_manager)
{
    static_assert(
        std::constructible_from<NHE, const SM&>,
        "EasyLocal testing cannot construct the NeighborhoodExplorer from the "
        "SolutionManager; construct it yourself and pass the object to the check");
    return NHE{solution_manager};
}

} // namespace detail

} // namespace easylocal::testing
