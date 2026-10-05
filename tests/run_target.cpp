// Target costs (run_options::stop_at, easylocal::stop_at), run options passed
// to solvers, and the effort solvers report.
#include <easylocal/cost.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/solvers.hpp>
#include <easylocal/trace/events.hpp>

#include <concepts>
#include <cstddef>
#include <iostream>
#include <optional>
#include <random>
#include <stop_token>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{

struct Countdown
{
    int start{};
};

struct Value
{
    int x{};
};

class CountdownManager
{
public:
    using input_type = Countdown;
    using solution_type = Value;

    explicit CountdownManager(const Countdown& input) : input_{input} {}

    [[nodiscard]] auto input() const noexcept -> const Countdown& { return input_; }
    [[nodiscard]] static auto is_valid(const Value& value) noexcept -> bool { return value.x >= 0; }
    [[nodiscard]] auto initial_solution() const -> Value { return {input_.start}; }

private:
    const Countdown& input_;
};

struct Size
{
    [[nodiscard]] static auto evaluate(const Value& value) -> int { return value.x; }
};

struct Decrement
{
    auto operator==(const Decrement&) const -> bool = default;
};

class DecrementExplorer
{
public:
    using input_type = Countdown;
    using solution_type = Value;
    using move_type = Decrement;

    explicit DecrementExplorer(const CountdownManager& sm) : sm_{sm} {}

    [[nodiscard]] auto input() const noexcept -> const Countdown& { return sm_.input(); }

    [[nodiscard]] static auto first_move(const Value& value, Decrement&) noexcept -> bool
    {
        return value.x > 0;
    }

    [[nodiscard]] static auto next_move(const Value&, Decrement&) noexcept -> bool
    {
        return false;
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Value& value, RNG&) -> std::optional<Decrement>
    {
        return value.x > 0 ? std::optional<Decrement>{Decrement{}} : std::nullopt;
    }

    [[nodiscard]] static auto is_valid(const Value& value, const Decrement&) noexcept -> bool
    {
        return value.x > 0;
    }

    static void make_move(Value& value, const Decrement&) noexcept { --value.x; }

private:
    const CountdownManager& sm_;
};

// Counts the runs a tracer sees.
struct RunCounter
{
    template<class Event>
    static constexpr bool observes = false;

    template<class Cost>
    static constexpr bool observes<easylocal::trace::event::run_started<Cost>> = true;

    std::size_t runs{};

    template<class Event>
    void emit(const Event&) noexcept
    {
        ++runs;
    }
};

// Whether with(control) and with(control, tracer) take a control of this kind:
// not a temporary, which the options would outlive.
template<class Control>
concept options_with_control = requires(Control&& control) {
    easylocal::with(std::forward<Control>(control));
};

template<class Control>
concept options_with_control_and_tracer =
    requires(Control&& control, easylocal::trace::null_tracer& tracer) {
        easylocal::with(std::forward<Control>(control), tracer);
    };

static_assert(options_with_control<const easylocal::run_control&>);
static_assert(!options_with_control<easylocal::run_control>);
static_assert(options_with_control_and_tracer<const easylocal::run_control&>);
static_assert(!options_with_control_and_tracer<easylocal::run_control>);

// Whether a run makes a run over a context of this kind: not a temporary,
// which the new run would outlive.
template<class Run, class Context>
concept run_with_context = requires(Run& run, Context&& context) {
    run.with_context(std::forward<Context>(context));
};

struct NoParameters
{
};

// First Improvement, checking the lifetimes its run accepts.
class LifetimeProbe
{
public:
    using parameters_type = NoParameters;

    explicit LifetimeProbe(NoParameters) {}

    template<class Run>
    auto run(Run& run, Run::solution_type solution) const
    {
        using context_type = typename Run::context_type;
        static_assert(run_with_context<Run, const context_type&>);
        static_assert(!run_with_context<Run, context_type>);
        static_assert(!std::constructible_from<
            Run,
            context_type,
            const easylocal::run_control&,
            typename Run::tracer_type&>);
        return easylocal::runners::FirstImprovement{
            easylocal::runners::FirstImprovementParameters{}}
            .run(run, std::move(solution));
    }
};

auto expect(bool condition, std::string_view message) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

} // namespace

