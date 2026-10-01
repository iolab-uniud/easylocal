#include "assignment_variants.hpp"
#include "tsp_variants.hpp"

#include <easylocal/runners/best_improvement.hpp>
#include "../../examples/assignment/capacity_delta.hpp"
#include <easylocal/runners/first_improvement.hpp>
#include "../../examples/tsp/tour_length_component.hpp"
#include "../../examples/tsp/tour_length_delta.hpp"

#include <easylocal/runners/runner.hpp>

#include <algorithm>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace bench = easylocal::benchmark::neighborhood_traversal;
namespace assignment = easylocal::mwe::assignment;
namespace tsp = easylocal::mwe::tsp;
namespace runners = easylocal::runners;

namespace allocation_probe
{

inline bool tracking = false;
inline std::size_t allocation_count = 0;
inline std::size_t allocated_bytes = 0;

void reset() noexcept
{
    allocation_count = 0;
    allocated_bytes = 0;
}

void record(const std::size_t bytes) noexcept
{
    if (tracking)
    {
        ++allocation_count;
        allocated_bytes += bytes;
    }
}

} // namespace allocation_probe

void* operator new(const std::size_t size)
{
    allocation_probe::record(size);
    if (auto* memory = std::malloc(size == 0 ? 1 : size))
    {
        return memory;
    }
    throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

void* operator new[](const std::size_t size)
{
    allocation_probe::record(size);
    if (auto* memory = std::malloc(size == 0 ? 1 : size))
    {
        return memory;
    }
    throw std::bad_alloc{};
}

void operator delete[](void* memory) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept
{
    std::free(memory);
}

namespace
{

struct AllocationResult
{
    std::size_t allocations{};
    std::size_t bytes{};
};

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
        const runners::FirstImprovementParameters parameters) noexcept
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
            easylocal::search_result<solution_type, cost_type>;

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
                            .termination = easylocal::termination_reason::
                                evaluation_budget_exhausted,
                        };
                    }

                    auto candidate =
                        evaluation.evaluate_move(solution, current, move);
                    ++evaluations;

                    if (context.better(candidate.cost(), current.cost()))
                    {
                        evaluation.commit(
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
                        easylocal::termination_reason::local_optimum,
                };
            }
        }
    }

private:
    runners::FirstImprovementParameters parameters_;
};

class RawCursorBestImprovement
{
public:
    explicit RawCursorBestImprovement(
        const runners::BestImprovementParameters parameters) noexcept
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
            easylocal::search_result<solution_type, cost_type>;
        using candidate_type = typename decltype(evaluation)::candidate_type;

        auto current = evaluation.evaluate(solution);
        std::size_t evaluations = 1;

        while (true)
        {
            std::optional<candidate_type> best_candidate;
            auto best_cost = current.cost();
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
                            .termination = easylocal::termination_reason::
                                evaluation_budget_exhausted,
                        };
                    }

                    auto candidate =
                        evaluation.evaluate_move(solution, current, move);
                    ++evaluations;

                    if (context.better(candidate.cost(), best_cost))
                    {
                        best_cost = candidate.cost();
                        best_candidate = std::move(candidate);
                    }
                }
                while (neighborhood.next_move(solution, move));
            }

            if (!best_candidate.has_value())
            {
                return result_type{
                    .solution = std::move(solution),
                    .cost = current.cost(),
                    .evaluations = evaluations,
                    .termination =
                        easylocal::termination_reason::local_optimum,
                };
            }

            evaluation.commit(
                solution,
                current,
                std::move(*best_candidate));
        }
    }

private:
    runners::BestImprovementParameters parameters_;
};

[[nodiscard]]
auto termination_name(const easylocal::termination_reason termination)
    -> std::string_view
{
    switch (termination)
    {
    case easylocal::termination_reason::completed:
        return "completed";
    case easylocal::termination_reason::local_optimum:
        return "local-optimum";
    case easylocal::termination_reason::evaluation_budget_exhausted:
        return "budget";
    case easylocal::termination_reason::cancelled:
        return "cancelled";
    }

    return "unknown";
}

