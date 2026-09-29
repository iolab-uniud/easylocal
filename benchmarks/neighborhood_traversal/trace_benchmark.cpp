#include "cost_components.hpp"
#include "capacity_delta.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/aggregation.hpp>
#include <easylocal/runner.hpp>
#include <easylocal/search/first_improvement.hpp>
#include <easylocal/trace.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <utility>
#include <vector>

namespace assignment = easylocal::mwe::assignment;

namespace
{

struct counting_tracer
{
    template<class Event>
    static constexpr bool observes = true;

    template<class Event>
    void emit(const Event&) noexcept
    {
        ++events;
    }

    std::uint64_t events{};
};

template<class Function>
auto measure(Function&& function, std::size_t repetitions)
{
    const auto start = std::chrono::steady_clock::now();
    std::uint64_t checksum = 0;
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
    {
        checksum += static_cast<std::uint64_t>(function());
    }
    const auto stop = std::chrono::steady_clock::now();
    const auto ns = std::chrono::duration<double, std::nano>(stop - start).count();
    return std::pair{ns, checksum};
}

} // namespace

int main()
{
    constexpr std::size_t jobs = 96;
    constexpr std::size_t machines = 8;
    constexpr std::size_t repetitions = 64;

    assignment::AssignmentInstance instance{
        .demand = std::vector<assignment::quantity_type>(jobs),
        .capacity = std::vector<assignment::quantity_type>(machines, 64),
    };
    assignment::AssignmentSolution initial{
        .assignment = std::vector<assignment::machine_id>(jobs, 0),
    };
    for (std::size_t job = 0; job < jobs; ++job)
    {
        instance.demand[job] = static_cast<assignment::quantity_type>(1 + (job * 17) % 9);
    }

    auto runner = easylocal::Runner{
        easylocal::search::FirstImprovement{{.max_evaluations = 500'000}}}
        | (easylocal::solution_manager<assignment::AssignmentSolutionManager>()
           | easylocal::component<assignment::CapacityCostComponent>()
           | easylocal::aggregator([](const assignment::CapacityValue& capacity) {
                 return assignment::AssignmentCostAggregator{}.hard(capacity);
             }))
        | (easylocal::neighborhood<assignment::ReassignJobNeighborhoodExplorer>()
           | easylocal::delta<
                 assignment::CapacityCostComponent,
                 assignment::ReassignCapacityDeltaEvaluator>());
    auto bound = runner.bind(instance);

    const auto reference = bound.run(initial);
    using bound_type = std::remove_reference_t<decltype(bound)>;
    using cost_type = typename bound_type::cost_type;

    const auto baseline = measure([&] {
        const auto result = bound.run(initial);
        return result.evaluations;
    }, repetitions);

    easylocal::trace::null_tracer null;
    const auto explicit_null = measure([&] {
        const auto result = bound.run(initial, null);
        return result.evaluations;
    }, repetitions);

    counting_tracer counter;
    const auto counting = measure([&] {
        const auto result = bound.run(initial, counter);
        return result.evaluations;
    }, repetitions);

    const auto memory = measure([&] {
        easylocal::trace::memory_recorder<cost_type> recorder;
        const auto result = bound.run(initial, recorder);
        return result.evaluations + recorder.records().size();
    }, repetitions);

    const auto evaluations = static_cast<double>(reference.evaluations * repetitions);
    std::cout << "mode,ns_per_evaluation,checksum\n";
    std::cout << "baseline," << baseline.first / evaluations << ',' << baseline.second << '\n';
    std::cout << "explicit-null," << explicit_null.first / evaluations << ',' << explicit_null.second << '\n';
    std::cout << "counting," << counting.first / evaluations << ',' << counting.second << '\n';
    std::cout << "memory," << memory.first / evaluations << ',' << memory.second << '\n';
    std::cerr << "counted_events=" << counter.events << '\n';
    return baseline.second == explicit_null.second ? 0 : 2;
}
