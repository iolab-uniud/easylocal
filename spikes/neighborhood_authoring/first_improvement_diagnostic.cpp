#include "assignment_variants.hpp"

#include "../../examples/assignment/capacity_delta.hpp"
#include "../../examples/assignment/first_improvement.hpp"

#include <easylocal/runner.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace spike = easylocal::spike::neighborhood_authoring;
namespace assignment = easylocal::mwe::assignment;

namespace
{

inline void observe(const std::uint64_t value) noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    __asm__ __volatile__("" : : "r"(value));
#else
    volatile auto sink = value;
    (void)sink;
#endif
}

class RawCursorFirstImprovement
{
public:
    explicit RawCursorFirstImprovement(
        const assignment::FirstImprovementParameters parameters) noexcept
        : parameters_{parameters}
    {
    }

    template<class Context>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const
    {
        const auto& neighborhood = context.neighborhood_explorer();
        const auto evaluation = context.evaluation();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using neighborhood_type = typename Context::neighborhood_explorer_type;
        using move_type = typename neighborhood_type::move_type;
        using result_type =
            assignment::FirstImprovementResult<solution_type, cost_type>;

        auto current = evaluation.evaluate(solution);
        std::size_t evaluations = 1;

        while (true)
        {
            bool improved = false;
            move_type move{};

            if (neighborhood.first_move(solution, move))
            {
                do
                {
                    if (evaluations == parameters_.max_evaluations)
                    {
                        return result_type{
                            .solution = std::move(solution),
                            .cost = current.cost(),
                            .evaluations = evaluations,
                            .termination = assignment::FirstImprovementTermination::
                                evaluation_budget_exhausted,
                        };
                    }

                    auto candidate =
                        evaluation.after_move(solution, current, move);
                    ++evaluations;

                    if (context.better(candidate.cost(), current.cost()))
                    {
                        evaluation.accept(
                            solution,
                            current,
                            std::move(candidate));
                        improved = true;
                        break;
                    }
                }
                while (neighborhood.next_move(solution, move));
            }

            if (!improved)
            {
                return result_type{
                    .solution = std::move(solution),
                    .cost = current.cost(),
                    .evaluations = evaluations,
                    .termination =
                        assignment::FirstImprovementTermination::local_optimum,
                };
            }
        }
    }

private:
    assignment::FirstImprovementParameters parameters_;
};

class RangeReferenceFirstImprovement
{
public:
    explicit RangeReferenceFirstImprovement(
        const assignment::FirstImprovementParameters parameters) noexcept
        : parameters_{parameters}
    {
    }

    template<class Context>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const
    {
        const auto& neighborhood = context.neighborhood_explorer();
        const auto evaluation = context.evaluation();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using result_type =
            assignment::FirstImprovementResult<solution_type, cost_type>;

        auto current = evaluation.evaluate(solution);
        std::size_t evaluations = 1;

        while (true)
        {
            bool improved = false;

            for (const auto& move : neighborhood.moves(solution))
            {
                if (evaluations == parameters_.max_evaluations)
                {
                    return result_type{
                        .solution = std::move(solution),
                        .cost = current.cost(),
                        .evaluations = evaluations,
                        .termination = assignment::FirstImprovementTermination::
                            evaluation_budget_exhausted,
                    };
                }

                auto candidate =
                    evaluation.after_move(solution, current, move);
                ++evaluations;

                if (context.better(candidate.cost(), current.cost()))
                {
                    evaluation.accept(
                        solution,
                        current,
                        std::move(candidate));
                    improved = true;
                    break;
                }
            }

            if (!improved)
            {
                return result_type{
                    .solution = std::move(solution),
                    .cost = current.cost(),
                    .evaluations = evaluations,
                    .termination =
                        assignment::FirstImprovementTermination::local_optimum,
                };
            }
        }
    }

private:
    assignment::FirstImprovementParameters parameters_;
};

class ExplicitIteratorFirstImprovement
{
public:
    explicit ExplicitIteratorFirstImprovement(
        const assignment::FirstImprovementParameters parameters) noexcept
        : parameters_{parameters}
    {
    }