template<class LhsResult, class RhsResult, class SameSolution>
[[nodiscard]]
auto same_result(
    const LhsResult& lhs,
    const RhsResult& rhs,
    SameSolution same_solution) -> bool
{
    return same_solution(lhs.solution, rhs.solution) &&
           lhs.cost == rhs.cost &&
           lhs.evaluations == rhs.evaluations &&
           lhs.termination == rhs.termination;
}

template<
    class Algorithm,
    class Instance,
    class Solution,
    class ManagerRecipe,
    class NeighborhoodRecipe>
[[nodiscard]]
auto run_once(
    Algorithm algorithm,
    const Instance& instance,
    const Solution& initial,
    const ManagerRecipe& manager_recipe,
    const NeighborhoodRecipe& neighborhood_recipe)
{
    auto runner = easylocal::Runner{std::move(algorithm)}
                | manager_recipe
                | neighborhood_recipe;
    auto bound_runner = runner.bind(instance);
    return bound_runner.run(initial);
}

template<class BoundRunner, class Solution, class Token>
[[nodiscard]]
auto allocations_for_run(
    BoundRunner& bound_runner,
    const Solution& initial,
    Token token) -> AllocationResult
{
    allocation_probe::reset();
    allocation_probe::tracking = true;

    {
        const auto result = bound_runner.run(initial);
        observe(token(result));
    }

    allocation_probe::tracking = false;
    return AllocationResult{
        .allocations = allocation_probe::allocation_count,
        .bytes = allocation_probe::allocated_bytes,
    };
}

template<
    class Algorithm,
    class Instance,
    class Solution,
    class ManagerRecipe,
    class NeighborhoodRecipe,
    class Token>
void benchmark_variant(
    const std::string_view domain,
    const std::string_view algorithm_name,
    const std::string_view variant,
    Algorithm algorithm,
    const Instance& instance,
    const Solution& initial,
    const ManagerRecipe& manager_recipe,
    const NeighborhoodRecipe& neighborhood_recipe,
    Token token,
    const std::size_t target_evaluations,
    const std::size_t trials)
{
    auto runner = easylocal::Runner{std::move(algorithm)}
                | manager_recipe
                | neighborhood_recipe;
    auto bound_runner = runner.bind(instance);

    const auto reference = bound_runner.run(initial);
    const auto allocations = allocations_for_run(bound_runner, initial, token);
    const auto repetitions = std::max<std::size_t>(
        1,
        target_evaluations / reference.evaluations);

    for (std::size_t warmup = 0; warmup < 2; ++warmup)
    {
        const auto result = bound_runner.run(initial);
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
            const auto result = bound_runner.run(initial);
            checksum += token(result);
        }

        const auto stop = std::chrono::steady_clock::now();
        const auto elapsed_ns =
            std::chrono::duration<double, std::nano>(stop - start).count();
        const auto measured_evaluations =
            repetitions * reference.evaluations;
        observe(checksum);

        std::cout
            << domain << ','
            << algorithm_name << ','
            << variant << ','
            << trial << ','
            << repetitions << ','
            << reference.evaluations << ','
            << measured_evaluations << ','
            << allocations.allocations << ','
            << allocations.bytes << ','
            << elapsed_ns / static_cast<double>(measured_evaluations) << ','
            << elapsed_ns / static_cast<double>(repetitions) << ','
            << termination_name(reference.termination) << ','
            << checksum << '\n';
    }
}

template<
    class Instance,
    class Solution,
    class ManagerRecipe,
    class CursorRecipe,
    class CoroutineRecipe,
    class SameSolution,
    class Token>
