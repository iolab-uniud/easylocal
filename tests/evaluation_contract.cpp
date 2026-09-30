#include <easylocal/runner.hpp>

#include <functional>
#include <iostream>
#include <ranges>
#include <string_view>
#include <utility>

namespace
{

struct Counters
{
    int first_full_evaluations = 0;
    int second_full_evaluations = 0;
    int first_delta_evaluations = 0;
    int second_delta_evaluations = 0;
    int aggregations = 0;
    int make_moves = 0;
};

struct Instance
{
};

struct Solution
{
    int value = 0;
};

struct Move
{
    int delta = 0;
};

class SolutionManager
{
public:
    using input_type = Instance;
    using solution_type = Solution;

    SolutionManager(
        const Instance& instance,
        Counters& counters) noexcept
        : instance_{instance},
          counters_{counters}
    {
    }

    [[nodiscard]]
    auto input() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]]
    static constexpr auto is_valid(const Solution&) noexcept -> bool
    {
        return true;
    }

private:
    const Instance& instance_;
    std::reference_wrapper<Counters> counters_;
};

class CountingAggregator
{
public:
    explicit CountingAggregator(Counters& counters) noexcept
        : counters_{counters}
    {
    }

    [[nodiscard]]
    auto operator()(const int first, const int second) const noexcept -> int
    {
        ++counters_.get().aggregations;
        return first + second;
    }

private:
    std::reference_wrapper<Counters> counters_;
};

class FirstComponent
{
public:
    using value_type = int;

    FirstComponent(
        const Instance&,
        Counters& counters) noexcept
        : counters_{counters}
    {
    }

    [[nodiscard]]
    auto evaluate(const Solution& solution) const noexcept -> value_type
    {
        ++counters_.get().first_full_evaluations;
        return solution.value;
    }

private:
    std::reference_wrapper<Counters> counters_;
};

class SecondComponent
{
public:
    using value_type = int;

    SecondComponent(
        const Instance&,
        Counters& counters) noexcept
        : counters_{counters}
    {
    }

    [[nodiscard]]
    auto evaluate(const Solution& solution) const noexcept -> value_type
    {
        ++counters_.get().second_full_evaluations;
        return 10 * solution.value;
    }

private:
    std::reference_wrapper<Counters> counters_;
};

class FirstDeltaEvaluator
{
public:
    FirstDeltaEvaluator(
        const Instance&,
        Counters& counters) noexcept
        : counters_{counters}
    {
    }

    [[nodiscard]]
    auto delta_evaluate(const Solution&, const Move& move) const noexcept -> int
    {
        ++counters_.get().first_delta_evaluations;
        return move.delta;
    }

private:
    std::reference_wrapper<Counters> counters_;
};

class SecondDeltaEvaluator
{
public:
    SecondDeltaEvaluator(
        const Instance&,
        Counters& counters) noexcept
        : counters_{counters}
    {
    }

    [[nodiscard]]
    auto delta_evaluate(const Solution&, const Move& move) const noexcept -> int
    {
        ++counters_.get().second_delta_evaluations;
        return 10 * move.delta;
    }

private:
    std::reference_wrapper<Counters> counters_;
};

class NeighborhoodExplorer
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    NeighborhoodExplorer(
        const SolutionManager& solution_manager,
        Counters& counters) noexcept
        : instance_{solution_manager.input()},
          counters_{counters}
    {
    }

    [[nodiscard]]
    auto input() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]]
    static auto moves(const Solution&) noexcept
    {
        return std::views::single(Move{.delta = 1});
    }

    [[nodiscard]] static auto is_valid(const Solution&, const Move&) noexcept -> bool { return true; }

    void make_move(Solution& solution, const Move& move) const noexcept
    {
        ++counters_.get().make_moves;
        solution.value += move.delta;
    }

private:
    const Instance& instance_;
    std::reference_wrapper<Counters> counters_;
};

struct ProbeResult
{
    Solution solution;
    int initial_cost;
    int candidate_cost;
    int current_cost;
};

template<class Candidate>
concept move_backed_candidate =
    requires(const Candidate& candidate)
    {
        candidate.move();
    };

template<class Candidate>
concept solution_backed_candidate =
    requires(Candidate& candidate)
    {
        candidate.solution();
    };

template<bool Materialized>
class ProbeOneMove
{
public:
    explicit ProbeOneMove(const bool accept) noexcept
        : accept_{accept}
    {
    }

    template<class Context>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const -> ProbeResult
    {
        const auto evaluation = context.evaluation();
        using candidate_type = typename decltype(evaluation)::candidate_type;

        static_assert(
            move_backed_candidate<candidate_type> == !Materialized);
        static_assert(
            solution_backed_candidate<candidate_type> == Materialized);

        auto current = evaluation.evaluate(solution);
        const auto initial_cost = current.cost();

        auto candidate = evaluation.evaluate_move(
            solution,
            current,
            Move{.delta = 1});
        const auto candidate_cost = candidate.cost();

        if (accept_)
        {
            evaluation.commit(solution, current, std::move(candidate));
        }

        return ProbeResult{
            .solution = std::move(solution),
            .initial_cost = initial_cost,
            .candidate_cost = candidate_cost,
            .current_cost = current.cost(),
        };
    }

private:
    bool accept_;
};

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }

    return true;
}

