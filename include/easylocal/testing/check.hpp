#pragma once

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

    [[nodiscard]] auto subject() const noexcept -> std::string_view
    {
        return subject_;
    }

    [[nodiscard]] auto passed() const noexcept -> bool
    {
        return failures_.empty();
    }

    [[nodiscard]] auto checks() const noexcept -> std::size_t
    {
        return checks_;
    }

    [[nodiscard]] auto failures() const noexcept
        -> std::span<const check_failure>
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

template<class... Reports>
    requires (std::same_as<std::remove_cvref_t<Reports>, check_report> && ...)
auto run_checks(std::ostream& out, Reports&&... reports) -> int
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
    requires (std::same_as<std::remove_cvref_t<Reports>, check_report> && ...)
auto run_checks(Reports&&... reports) -> int
{
    return run_checks(std::cerr, std::forward<Reports>(reports)...);
}

namespace detail
{

template<class Test, bool HasExplicitSM = requires {
    typename Test::solution_manager;
}>
struct test_solution_manager;

template<class Test>
struct test_solution_manager<Test, true>
{
    using type = typename Test::solution_manager;
};

template<class Test>
struct test_solution_manager<Test, false>
{
    using type = typename Test::neighborhood::solution_manager_type;
};

template<class Test>
using test_solution_manager_t = typename test_solution_manager<Test>::type;

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
            "EasyLocal testing cannot construct the SolutionManager; "
            "provide Test::make_solution_manager(instance)");
        return SM{instance};
    }
}

template<class Test, class Instance, class Component>
[[nodiscard]] auto make_component(Instance& instance)
{
    if constexpr (requires { Test::make_component(instance); })
    {
        auto component = Test::make_component(instance);
        static_assert(std::same_as<
            std::remove_cvref_t<decltype(component)>,
            Component>);
        return component;
    }
    else if constexpr (std::constructible_from<Component, const Instance&>)
    {
        return Component{instance};
    }
    else
    {
        static_assert(
            std::default_initializable<Component>,
            "EasyLocal testing cannot construct the cost component; provide "
            "Test::make_component(instance)");
        return Component{};
    }
}

template<class Test, class SM, class NHE>
[[nodiscard]] auto make_neighborhood(SM& solution_manager)
{
    if constexpr (requires { Test::make_neighborhood(solution_manager); })
    {
        auto neighborhood = Test::make_neighborhood(solution_manager);
        static_assert(std::same_as<
            std::remove_cvref_t<decltype(neighborhood)>,
            NHE>);
        return neighborhood;
    }
    else
    {
        static_assert(
            std::constructible_from<NHE, SM&> ||
            std::constructible_from<NHE, const SM&>,
            "EasyLocal testing cannot construct the NeighborhoodExplorer; "
            "provide Test::make_neighborhood(solution_manager)");
        return NHE{solution_manager};
    }
}

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

template<class Test, class Instance, class DeltaEvaluator>
[[nodiscard]] auto make_delta_evaluator(Instance& instance)
{
    if constexpr (requires { Test::make_delta_evaluator(instance); })
    {
        auto evaluator = Test::make_delta_evaluator(instance);
        static_assert(std::same_as<
            std::remove_cvref_t<decltype(evaluator)>,
            DeltaEvaluator>);
        return evaluator;
    }
    else if constexpr (std::constructible_from<DeltaEvaluator, const Instance&>)
    {
        return DeltaEvaluator{instance};
    }
    else
    {
        static_assert(
            std::default_initializable<DeltaEvaluator>,
            "EasyLocal testing cannot construct the delta evaluator; provide "
            "Test::make_delta_evaluator(instance)");
        return DeltaEvaluator{};
    }
}

template<class Test, class Left, class Right>
[[nodiscard]] auto equivalent(const Left& lhs, const Right& rhs) -> bool
{
    if constexpr (requires { Test::equivalent(lhs, rhs); })
    {
        return static_cast<bool>(Test::equivalent(lhs, rhs));
    }
    else
    {
        static_assert(
            requires { { lhs == rhs } -> std::convertible_to<bool>; },
            "EasyLocal testing needs equality-comparable values; alternatively "
            "provide Test::equivalent(lhs, rhs)");
        return static_cast<bool>(lhs == rhs);
    }
}

} // namespace detail

} // namespace easylocal::testing
