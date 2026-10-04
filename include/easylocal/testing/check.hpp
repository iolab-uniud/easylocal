#pragma once

/// \file
/// The reporting side of the contract checks: check_report collects the
/// failures of a component's checks, run_checks prints the reports and gives
/// the exit code of a test program.
///
/// Also deterministic_rng for reproducible checks.

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

struct check_failure
{
    std::string check;
    std::string message;
};

class check_report
{
public:
    explicit check_report(std::string_view subject = "custom")
        : subject_{subject}
    {
    }

    [[nodiscard]] std::string_view subject() const noexcept
    {
        return subject_;
    }

    [[nodiscard]] bool passed() const noexcept
    {
        return failures_.empty();
    }

    [[nodiscard]] std::size_t checks() const noexcept
    {
        return checks_;
    }

    [[nodiscard]] std::span<const check_failure> failures() const noexcept
    {
        return failures_;
    }

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
    static constexpr result_type min() noexcept
    {
        return std::numeric_limits<result_type>::min();
    }

    [[nodiscard]]
    static constexpr result_type max() noexcept
    {
        return std::numeric_limits<result_type>::max();
    }

    result_type operator()() noexcept
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

template<class... Reports>
    requires(std::same_as<std::remove_cvref_t<Reports>, check_report> && ...)
int run_checks(Reports&&... reports)
{
    return run_checks(std::cerr, std::forward<Reports>(reports)...);
}

} // namespace easylocal::testing