auto check_common_evaluation(
    const ProbeResult& result,
    const Counters& counters,
    const bool accepted,
    const std::string_view mode) -> bool
{
    bool ok = true;

    ok &= expect(
        result.initial_cost == 11,
        "initial evaluation aggregates both components");
    ok &= expect(
        result.candidate_cost == 22,
        "candidate evaluation aggregates the post-move component values");
    ok &= expect(
        result.current_cost == (accepted ? 22 : 11),
        "commit is the only operation that promotes candidate evaluation state");
    ok &= expect(
        result.solution.value == (accepted ? 2 : 1),
        "commit is the only operation that changes the incumbent solution");
    ok &= expect(
        counters.aggregations == 2,
        "initial and candidate evaluations each aggregate exactly once");

    if (!ok)
    {
        std::cerr << "  mode: " << mode << '\n';
    }

    return ok;
}

} // namespace

int main()
{
    using easylocal::Runner;
    using easylocal::aggregator;
    using easylocal::component;
    using easylocal::delta;
    using easylocal::neighborhood;
    using easylocal::solution_manager;

    bool ok = true;
    const Instance instance;

    for (const bool accepted : {false, true})
    {
        Counters counters;

        auto runner =
            Runner{ProbeOneMove<false>{accepted}}
            | (solution_manager<SolutionManager>(std::ref(counters))
               | component<FirstComponent>(std::ref(counters))
               | component<SecondComponent>(std::ref(counters))
               | aggregator(CountingAggregator{counters}))
            | (neighborhood<NeighborhoodExplorer>(std::ref(counters))
               | delta<FirstComponent, FirstDeltaEvaluator>(
                     std::ref(counters))
               | delta<SecondComponent, SecondDeltaEvaluator>(
                     std::ref(counters)));

        const auto result = runner.bind(instance).run(Solution{.value = 1});

        ok &= check_common_evaluation(
            result,
            counters,
            accepted,
            "all-delta");
        ok &= expect(
            counters.first_full_evaluations == 1 &&
                counters.second_full_evaluations == 1,
            "all-delta candidate does not invoke any full component evaluator");
        ok &= expect(
            counters.first_delta_evaluations == 1 &&
                counters.second_delta_evaluations == 1,
            "all-delta candidate invokes each matching delta exactly once");
        ok &= expect(
            counters.make_moves == (accepted ? 1 : 0),
            "all-delta candidate applies the move only when committed");
    }

    for (const bool accepted : {false, true})
    {
        Counters counters;

        auto runner =
            Runner{ProbeOneMove<true>{accepted}}
            | (solution_manager<SolutionManager>(std::ref(counters))
               | component<FirstComponent>(std::ref(counters))
               | component<SecondComponent>(std::ref(counters))
               | aggregator(CountingAggregator{counters}))
            | (neighborhood<NeighborhoodExplorer>(std::ref(counters))
               | delta<FirstComponent, FirstDeltaEvaluator>(
                     std::ref(counters)));

        const auto result = runner.bind(instance).run(Solution{.value = 1});

        ok &= check_common_evaluation(
            result,
            counters,
            accepted,
            "mixed delta/fallback");
        ok &= expect(
            counters.first_full_evaluations == 1,
            "mixed candidate keeps the delta-backed component incremental");
        ok &= expect(
            counters.second_full_evaluations == 2,
            "mixed candidate fully evaluates only the component without a delta");
        ok &= expect(
            counters.first_delta_evaluations == 1 &&
                counters.second_delta_evaluations == 0,
            "mixed candidate invokes only the attached delta evaluator");
        ok &= expect(
            counters.make_moves == 1,
            "mixed candidate materializes one Solution and commit never reapplies the move");
    }

    for (const bool accepted : {false, true})
    {
        Counters counters;

        auto runner =
            Runner{ProbeOneMove<true>{accepted}}
            | (solution_manager<SolutionManager>(std::ref(counters))
               | component<FirstComponent>(std::ref(counters))
               | component<SecondComponent>(std::ref(counters))
               | aggregator(CountingAggregator{counters}))
            | neighborhood<NeighborhoodExplorer>(std::ref(counters));

        const auto result = runner.bind(instance).run(Solution{.value = 1});

        ok &= check_common_evaluation(
            result,
            counters,
            accepted,
            "no-delta");
        ok &= expect(
            counters.first_full_evaluations == 2 &&
                counters.second_full_evaluations == 2,
            "no-delta candidate fully evaluates each component exactly once");
        ok &= expect(
            counters.first_delta_evaluations == 0 &&
                counters.second_delta_evaluations == 0,
            "no-delta candidate never invokes a delta evaluator");
        ok &= expect(
            counters.make_moves == 1,
            "no-delta candidate materializes one Solution and commit reuses it");
    }

    return ok ? 0 : 1;
}