void benchmark_search_case(
    const std::string_view domain,
    const Instance& instance,
    const Solution& initial,
    const ManagerRecipe& manager_recipe,
    const CursorRecipe& cursor_recipe,
    const CoroutineRecipe& coroutine_recipe,
    SameSolution same_solution,
    Token token,
    const std::size_t algorithm_budget,
    const std::size_t target_evaluations,
    const std::size_t trials)
{
    const auto check_algorithm = [&]<class RawAlgorithm, class RangeAlgorithm>(
        const std::string_view algorithm_name,
        RawAlgorithm raw_algorithm,
        RangeAlgorithm range_algorithm) {
        const auto raw = run_once(
            raw_algorithm,
            instance,
            initial,
            manager_recipe,
            cursor_recipe);
        const auto cursor_range = run_once(
            range_algorithm,
            instance,
            initial,
            manager_recipe,
            cursor_recipe);
        const auto coroutine_range = run_once(
            range_algorithm,
            instance,
            initial,
            manager_recipe,
            coroutine_recipe);

        if (!same_result(raw, cursor_range, same_solution) ||
            !same_result(raw, coroutine_range, same_solution))
        {
            std::cerr
                << "runner traversal semantic mismatch: "
                << domain << '/' << algorithm_name << '\n';
            std::exit(2);
        }

        std::cerr
            << "runner traversal semantics agree: "
            << domain << '/' << algorithm_name
            << " evaluations=" << raw.evaluations
            << " termination=" << termination_name(raw.termination)
            << '\n';
    };

    const auto first_parameters = runners::FirstImprovementParameters{
        .max_evaluations = algorithm_budget,
    };
    const auto best_parameters = runners::BestImprovementParameters{
        .max_evaluations = algorithm_budget,
    };

    check_algorithm(
        "first-improvement",
        RawCursorFirstImprovement{first_parameters},
        runners::FirstImprovement{first_parameters});
    check_algorithm(
        "best-improvement",
        RawCursorBestImprovement{best_parameters},
        runners::BestImprovement{best_parameters});

    const auto run_algorithm = [&]<class RawAlgorithm, class RangeAlgorithm>(
        const std::string_view algorithm_name,
        RawAlgorithm raw_algorithm,
        RangeAlgorithm range_algorithm) {
        benchmark_variant(
            domain,
            algorithm_name,
            "raw-cursor",
            raw_algorithm,
            instance,
            initial,
            manager_recipe,
            cursor_recipe,
            token,
            target_evaluations,
            trials);
        benchmark_variant(
            domain,
            algorithm_name,
            "cursor-range",
            range_algorithm,
            instance,
            initial,
            manager_recipe,
            cursor_recipe,
            token,
            target_evaluations,
            trials);
        benchmark_variant(
            domain,
            algorithm_name,
            "coroutine-range",
            range_algorithm,
            instance,
            initial,
            manager_recipe,
            coroutine_recipe,
            token,
            target_evaluations,
            trials);
    };

    run_algorithm(
        "first-improvement",
        RawCursorFirstImprovement{first_parameters},
        runners::FirstImprovement{first_parameters});
    run_algorithm(
        "best-improvement",
        RawCursorBestImprovement{best_parameters},
        runners::BestImprovement{best_parameters});
}

struct AssignmentBenchmarkCase
{
    assignment::AssignmentInstance instance;
    assignment::AssignmentSolution initial;
};

