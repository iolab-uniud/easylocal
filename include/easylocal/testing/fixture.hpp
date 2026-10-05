#pragma once

/// \file
/// fixture: what every contract check runs on, an Input, a valid Solution and
/// the SolutionManager built on the Input, with the options (samples, move
/// limits, tolerance) and the comparison of the values.

#include <easylocal/cost/tolerance.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/testing/check.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <random>
#include <type_traits>
#include <utility>

namespace easylocal::testing
{

/// The comparison of the checks: values equal within a relative and an
/// absolute tolerance, `approximately{.relative = 1e-6, .absolute = 1e-9}`
/// (see cost::tolerance).
///
/// Floating-point values computed along two paths (a full evaluation, and the
/// value before a move plus its delta) differ by rounding errors: the checks
/// compare them within it, integers exactly, structured costs level by level,
/// other values with `==`.
using approximately = cost::tolerance;

/// The options of the contract checks: how many moves and solutions they try,
/// the seed of their random draws and the tolerance of their comparisons.
///
/// The randomized checks draw from a `std::mt19937_64` seeded with `seed`: the
/// same seed repeats the same draws, another seed tries other moves and
/// solutions.
struct check_options
{
    /// The random moves and solutions drawn per check (default 32).
    std::size_t random_samples{32};
    /// The enumerated moves visited per traversal (default 1024).
    std::size_t max_enumerated_moves{1024};
    /// The seed of the random draws (default the generator's default seed).
    std::uint64_t seed{std::mt19937_64::default_seed};
    /// The tolerance of the comparisons of floating-point values (default
    /// relative and absolute 1e-9); `{0, 0}` compares them exactly.
    approximately tolerance{};
};

/// The data every component check runs on: an Input, a valid Solution and the
/// SolutionManager built on the Input.
///
/// Values are compared with Equivalent: by default testing::approximately,
/// with the tolerance of the options, which forgives the rounding errors of
/// floating-point values and compares the others exactly; `std::equal_to<>`
/// compares every value with `==`.
///
/// The fixture owns the Input the SolutionManager refers to, so it can be
/// neither copied nor moved: construct it in place.
template<easylocal::base_solution_manager SM, class Equivalent = approximately>
class fixture
{
public:
    /// The SolutionManager under test.
    using solution_manager_type = SM;
    /// The Input of the SolutionManager.
    using input_type = typename SM::input_type;
    /// The Solution of the SolutionManager.
    using solution_type = typename SM::solution_type;

    /// From an Input, a Solution and the options of the checks.
    ///
    /// The checks verify that the Solution is valid.
    fixture(input_type input, solution_type solution, check_options options = {})
        : input_{std::move(input)},
          solution_manager_{input_},
          solution_{std::move(solution)},
          options_{options},
          equivalent_{comparison(options)}
    {
    }

    /// The Solution defaults to the SolutionManager's initial solution.
    explicit fixture(input_type input, check_options options = {})
        requires easylocal::has_initial_solution<SM>
        : input_{std::move(input)},
          solution_manager_{input_},
          solution_{solution_manager_.initial_solution()},
          options_{options},
          equivalent_{comparison(options)}
    {
    }

    /// Not copyable: its SolutionManager refers to its Input.
    fixture(const fixture&) = delete;
    /// Not copyable: its SolutionManager refers to its Input.
    fixture& operator=(const fixture&) = delete;

    /// The Input.
    [[nodiscard]] const input_type& input() const noexcept
    {
        return input_;
    }

    /// The SolutionManager built on the Input.
    [[nodiscard]] const SM& solution_manager() const noexcept
    {
        return solution_manager_;
    }

    /// The Solution.
    [[nodiscard]] const solution_type& solution() const noexcept
    {
        return solution_;
    }

    /// The options of the checks.
    [[nodiscard]] const check_options& options() const noexcept
    {
        return options_;
    }

    /// Whether `lhs` and `rhs` are equivalent according to Equivalent.
    template<class Left, class Right>
        requires std::predicate<const Equivalent&, const Left&, const Right&>
    [[nodiscard]] bool equivalent(const Left& lhs, const Right& rhs) const
    {
        return static_cast<bool>(equivalent_(lhs, rhs));
    }

private:
    // The comparison: the tolerance of the options for approximately, a
    // default Equivalent otherwise.
    [[nodiscard]]
    static Equivalent comparison(const check_options& options)
    {
        if constexpr (std::same_as<Equivalent, approximately>)
            return options.tolerance;
        else
            return Equivalent{};
    }

    input_type input_;
    SM solution_manager_;
    solution_type solution_;
    check_options options_;
    EASYLOCAL_NO_UNIQUE_ADDRESS Equivalent equivalent_;
};

namespace detail
{

template<class T>
inline constexpr bool is_fixture_v = false;

template<class SM, class Equivalent>
inline constexpr bool is_fixture_v<fixture<SM, Equivalent>> = true;

} // namespace detail

/// A testing::fixture, whatever its SolutionManager and comparison.
template<class T>
concept check_fixture = detail::is_fixture_v<std::remove_cvref_t<T>>;

namespace detail
{

// Builds a component or a delta cost component the way an app does: from the Input
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
