// solvers::Pipeline: stages in sequence, their targets and attempts, the
// effort and report of each stage, cancellation and parameters.
#include <easylocal/cost.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/solvers.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

struct Instance
{
};

struct Solution
{
    int hard{};
    int soft{};
};

class SolutionManager
{
public:
    using input_type = Instance;
    using solution_type = Solution;

    explicit SolutionManager(const Instance& instance) : instance_{instance} {}

    [[nodiscard]]
    const Instance& input() const noexcept
    {
        return instance_;
    }

    [[nodiscard]]
    static bool is_valid(const Solution&) noexcept
    {
        return true;
    }

    [[nodiscard]]
    static Solution initial_solution()
    {
        return {.hard = 5, .soft = 9};
    }

private:
    const Instance& instance_;
};

struct HardPart
{
    [[nodiscard]]
    static int evaluate(const Solution& solution)
    {
        return solution.hard;
    }
};

struct SoftPart
{
    [[nodiscard]]
    static int evaluate(const Solution& solution)
    {
        return solution.soft;
    }
};

struct Move
{
};

class Neighborhood
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    explicit Neighborhood(const SolutionManager& sm) : sm_{sm} {}

    [[nodiscard]]
    const Instance& input() const noexcept
    {
        return sm_.input();
    }

    [[nodiscard]]
    static bool is_valid(const Solution&, const Move&) noexcept
    {
        return true;
    }

    static void make_move(Solution&, const Move&) noexcept {}

private:
    const SolutionManager& sm_;
};

// The result of the test algorithms: ten evaluations and one iteration a run.
template<class Cost>
struct Outcome
{
    Solution solution;
    Cost cost;
    std::size_t evaluations{10};
    std::size_t iterations{1};
    easylocal::termination_reason termination{easylocal::termination_reason::completed};
};

template<class Context>
auto outcome(const Context& context, const Solution& solution)
{
    return Outcome<typename Context::cost_type>{
        .solution = solution,
        .cost = context.evaluation().evaluate(solution).cost(),
    };
}

// The k-th run lowers the hard part by 2k: from 5, runs 1, 2 and 3 leave 3, 1
// and 0.
struct Countdown
{
    std::shared_ptr<int> runs = std::make_shared<int>(0);

    template<class Context>
    auto run(const Context& context, Solution solution) const
    {
        ++*runs;
        solution.hard = std::max(0, solution.hard - 2 * *runs);
        return outcome(context, solution);
    }
};

// Lowers the soft part by 4.
struct SoftDown
{
    template<class Context>
    auto run(const Context& context, Solution solution) const
    {
        solution.soft = std::max(0, solution.soft - 4);
        return outcome(context, solution);
    }
};

// Sets the soft part to 7, 3, 5, 6 in its successive runs.
struct Noisy
{
    std::shared_ptr<int> runs = std::make_shared<int>(0);

    template<class Context>
    auto run(const Context& context, Solution solution) const
    {
        static constexpr int values[] = {7, 3, 5, 6};
        solution.soft = values[(*runs)++ % 4];
        return outcome(context, solution);
    }
};

bool expect(const bool condition, const std::string_view message)
{
    if (!condition)
        std::cerr << "FAILED: " << message << '\n';
    return condition;
}

} // namespace