[[nodiscard]]
auto make_assignment_case(const std::uint64_t seed) -> AssignmentBenchmarkCase
{
    constexpr std::size_t jobs = 96;
    constexpr std::size_t machines = 8;

    AssignmentBenchmarkCase result{
        .instance = assignment::AssignmentInstance{
            .demand = std::vector<assignment::quantity_type>(jobs),
            .capacity = std::vector<assignment::quantity_type>(machines),
        },
        .initial = assignment::AssignmentSolution{
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

struct TspBenchmarkCase
{
    tsp::TspInstance instance;
    tsp::Tour initial;
};

[[nodiscard]]
auto make_tsp_case() -> TspBenchmarkCase
{
    constexpr std::size_t city_count = 64;

    TspBenchmarkCase result{
        .instance = tsp::TspInstance{
            .city_count = city_count,
            .distances = std::vector<tsp::distance_type>(
                city_count * city_count,
                0.0),
        },
        .initial = tsp::Tour{
            .tour = std::vector<tsp::city_id>(city_count),
        },
    };

    for (std::size_t first = 0; first < city_count; ++first)
    {
        for (std::size_t second = 0; second < city_count; ++second)
        {
            const auto difference = first > second
                ? first - second
                : second - first;
            result.instance.distances[first * city_count + second] =
                static_cast<tsp::distance_type>(difference);
        }
    }

    for (std::size_t position = 0; position < city_count; ++position)
    {
        result.initial.tour[position] = position % 2 == 0
            ? position / 2
            : city_count - 1 - position / 2;
    }

    return result;
}

void benchmark_assignment(
    const std::size_t target_evaluations,
    const std::size_t trials,
    const std::uint64_t seed)
{
    const auto benchmark_case = make_assignment_case(seed);

    const auto manager_recipe =
        easylocal::solution_manager<assignment::AssignmentSolutionManager>()
        | easylocal::component<assignment::CapacityCostComponent>()
        | easylocal::aggregator([](const assignment::CapacityValue& capacity) {
              return easylocal::cost::lexicographic{
                  capacity.total_overload,
                  capacity.overloaded_machines};
          });
    const auto cursor_recipe =
        easylocal::neighborhood<assignment::ReassignJobNeighborhoodExplorer>()
        | easylocal::delta<
              assignment::CapacityCostComponent,
              assignment::ReassignCapacityDeltaEvaluator>();
    const auto coroutine_recipe =
        easylocal::neighborhood<
            bench::assignment::CoroutineNeighborhoodExplorer>()
        | easylocal::delta<
              assignment::CapacityCostComponent,
              assignment::ReassignCapacityDeltaEvaluator>();

    const auto same_solution = [](const assignment::AssignmentSolution& lhs,
                                  const assignment::AssignmentSolution& rhs) {
        return lhs.assignment == rhs.assignment;
    };
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

    benchmark_search_case(
        "assignment",
        benchmark_case.instance,
        benchmark_case.initial,
        manager_recipe,
        cursor_recipe,
        coroutine_recipe,
        same_solution,
        token,
        500'000,
        target_evaluations,
        trials);
}

void benchmark_tsp(
    const std::size_t target_evaluations,
    const std::size_t trials)
{
    const auto benchmark_case = make_tsp_case();

    const auto manager_recipe =
        easylocal::solution_manager<tsp::TspSolutionManager>()
        | easylocal::component<tsp::TourLengthComponent>()
        | easylocal::aggregator([](const tsp::TourLengthValue& value) {
              return value.total;
          });
    const auto cursor_recipe =
        easylocal::neighborhood<tsp::TwoOptNeighborhoodExplorer>()
        | easylocal::delta<
              tsp::TourLengthComponent,
              tsp::TwoOptTourLengthDeltaEvaluator>();
    const auto coroutine_recipe =
        easylocal::neighborhood<bench::tsp::CoroutineNeighborhoodExplorer>()
        | easylocal::delta<
              tsp::TourLengthComponent,
              tsp::TwoOptTourLengthDeltaEvaluator>();

    const auto same_solution = [](const tsp::Tour& lhs,
                                  const tsp::Tour& rhs) {
        return lhs.tour == rhs.tour;
    };
    const auto token = [](const auto& result) {
        auto value = std::bit_cast<std::uint64_t>(result.cost);
        value ^= static_cast<std::uint64_t>(result.evaluations) << 1;
        value ^= static_cast<std::uint64_t>(result.termination) << 33;
        if (!result.solution.tour.empty())
        {
            value ^= static_cast<std::uint64_t>(
                result.solution.tour.front() + 1) * 1'000'003ULL;
            value ^= static_cast<std::uint64_t>(
                result.solution.tour.back() + 1) * 65'537ULL;
        }
        return value;
    };

    benchmark_search_case(
        "tsp",
        benchmark_case.instance,
        benchmark_case.initial,
        manager_recipe,
        cursor_recipe,
        coroutine_recipe,
        same_solution,
        token,
        500'000,
        target_evaluations,
        trials);
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
        parse_positive(argc, argv, 1, 2'000'000, "target evaluations");
    const auto trials = parse_positive(argc, argv, 2, 3, "trials");
    const auto seed = parse_seed(argc, argv, 3, 123'456'789ULL);

    std::cerr
        << "runner benchmark: target_evaluations=" << target_evaluations
        << " trials=" << trials
        << " seed=" << seed << '\n';

    std::cout
        << "domain,algorithm,variant,trial,repetitions,"
           "evaluations_per_run,measured_evaluations,allocations_per_run,"
           "allocated_bytes_per_run,ns_per_evaluation,ns_per_run,termination,"
           "checksum\n";

    benchmark_assignment(target_evaluations, trials, seed);
    benchmark_tsp(target_evaluations, trials);
    return 0;
}
