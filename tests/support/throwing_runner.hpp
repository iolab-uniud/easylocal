#pragma once

// A runner whose hooks fail in the middle of a run: it starts, makes
// fail_after random moves, accepted whatever their cost, then throws
// std::runtime_error with throwing::failure. And a counter to run it on,
// whose cost is its value, lowered by one at each move down to 0.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/utils/generator.hpp>
#include <easylocal/utils/limit.hpp>

#include <cstddef>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace throwing
{

// What the runner throws.
inline constexpr std::string_view failure = "the search broke mid-run";

struct ThrowingRunnerParameters
{
    // The moves made before the failure; unlimited: it never fails, and ends
    // at a solution without moves or when it is stopped.
    easylocal::limit fail_after{3};

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"fail_after", &ThrowingRunnerParameters::fail_after>(
                "Moves made before the run throws, or unlimited",
                easylocal::config::range(0, easylocal::unlimited)));
    }

    constexpr easylocal::config::validation_result validate() const noexcept
    {
        return easylocal::config::check_schema(*this);
    }
};

class ThrowingRunner
{
public:
    using parameters_type = ThrowingRunnerParameters;

    explicit ThrowingRunner(const ThrowingRunnerParameters& parameters)
        : parameters_{parameters}
    {
    }

    // Throws once it has made its moves, unless the run stopped or ran out of
    // moves first.
    template<class Run, std::uniform_random_bit_generator RNG>
        requires easylocal::runners::detail::random_move_context<
            typename Run::context_type,
            RNG>
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
        auto current = run.start(solution);
        std::size_t moves = 0;
        while (!run.should_stop())
        {
            if (moves >= parameters_.fail_after)
                throw std::runtime_error{std::string{failure}};
            auto move = run.random_move(solution, rng);
            if (!move.has_value())
            {
                return run.finish(
                    std::move(solution),
                    current.cost(),
                    easylocal::termination_reason::local_optimum);
            }
            run.next_iteration();
            auto candidate = run.evaluate_move(solution, current, *move);
            run.commit(solution, current, std::move(candidate), *move);
            ++moves;
        }
        return run.finish(std::move(solution), current.cost());
    }

private:
    ThrowingRunnerParameters parameters_;
};

struct Counter
{
    int start{10};
};

struct Count
{
    int value{};
};

class CounterManager
{
public:
    using input_type = Counter;
    using solution_type = Count;

    explicit CounterManager(const Counter& input) : input_{input} {}

    const Counter& input() const noexcept
    {
        return input_;
    }
    static bool is_valid(const Count& count) noexcept
    {
        return count.value >= 0;
    }
    Count initial_solution() const
    {
        return {input_.start};
    }

private:
    const Counter& input_;
};

struct CountValue
{
    static int evaluate(const Count& count)
    {
        return count.value;
    }
};

struct Decrement
{
    bool operator==(const Decrement&) const = default;
};

class DecrementExplorer
{
public:
    using input_type = Counter;
    using solution_type = Count;
    using move_type = Decrement;

    explicit DecrementExplorer(const CounterManager& sm) : sm_{sm} {}

    const Counter& input() const noexcept
    {
        return sm_.input();
    }
    static easylocal::generator<Decrement> moves(const Count& count)
    {
        if (count.value > 0)
            co_yield Decrement{};
    }
    template<std::uniform_random_bit_generator RNG>
    static std::optional<Decrement> random_move(const Count& count, RNG&)
    {
        if (count.value > 0)
            return Decrement{};
        return std::nullopt;
    }
    static bool is_valid(const Count& count, const Decrement&) noexcept
    {
        return count.value > 0;
    }
    static void make_move(Count& count, const Decrement&) noexcept
    {
        --count.value;
    }

private:
    const CounterManager& sm_;
};

// The throwing runner on the counter.
inline auto counter_runner(const ThrowingRunnerParameters& parameters = {})
{
    return easylocal::make_runner<ThrowingRunner>(parameters)
        | (easylocal::solution_manager<CounterManager>()
            | easylocal::component<CountValue>())
        | easylocal::neighborhood<DecrementExplorer>();
}

} // namespace throwing