    template<class Context>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const
    {
        const auto& neighborhood = context.neighborhood_explorer();
        const auto evaluation = context.evaluation();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using result_type =
            assignment::FirstImprovementResult<solution_type, cost_type>;

        auto current = evaluation.evaluate(solution);
        std::size_t evaluations = 1;

        while (true)
        {
            bool improved = false;
            auto moves = neighborhood.moves(solution);
            auto iterator = moves.begin();
            const auto sentinel = moves.end();

            while (iterator != sentinel)
            {
                const auto move = *iterator;

                if (evaluations == parameters_.max_evaluations)
                {
                    return result_type{
                        .solution = std::move(solution),
                        .cost = current.cost(),
                        .evaluations = evaluations,
                        .termination = assignment::FirstImprovementTermination::
                            evaluation_budget_exhausted,
                    };
                }

                auto candidate =
                    evaluation.after_move(solution, current, move);
                ++evaluations;

                if (context.better(candidate.cost(), current.cost()))
                {
                    evaluation.accept(
                        solution,
                        current,
                        std::move(candidate));
                    improved = true;
                    break;
                }

                ++iterator;
            }

            if (!improved)
            {
                return result_type{
                    .solution = std::move(solution),
                    .cost = current.cost(),
                    .evaluations = evaluations,
                    .termination =
                        assignment::FirstImprovementTermination::local_optimum,
                };
            }
        }
    }

private:
    assignment::FirstImprovementParameters parameters_;
};

class DeferredAcceptFirstImprovement
{
public:
    explicit DeferredAcceptFirstImprovement(
        const assignment::FirstImprovementParameters parameters) noexcept
        : parameters_{parameters}
    {
    }

    template<class Context>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const
    {
        const auto& neighborhood = context.neighborhood_explorer();
        const auto evaluation = context.evaluation();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using result_type =
            assignment::FirstImprovementResult<solution_type, cost_type>;
        using candidate_type = typename decltype(evaluation)::candidate_type;

        auto current = evaluation.evaluate(solution);
        std::size_t evaluations = 1;

        while (true)
        {
            std::optional<candidate_type> improving_candidate;

            {
                for (const auto move : neighborhood.moves(solution))
                {
                    if (evaluations == parameters_.max_evaluations)
                    {
                        return result_type{
                            .solution = std::move(solution),
                            .cost = current.cost(),
                            .evaluations = evaluations,
                            .termination = assignment::FirstImprovementTermination::
                                evaluation_budget_exhausted,
                        };
                    }

                    auto candidate =
                        evaluation.after_move(solution, current, move);
                    ++evaluations;

                    if (context.better(candidate.cost(), current.cost()))
                    {
                        improving_candidate.emplace(std::move(candidate));
                        break;
                    }
                }
            }

            if (!improving_candidate.has_value())
            {
                return result_type{
                    .solution = std::move(solution),
                    .cost = current.cost(),
                    .evaluations = evaluations,
                    .termination =
                        assignment::FirstImprovementTermination::local_optimum,
                };
            }

            evaluation.accept(
                solution,
                current,
                std::move(*improving_candidate));
        }
    }

private:
    assignment::FirstImprovementParameters parameters_;
};

class DeferredAcceptReferenceFirstImprovement
{
public:
    explicit DeferredAcceptReferenceFirstImprovement(
        const assignment::FirstImprovementParameters parameters) noexcept
        : parameters_{parameters}
    {
    }

    template<class Context>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const
    {
        const auto& neighborhood = context.neighborhood_explorer();
        const auto evaluation = context.evaluation();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using result_type =
            assignment::FirstImprovementResult<solution_type, cost_type>;
        using candidate_type = typename decltype(evaluation)::candidate_type;

        auto current = evaluation.evaluate(solution);
        std::size_t evaluations = 1;

        while (true)
        {
            std::optional<candidate_type> improving_candidate;

            {
                for (const auto& move : neighborhood.moves(solution))
                {
                    if (evaluations == parameters_.max_evaluations)
                    {
                        return result_type{
                            .solution = std::move(solution),
                            .cost = current.cost(),
                            .evaluations = evaluations,
                            .termination = assignment::FirstImprovementTermination::
                                evaluation_budget_exhausted,
                        };
                    }

                    auto candidate =
                        evaluation.after_move(solution, current, move);
                    ++evaluations;

                    if (context.better(candidate.cost(), current.cost()))
                    {
                        improving_candidate.emplace(std::move(candidate));
                        break;
                    }
                }
            }

            if (!improving_candidate.has_value())
            {
                return result_type{
                    .solution = std::move(solution),
                    .cost = current.cost(),
                    .evaluations = evaluations,
                    .termination =
                        assignment::FirstImprovementTermination::local_optimum,
                };
            }

            evaluation.accept(
                solution,
                current,
                std::move(*improving_candidate));
        }
    }

private:
    assignment::FirstImprovementParameters parameters_;
};

[[nodiscard]]
auto termination_name(
    const assignment::FirstImprovementTermination termination)
    -> std::string_view
{
    switch (termination)
    {
    case assignment::FirstImprovementTermination::local_optimum:
        return "local-optimum";
    case assignment::FirstImprovementTermination::evaluation_budget_exhausted:
        return "budget";
    }

    return "unknown";
}

struct AssignmentBenchmarkCase
{
    assignment::Instance instance;
    assignment::Solution initial;
};