int main()
{
    namespace el = easylocal;
    namespace solvers = easylocal::solvers;

    const auto sm = el::solution_manager<SolutionManager>()
        | el::cost::hard_soft(el::component<HardPart>(), el::component<SoftPart>());
    const auto nhe = el::neighborhood<Neighborhood>();
    const Instance instance{};
    bool ok = true;

    // Three stages: the first on the hard cost until it is zero, in up to five
    // attempts from new initial solutions; the others from its solution.
    const Countdown countdown;
    auto three = solvers::pipeline()
        | solvers::stage("feasible", el::Runner{countdown} | sm | nhe)
              .until_feasible()
              .attempts(5)
        | solvers::stage("polish", el::Runner{SoftDown{}} | sm | nhe)
        | solvers::stage("finish", el::Runner{SoftDown{}} | sm | nhe);
    static_assert(decltype(three)::stage_count == 3);
    static_assert(
        std::same_as<std::remove_cvref_t<decltype(*three.stage<0>().target())>, int>);

    const auto result = three.initialization(el::initialization::initial).solve(instance);
    ok &= expect(*countdown.runs == 3, "the first stage stops at its target");
    ok &= expect(
        result.solution.hard == 0 && result.solution.soft == 1,
        "each stage starts from the solution of the previous one");
    ok &= expect(
        result.cost.hard() == 0 && result.cost.soft() == 1,
        "the result has the last stage's cost");
    ok &= expect(
        result.evaluations == 50 && result.iterations == 5,
        "the result has the effort of every stage and attempt");
    ok &= expect(result.stages.size() == 3, "one report per stage");
    ok &= expect(
        result.stages[0].name == "feasible" && result.stages[0].attempts == 3
            && result.stages[0].evaluations == 30 && result.stages[0].cost == "0",
        "the first stage's report");
    ok &= expect(
        result.stages[2].name == "finish" && result.stages[2].attempts == 1
            && result.stages[2].cost == "[0, 1]"
            && result.stages[2].termination == el::termination_reason::completed,
        "the last stage's report");

    // Without a target, a stage runs all its attempts and keeps the best.
    const Noisy noisy;
    auto best_of = solvers::pipeline(
        solvers::stage("noisy", el::Runner{noisy} | sm | nhe).attempts(4));
    const auto best = best_of.solve(instance);
    ok &= expect(
        *noisy.runs == 4 && best.solution.soft == 3 && best.stages[0].attempts == 4,
        "a stage without a target keeps its best attempt");

    // pipeline(a, b) is pipeline() | a | b.
    const auto shortcut = solvers::pipeline(
        solvers::stage("polish", el::Runner{SoftDown{}} | sm | nhe),
        solvers::stage("finish", el::Runner{SoftDown{}} | sm | nhe));
    const auto chained = solvers::pipeline()
        | solvers::stage("polish", el::Runner{SoftDown{}} | sm | nhe)
        | solvers::stage("finish", el::Runner{SoftDown{}} | sm | nhe);
    static_assert(std::same_as<decltype(shortcut), decltype(chained)>);

    // A cancelled solve makes one attempt per stage, and the solve's own
    // target goes to the last stage.
    std::stop_source stop;
    stop.request_stop();
    const el::run_control stopped{stop.get_token()};
    const Countdown cancelled_countdown;
    auto cancellable = solvers::pipeline(
        solvers::stage("feasible", el::Runner{cancelled_countdown} | sm | nhe)
            .until_feasible()
            .attempts(5),
        solvers::stage("polish", el::Runner{SoftDown{}} | sm | nhe));
    const auto cancelled = cancellable.solve(
        instance,
        el::with(stopped).stop_at(el::cost::hierarchical{0, 0}));
    ok &= expect(
        *cancelled_countdown.runs == 1 && cancelled.stages[0].attempts == 1,
        "after a cancellation a stage makes no further attempt");

    // The parameters: each stage's own, and its runner's, under its name.
    auto annealing =
        el::make_runner<
            el::runners::SimulatedAnnealing<el::runners::temperature::FixedLength>>(
            {.temperature =
                    el::runners::temperature::FixedLengthParameters{
                        .initial_temperature = 2.0,
                        .final_temperature = 0.5,
                        .cooling_rate = 0.5,
                        .max_iterations = 32,
                    }})
        | sm | nhe;
    auto configurable = solvers::pipeline(
        solvers::stage("anneal", annealing).attempts(2),
        solvers::stage("polish", el::Runner{SoftDown{}} | sm | nhe));
    const auto parameters = configurable.configuration();
    std::vector<std::string> paths;
    for (const auto& parameter : parameters.parameters())
        paths.push_back(parameter.path);
    const auto has = [&paths](const std::string_view path) {
        return std::ranges::find(paths, path) != paths.end();
    };
    ok &= expect(
        has("anneal.attempts") && has("polish.attempts"),
        "every stage has its attempts");
    ok &= expect(
        std::ranges::any_of(
            paths,
            [](const std::string& path) { return path.starts_with("anneal.search."); }),
        "a stage's runner has its parameters under the stage's name");
    const auto attempts_override = el::config::apply_overrides(
        parameters,
        std::vector<el::config::text_override>{{"anneal.attempts", "3"}});
    ok &= expect(
        attempts_override && configurable.stage<0>().parameters().attempts == 3,
        "the attempts are set through the parameters");

    // Stage names are distinct and not empty; a stage has at least one attempt.
    auto twins = solvers::pipeline(
        solvers::stage("same", el::Runner{SoftDown{}} | sm | nhe),
        solvers::stage("same", el::Runner{SoftDown{}} | sm | nhe));
    bool rejected = false;
    try
    {
        static_cast<void>(twins.configuration());
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    ok &= expect(rejected, "two stages with the same name are rejected");
    rejected = false;
    try
    {
        static_cast<void>(
            solvers::stage("none", el::Runner{SoftDown{}} | sm | nhe).attempts(0));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    ok &= expect(rejected, "a stage without attempts is rejected");

    return ok ? 0 : 1;
}
