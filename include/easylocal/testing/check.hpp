#pragma once

/// \file
/// The reporting side of the contract checks: check_report collects the
/// failures of a component's checks, run_checks prints the reports and gives
/// the exit code of a test program.
///
/// Also deterministic_rng, for unit tests that script the random draws.

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

/// A failed check: its name and what went wrong.
struct check_failure
{
    /// The name of the check.
    std::string check;
    /// What went wrong.
    std::string message;
};

/// The outcome of the checks on a subject: how many ran and which failed.
class check_report
{
public:
    /// An empty report on `subject`, the name print_report() shows.
    explicit check_report(std::string_view subject = "custom")
        : subject_{subject}
    {
    }

    /// What the checks are about ("SolutionManager", "cost component"...).
    [[nodiscard]] std::string_view subject() const noexcept
    {
        return subject_;
    }

    /// Whether no check failed.
    [[nodiscard]] bool passed() const noexcept
    {
        return failures_.empty();
    }

    /// The number of checks run.
    [[nodiscard]] std::size_t checks() const noexcept
    {
        return checks_;
    }

    /// The failed checks.
    [[nodiscard]] std::span<const check_failure> failures() const noexcept
    {
        return failures_;
    }

    /// Counts a check named `check_name`, and records a failure with `message`
    /// when `condition` is false.
    void check(
        const bool condition,
        std::string_view check_name,
        std::string_view message)
    {
        ++checks_;
        if (!condition)
        {
            failures_.push_back(check_failure{
                .check = std::string{check_name},
                .message = std::string{message},
            });
        }
    }

private:
    std::string subject_;
    std::size_t checks_{};
    std::vector<check_failure> failures_;
};

/// Prints `report` to `out`: the number of checks passed, or each failure with
/// its message.
inline void print_report(
    std::ostream& out,
    const check_report& report)
{
    if (report.passed())
    {
        out << "EasyLocal " << report.subject() << " check: "
            << report.checks() << " checks passed\n";
        return;
    }

    out << "EasyLocal " << report.subject() << " check: "
        << report.failures().size() << " failure(s) in "
        << report.checks() << " checks\n";

    for (const auto& failure : report.failures())
    {
        out << "[fail] " << failure.check << "\n"
            << "       " << failure.message << "\n";
    }
}

/// A random bit generator that cycles through a fixed list of values, for unit
/// tests that script the random draws.
///
/// Not for sampling: with a few values, a random_move() that rejects draws
/// until one fits may never end. The contract checks draw from a seeded
/// `std::mt19937_64` (check_options::seed).
class deterministic_rng
{
public:
    /// The type of the generated values.
    using result_type = std::uint64_t;

    /// Cycles through four fixed values.
    deterministic_rng()
        : deterministic_rng({
              0x9e3779b97f4a7c15ULL,
              0xbf58476d1ce4e5b9ULL,
              0x94d049bb133111ebULL,
              0x123456789abcdef0ULL,
          })
    {
    }

    /// Cycles through `values` (a single 0 when empty).
    deterministic_rng(std::initializer_list<result_type> values)
        : values_{values}
    {
        if (values_.empty())
        {
            values_.push_back(0);
        }
    }

    /// The smallest value it can return.
    [[nodiscard]]
    static constexpr result_type min() noexcept
    {
        return std::numeric_limits<result_type>::min();
    }

    /// The largest value it can return.
    [[nodiscard]]
    static constexpr result_type max() noexcept
    {
        return std::numeric_limits<result_type>::max();
    }

    /// The next value of the list, back to the first after the last.
    result_type operator()() noexcept
    {
        const auto value = values_[index_];
        index_ = (index_ + 1) % values_.size();
        return value;
    }

    /// Restarts from the first value.
    void reset() noexcept
    {
        index_ = 0;
    }

private:
    std::vector<result_type> values_;
    std::size_t index_{};
};

static_assert(std::uniform_random_bit_generator<deterministic_rng>);

/// Prints the reports to `out`, separated by blank lines, and returns
/// `EXIT_SUCCESS` when all passed, `EXIT_FAILURE` otherwise.
template<class... Reports>
    requires(std::same_as<std::remove_cvref_t<Reports>, check_report> && ...)
int run_checks(std::ostream& out, Reports&&... reports)
{
    bool passed = true;
    bool first = true;

    auto print_one = [&](const check_report& report) {
        if (!first)
        {
            out << '\n';
        }
        first = false;
        print_report(out, report);
        passed = report.passed() && passed;
    };

    (print_one(reports), ...);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

/// Prints the reports to `std::cerr` and returns `EXIT_SUCCESS` when all
/// passed, `EXIT_FAILURE` otherwise.
template<class... Reports>
    requires(std::same_as<std::remove_cvref_t<Reports>, check_report> && ...)
int run_checks(Reports&&... reports)
{
    return run_checks(std::cerr, std::forward<Reports>(reports)...);
}

} // namespace easylocal::testing