[[nodiscard]]
auto make_assignment_case(const std::uint64_t seed) -> AssignmentBenchmarkCase
{
    constexpr std::size_t jobs = 96;
    constexpr std::size_t machines = 8;

    AssignmentBenchmarkCase result{
        .instance = assignment::Instance{
            .demand = std::vector<assignment::quantity_type>(jobs),
            .capacity = std::vector<assignment::quantity_type>(machines),
        },
        .initial = assignment::Solution{
            .assignment = std::vector<assignment::machine_id>(jobs, 0),
        },
    };

    assignment::quantity_type total_demand = 0;
    for (std::size_t job = 0; job < jobs; ++job)
    {
        const auto demand = static_cast<assignment::quantity_type>(
            1 + ((job * 17ULL + seed) % 9ULL));
        result.instance.demand[job] = demand;
        total_demand += demand;
    }

    const auto base_capacity =
        (total_demand + static_cast<assignment::quantity_type>(machines) - 1) /
        static_cast<assignment::quantity_type>(machines);
    std::fill(
        result.instance.capacity.begin(),
        result.instance.capacity.end(),
        base_capacity + 3);

    return result;
}

template<class Algorithm, class ManagerRecipe, class NeighborhoodRecipe>
[[nodiscard]]
auto run_once(
    Algorithm algorithm,
    const assignment::Instance& instance,
    const assignment::Solution& initial,
    const ManagerRecipe& manager_recipe,
    const NeighborhoodRecipe& neighborhood_recipe)
{
    auto runner = easylocal::Runner{std::move(algorithm)}
                | manager_recipe
                | neighborhood_recipe;
    auto bound = runner.bind(instance);
    return bound.run(initial);
}

template<class LhsResult, class RhsResult>
[[nodiscard]]
auto same_result(const LhsResult& lhs, const RhsResult& rhs) -> bool
{
    return lhs.solution.assignment == rhs.solution.assignment &&
           lhs.cost == rhs.cost &&
           lhs.evaluations == rhs.evaluations &&
           lhs.termination == rhs.termination;
}

template<class Algorithm, class ManagerRecipe, class NeighborhoodRecipe, class Token>
void benchmark_variant(
    const std::string_view variant,
    Algorithm algorithm,
    const assignment::Instance& instance,
    const assignment::Solution& initial,
    const ManagerRecipe& manager_recipe,
    const NeighborhoodRecipe& neighborhood_recipe,
    Token token,
    const std::size_t target_evaluations,
    const std::size_t trials)
{
    auto runner = easylocal::Runner{std::move(algorithm)}
                | manager_recipe
                | neighborhood_recipe;
    auto bound = runner.bind(instance);

    const auto reference = bound.run(initial);
    const auto repetitions = std::max<std::size_t>(
        1,
        target_evaluations / reference.evaluations);

    for (std::size_t warmup = 0; warmup < 2; ++warmup)
    {
        const auto result = bound.run(initial);
        observe(token(result));
    }

    for (std::size_t trial = 0; trial < trials; ++trial)
    {
        std::uint64_t checksum = 0;
        const auto start = std::chrono::steady_clock::now();

        for (std::size_t repetition = 0;
             repetition < repetitions;
             ++repetition)
        {
            const auto result = bound.run(initial);
            checksum += token(result);
        }

        const auto stop = std::chrono::steady_clock::now();
        const auto elapsed_ns =
            std::chrono::duration<double, std::nano>(stop - start).count();
        const auto measured_evaluations =
            repetitions * reference.evaluations;
        observe(checksum);

        std::cout
            << variant << ','
            << trial << ','
            << repetitions << ','
            << reference.evaluations << ','
            << measured_evaluations << ','
            << elapsed_ns / static_cast<double>(measured_evaluations) << ','
            << elapsed_ns / static_cast<double>(repetitions) << ','
            << termination_name(reference.termination) << ','
            << checksum << '\n';
    }
}

[[nodiscard]]
auto parse_positive(
    const int argc,
    char** argv,
    const int index,
    const std::size_t fallback,
    const std::string_view name) -> std::size_t
{
    if (argc <= index)
    {
        return fallback;
    }

    std::size_t result = 0;
    const std::string_view text{argv[index]};
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), result);

    if (error != std::errc{} || end != text.data() + text.size() || result == 0)
    {
        std::cerr
            << "invalid " << name << " '" << text
            << "'; using " << fallback << '\n';
        return fallback;
    }

    return result;
}

[[nodiscard]]
auto parse_seed(
    const int argc,
    char** argv,
    const int index,
    const std::uint64_t fallback) -> std::uint64_t
{
    if (argc <= index)
    {
        return fallback;
    }

    std::uint64_t result = 0;
    const std::string_view text{argv[index]};
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), result);

    if (error != std::errc{} || end != text.data() + text.size())
    {
        std::cerr
            << "invalid seed '" << text
            << "'; using " << fallback << '\n';
        return fallback;
    }

    return result;
}

} // namespace