int main()
{
    using namespace easylocal;
    using easylocal::termination_reason;

    static_assert(cost::zero<int>() == 0);
    static_assert(cost::zero<double>() == 0.0);
    static_assert(cost::zero<cost::lexicographic<int, long>>() == cost::lexicographic<int, long>{0, 0L});
    static_assert(
        cost::zero<cost::hierarchical<cost::lexicographic<int, int>, double>>() ==
        cost::hierarchical{cost::lexicographic<int, int>{0, 0}, 0.0});
    static_assert(cost::has_zero<int> && !cost::has_zero<std::stop_token&>);

    const auto sm = solution_manager<CountdownManager>() | component<Size>();
    const auto nhe = neighborhood<DecrementExplorer>();
    auto fi = make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{}) | sm | nhe;

    const Countdown ten{10};
    auto bound = fi.bind(ten);
    bool ok = true;

    // Runners.
    const auto unbounded = bound.run(Value{10});
    ok &= expect(unbounded.cost == 0, "without a target the run reaches the optimum");
    ok &= expect(unbounded.termination == termination_reason::local_optimum,
                 "without a target the run ends at the local optimum");

    const auto at_three = bound.run(Value{10}, stop_at(3));
    ok &= expect(at_three.cost == 3, "the run stops at the target cost");
    ok &= expect(at_three.termination == termination_reason::target_reached,
                 "a reached target is the termination reason");

    const auto at_zero = bound.run(Value{10}, stop_at(0));
    ok &= expect(at_zero.cost == 0 && at_zero.termination == termination_reason::target_reached,
                 "a target reached at a local optimum is still target_reached");

    const auto already = bound.run(Value{2}, stop_at(3));
    ok &= expect(already.evaluations == 1 && already.cost == 2 &&
                     already.termination == termination_reason::target_reached,
                 "a solution already at the target stops after the initial evaluation");

    RunCounter counter;
    const run_control no_stop{};
    const auto combined = bound.run(Value{10}, with(no_stop, counter).stop_at(5));
    ok &= expect(combined.cost == 5 && counter.runs == 1,
                 "with(control, tracer).stop_at(target) keeps control and tracer");

    auto sa =
        make_runner<runners::SimulatedAnnealing<runners::temperature::Classic>>({
            .temperature =
                {
                    .initial_temperature = 1.0,
                    .final_temperature = 0.001,
                    .cooling_rate = 0.999,
                    .samples_per_temperature = 1000,
                },
        })
        | sm | nhe;
    auto bound_sa = sa.bind(ten);
    std::mt19937_64 rng{1};
    const auto sa_result = bound_sa.run(Value{10}, rng, stop_at(0));
    ok &= expect(sa_result.cost == 0 && sa_result.termination == termination_reason::target_reached,
                 "Simulated Annealing stops at the target");
    ok &= expect(sa_result.evaluations < 100, "... instead of spending its whole schedule");

    auto probe = make_runner<LifetimeProbe>(NoParameters{}) | sm | nhe;
    ok &= expect(probe.bind(ten).run(Value{3}).cost == 0, "a delegating runner runs");

    // Solvers.
    auto local_search = make_solver<solvers::LocalSearch>(
        fi,
        solvers::LocalSearchConfig<initialization::Initial>{.initialization = initialization::initial});
    const auto solved = local_search.solve(ten, stop_at(4));
    ok &= expect(solved.cost == 4 && solved.termination == termination_reason::target_reached,
                 "LocalSearch passes the target to its run");

    std::stop_source stop;
    stop.request_stop();
    const run_control stopped{stop.get_token()};
    const auto cancelled = local_search.solve(ten, with(stopped));
    ok &= expect(cancelled.termination == termination_reason::cancelled && cancelled.evaluations == 1,
                 "LocalSearch is cancellable");

    auto multi_start = make_solver<solvers::MultiStart>(
        fi,
        solvers::MultiStartConfig<initialization::Initial>{
            .parameters = {.starts = 5},
            .initialization = initialization::initial,
        });
    const auto all_starts = multi_start.solve(ten);
    ok &= expect(all_starts.evaluations == 5 * unbounded.evaluations &&
                     all_starts.iterations == 5 * unbounded.iterations,
                 "MultiStart reports the effort of every start");
    ok &= expect(all_starts.termination == termination_reason::completed,
                 "MultiStart completes after all its starts");

    RunCounter starts;
    const auto first_hit = multi_start.solve(ten, with(no_stop, starts).stop_at(0));
    ok &= expect(first_hit.termination == termination_reason::target_reached && starts.runs == 1,
                 "MultiStart stops at the first start that reaches the target");

    const auto multi_cancelled = multi_start.solve(ten, with(stopped));
    ok &= expect(multi_cancelled.termination == termination_reason::cancelled &&
                     multi_cancelled.evaluations == 1,
                 "MultiStart does not start again after a cancellation");

    return ok ? 0 : 1;
}