int main(int argc, char** argv)
{
    const auto target_evaluations =
        parse_positive(argc, argv, 1, 5'000'000, "target evaluations");
    const auto trials = parse_positive(argc, argv, 2, 5, "trials");
    const auto seed = parse_seed(argc, argv, 3, 123'456'789ULL);

    const auto benchmark_case = make_assignment_case(seed);
    const auto parameters = assignment::FirstImprovementParameters{
        .max_evaluations = 500'000,
    };

    const auto manager_recipe =
        easylocal::solution_manager<assignment::SolutionManager>()
        | easylocal::component<assignment::CapacityCostComponent>();
    const auto cursor_recipe =
        easylocal::neighborhood<spike::assignment::CursorNeighborhoodExplorer>()
        | easylocal::delta<
              assignment::CapacityCostComponent,
              assignment::ReassignCapacityDeltaEvaluator>();

    const auto token = [](const auto& result) {
        auto value = static_cast<std::uint64_t>(result.cost.template get<0>());
        value ^= static_cast<std::uint64_t>(result.cost.template get<1>()) << 17;
        value ^= static_cast<std::uint64_t>(result.evaluations) << 1;
        value ^= static_cast<std::uint64_t>(result.termination) << 33;
        if (!result.solution.assignment.empty())
        {
            value ^= static_cast<std::uint64_t>(
                result.solution.assignment.front() + 1) * 1'000'003ULL;
            value ^= static_cast<std::uint64_t>(
                result.solution.assignment.back() + 1) * 65'537ULL;
        }
        return value;
    };

    const auto reference = run_once(
        RawCursorFirstImprovement{parameters},
        benchmark_case.instance,
        benchmark_case.initial,
        manager_recipe,
        cursor_recipe);

    const auto verify = [&](const std::string_view name, auto algorithm) {
        const auto result = run_once(
            std::move(algorithm),
            benchmark_case.instance,
            benchmark_case.initial,
            manager_recipe,
            cursor_recipe);
        if (!same_result(reference, result))
        {
            std::cerr << "first-improvement diagnostic semantic mismatch: "
                      << name << '\n';
            std::exit(2);
        }
    };

    verify("range-current", assignment::FirstImprovement{parameters});
    verify("range-reference", RangeReferenceFirstImprovement{parameters});
    verify("explicit-iterator", ExplicitIteratorFirstImprovement{parameters});
    verify("range-deferred-accept", DeferredAcceptFirstImprovement{parameters});
    verify(
        "range-reference-deferred",
        DeferredAcceptReferenceFirstImprovement{parameters});

    std::cerr
        << "first-improvement diagnostic semantics agree: evaluations="
        << reference.evaluations
        << " termination=" << termination_name(reference.termination)
        << '\n';

    std::cout
        << "variant,trial,repetitions,evaluations_per_run,"
           "measured_evaluations,ns_per_evaluation,ns_per_run,termination,"
           "checksum\n";

    benchmark_variant(
        "raw-cursor",
        RawCursorFirstImprovement{parameters},
        benchmark_case.instance,
        benchmark_case.initial,
        manager_recipe,
        cursor_recipe,
        token,
        target_evaluations,
        trials);
    benchmark_variant(
        "range-current",
        assignment::FirstImprovement{parameters},
        benchmark_case.instance,
        benchmark_case.initial,
        manager_recipe,
        cursor_recipe,
        token,
        target_evaluations,
        trials);
    benchmark_variant(
        "range-reference",
        RangeReferenceFirstImprovement{parameters},
        benchmark_case.instance,
        benchmark_case.initial,
        manager_recipe,
        cursor_recipe,
        token,
        target_evaluations,
        trials);
    benchmark_variant(
        "explicit-iterator",
        ExplicitIteratorFirstImprovement{parameters},
        benchmark_case.instance,
        benchmark_case.initial,
        manager_recipe,
        cursor_recipe,
        token,
        target_evaluations,
        trials);
    benchmark_variant(
        "range-deferred-accept",
        DeferredAcceptFirstImprovement{parameters},
        benchmark_case.instance,
        benchmark_case.initial,
        manager_recipe,
        cursor_recipe,
        token,
        target_evaluations,
        trials);
    benchmark_variant(
        "range-reference-deferred",
        DeferredAcceptReferenceFirstImprovement{parameters},
        benchmark_case.instance,
        benchmark_case.initial,
        manager_recipe,
        cursor_recipe,
        token,
        target_evaluations,
        trials);

    return 0;
}
