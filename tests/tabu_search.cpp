// Tabu Search on a line of positions 0..10, whose moves step left or right; a
// step forbids the opposite step while it is in the tabu list.
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/tabu_search.hpp>
#include <easylocal/trace.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string_view>
#include <variant>
#include <vector>

namespace
{

using namespace easylocal;
using namespace easylocal::runners;

struct LineInstance
{
};

struct Position
{
    int value{};
};

struct Step
{
    int delta{};
};

class LineManager
{
public:
    using input_type = LineInstance;
    using solution_type = Position;

    explicit LineManager(const LineInstance& instance) noexcept : instance_{instance} {}

    [[nodiscard]] auto input() const noexcept -> const LineInstance&
    {
        return instance_;
    }

    [[nodiscard]] static auto is_valid(const Position& position) noexcept -> bool
    {
        return position.value >= 0 && position.value <= 10;
    }

    // For the reactive list and the visited solutions; calls counts them.
    [[nodiscard]] static auto hash(const Position& position) noexcept -> std::uint64_t
    {
        ++calls;
        return static_cast<std::uint64_t>(position.value);
    }

    static inline std::size_t calls = 0;

private:
    const LineInstance& instance_;
};

// The left step first, then the right one. With every_move_tabu, every move
// is forbidden by any move in the list.
class LineExplorer
{
public:
    using input_type = LineInstance;
    using solution_type = Position;
    using move_type = Step;

    explicit LineExplorer(const LineManager& manager, const bool every_move_tabu = false)
        : instance_{manager.input()}, every_move_tabu_{every_move_tabu}
    {
    }

    [[nodiscard]] auto input() const noexcept -> const LineInstance&
    {
        return instance_;
    }

    [[nodiscard]] static auto is_valid(
        const Position& position,
        const Step& step) noexcept -> bool
    {
        return LineManager::is_valid(Position{position.value + step.delta});
    }

    static void make_move(Position& position, const Step& step) noexcept
    {
        position.value += step.delta;
    }

    // A generator, not a std::vector: GCC 15 at -O3 reports a spurious
    // free-nonheap-object on the vector's deallocation here.
    [[nodiscard]] static auto moves(const Position& position)
        -> easylocal::generator<Step>
    {
        for (const auto delta : {-1, +1})
            if (is_valid(position, Step{delta}))
                co_yield Step{delta};
    }

    // For the reactive list's escape.
    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Position& position, RNG& rng)
        -> std::optional<Step>
    {
        std::array<Step, 2> steps{};
        std::size_t count = 0;
        for (const auto step : moves(position))
            steps[count++] = step;
        if (count == 0)
            return std::nullopt;
        return steps[std::uniform_int_distribution<std::size_t>{0, count - 1}(rng)];
    }

    [[nodiscard]] auto inverse(const Position&, const Step& move, const Step& tabu_move)
        const -> bool
    {
        return every_move_tabu_ || move.delta == -tabu_move.delta;
    }

    // For the frequency list: the direction.
    [[nodiscard]] static auto tabu_attribute(const Step& move) noexcept -> int
    {
        return move.delta;
    }

private:
    const LineInstance& instance_;
    bool every_move_tabu_;
};

// The cost of each position, 0..10.
template<int... Costs>
struct Profile
{
    [[nodiscard]] static auto evaluate(const Position& position) noexcept -> int
    {
        constexpr std::array<int, sizeof...(Costs)> costs{Costs...};
        return costs[static_cast<std::size_t>(position.value)];
    }
};

// 1 is a local minimum (3, between 5 and 4); past the hill at 3, 5 has cost 0.
using Valley = Profile<5, 3, 4, 6, 2, 0, 7, 8, 9, 9, 9>;

template<class Algorithm, class Cost>
[[nodiscard]]
auto line_runner(
    const typename Algorithm::parameters_type& parameters,
    const bool every_move_tabu = false)
{
    return easylocal::make_runner<Algorithm>(parameters)
        | (solution_manager<LineManager>() | component<Cost>())
        | neighborhood<LineExplorer>(every_move_tabu);
}

// A fork: from 0 a move reaches 1, 2 or 3, whose costs in two objectives are
// (2, 2), (0, 5) and (3, 3); 0 costs (9, 9). (2, 2) and (0, 5) are unordered,
// as are (0, 5) and (3, 3), but (2, 2) dominates (3, 3).
class ForkManager
{
public:
    using input_type = LineInstance;
    using solution_type = Position;

    explicit ForkManager(const LineInstance& instance) noexcept : instance_{instance} {}

    [[nodiscard]] auto input() const noexcept -> const LineInstance&
    {
        return instance_;
    }

    [[nodiscard]] static auto is_valid(const Position& position) noexcept -> bool
    {
        return position.value >= 0 && position.value <= 3;
    }

private:
    const LineInstance& instance_;
};

class ForkExplorer
{
public:
    using input_type = LineInstance;
    using solution_type = Position;
    using move_type = Step;

    explicit ForkExplorer(const ForkManager& manager) : instance_{manager.input()} {}

    [[nodiscard]] auto input() const noexcept -> const LineInstance&
    {
        return instance_;
    }

    [[nodiscard]] static auto is_valid(const Position&, const Step& step) noexcept -> bool
    {
        return step.delta >= 1 && step.delta <= 3;
    }

    // From 0 only.
    static void make_move(Position& position, const Step& step) noexcept
    {
        position.value = step.delta;
    }

    [[nodiscard]] static auto moves(const Position& position)
        -> easylocal::generator<Step>
    {
        if (position.value == 0)
            for (const auto delta : {1, 2, 3})
                co_yield Step{delta};
    }

    [[nodiscard]] static auto inverse(const Position&, const Step&, const Step&) -> bool
    {
        return false;
    }

private:
    const LineInstance& instance_;
};

template<std::size_t Objective>
struct ForkObjective
{
    [[nodiscard]] static auto evaluate(const Position& position) noexcept -> int
    {
        constexpr std::array<std::array<int, 2>, 4> costs{
            {{9, 9}, {2, 2}, {0, 5}, {3, 3}}};
        return costs[static_cast<std::size_t>(position.value)][Objective];
    }
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

} // namespace

int main()
{
    bool ok = true;
    const LineInstance instance;

    {
        ok &= expect(
            static_cast<bool>(
                TabuSearchParameters<tabu::FixedLengthParameters>{}.validate())
                && !tabu::FixedLengthParameters{.tenure = 0}.validate()
                && !TabuSearchParameters<
                    tabu::FixedLengthParameters>{.max_idle_iterations = 0}
                    .validate(),
            "tabu search validates its parameters");

        TabuSearch<>::parameters_type parameters;
        easylocal::config::parameter_set configuration;
        configuration.add(parameters);
        ok &= expect(
            std::ranges::any_of(
                configuration.parameters(),
                [](const easylocal::config::parameter_info& parameter) {
                    return parameter.path == "tabu_list.tenure"
                        && parameter.value == "10";
                }),
            "the tabu list's parameters are the group tabu_list");
        const std::array invalid{
            easylocal::config::text_override{"tabu_list.tenure", "0"}};
        ok &= expect(
            !configuration.apply(invalid),
            "the tabu list's parameters are validated with the search's");
        const std::array valid{easylocal::config::text_override{"tabu_list.tenure", "3"}};
        ok &= expect(
            static_cast<bool>(configuration.apply(valid))
                && parameters.tabu_list.tenure == 3,
            "the tabu list's parameters can be changed");
        ok &= expect(
            std::ranges::none_of(
                configuration.parameters(),
                [](const easylocal::config::parameter_info& parameter) {
                    return parameter.path.starts_with("candidates");
                }),
            "the full scan of Tabu Search has no candidate parameters");
    }

    {
        // The candidate strategies share the block, under the group candidates.
        AspirationPlusTabuSearch<>::parameters_type parameters;
        easylocal::config::parameter_set configuration;
        configuration.add(parameters);
        const std::array changes{
            easylocal::config::text_override{"candidates.min_moves", "3"},
            easylocal::config::text_override{"max_iterations", "7"}};
        ok &= expect(
            static_cast<bool>(configuration.apply(changes))
                && parameters.candidates.min_moves == 3 && parameters.max_iterations == 7,
            "the parameters of a candidate strategy are the group candidates");
        static_assert(std::same_as<
            FirstImprovementTabuSearch<>::parameters_type,
            TabuSearchParameters<
                tabu::FixedLengthParameters,
                candidates::FirstImprovementParameters>>);
    }

    {
        // The cyclic list's tenures are a vector: [a, b, ...].
        TabuSearch<tabu::Cyclic>::parameters_type parameters;
        easylocal::config::parameter_set configuration;
        configuration.add(parameters);
        ok &= expect(
            std::ranges::any_of(
                configuration.parameters(),
                [](const easylocal::config::parameter_info& parameter) {
                    return parameter.path == "tabu_list.tenures"
                        && parameter.value == "[11, 34, 20, 8, 98]";
                }),
            "a vector parameter is written as [a, b, ...]");
        const std::array tenures{
            easylocal::config::text_override{"tabu_list.tenures", "[3, 4]"}};
        ok &= expect(
            static_cast<bool>(configuration.apply(tenures))
                && parameters.tabu_list.tenures == std::vector<std::size_t>{3, 4},
            "a vector parameter is read with any number of elements");
        const std::array empty{
            easylocal::config::text_override{"tabu_list.tenures", "[]"}};
        const std::array malformed{
            easylocal::config::text_override{"tabu_list.tenures", "[3, x]"}};
        ok &= expect(
            !configuration.apply(empty) && !configuration.apply(malformed)
                && parameters.tabu_list.tenures == std::vector<std::size_t>{3, 4},
            "an empty or malformed vector is rejected, leaving the parameters");
    }

    {
        // From 1: up to 2 (the best move, though worse), 3 (the way back is
        // tabu), 4 and 5 (cost 0); then 6, 7 and 8 without improving.
        auto runner = line_runner<TabuSearch<>, Valley>(
            {.max_idle_iterations = 3, .tabu_list = {.tenure = 2}});
        std::mt19937 rng{7U};
        const auto result = runner.bind(instance).run(Position{1}, rng);
        ok &= expect(
            result.solution.value == 5 && result.cost == 0,
            "tabu search leaves a local minimum and returns the best solution");
        ok &= expect(
            result.termination == termination_reason::idle_limit_reached
                && result.iterations == 7,
            "tabu search stops after max_idle_iterations without improving");

        auto bounded = line_runner<TabuSearch<>, Valley>(
            {.max_idle_iterations = 3, .max_iterations = 2, .tabu_list = {.tenure = 2}});
        const auto stopped = bounded.bind(instance).run(Position{1}, rng);
        ok &= expect(
            stopped.iterations == 2 && stopped.cost == 3
                && stopped.termination == termination_reason::completed,
            "tabu search stops after max_iterations");
    }

    {
        // Every move is tabu after the first: with aspiration by objective the
        // tabu moves are evaluated, and only improvements of the best are
        // admitted; without aspiration they are not evaluated, and the least
        // tabu move is applied.
        using Slope = Profile<10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0>;
        auto aspired =
            line_runner<TabuSearch<tabu::FixedLength, aspiration::ByObjective>, Slope>(
                {.max_idle_iterations = 2,
                    .max_iterations = 4,
                    .tabu_list = {.tenure = 3}},
                true);
        std::mt19937 rng{7U};
        const auto by_objective = aspired.bind(instance).run(Position{2}, rng);
        ok &= expect(
            by_objective.solution.value == 6 && by_objective.cost == 4,
            "aspiration admits tabu moves that improve the best cost");

        auto strict = line_runner<TabuSearch<tabu::FixedLength, aspiration::None>, Slope>(
            {.max_idle_iterations = 10, .max_iterations = 4, .tabu_list = {.tenure = 3}},
            true);
        const auto none = strict.bind(instance).run(Position{2}, rng);
        ok &= expect(
            none.iterations == 4 && none.evaluations < by_objective.evaluations,
            "without aspiration tabu moves are not evaluated, the least tabu is applied");

        // The three moves after the first were tabu and admitted by aspiration.
        std::mt19937 traced_rng{7U};
        easylocal::trace::memory_recorder<int> trace;
        const auto traced =
            aspired.bind(instance).run(Position{2}, traced_rng, easylocal::with(trace));
        std::size_t aspired_moves = 0;
        std::size_t visited = 0;
        for (const auto& record : trace.records())
        {
            using recorder = easylocal::trace::memory_recorder<int>;
            aspired_moves +=
                std::holds_alternative<recorder::aspiration_applied_record>(record);
            visited += std::holds_alternative<recorder::solution_visited_record>(record);
        }
        ok &= expect(
            aspired_moves == 3 && visited == traced.iterations + 1,
            "the trace records aspirated moves and every solution visited");
        // Each visit after the start comes from the one before it.
        std::vector<easylocal::trace::event::solution_visited<int>> visits;
        for (const auto& record : trace.records())
        {
            using recorder = easylocal::trace::memory_recorder<int>;
            if (const auto* visit =
                    std::get_if<recorder::solution_visited_record>(&record))
                visits.push_back(*visit);
        }
        bool linked = !visits.empty() && visits.front().previous_hash == 0;
        for (std::size_t index = 1; index < visits.size(); ++index)
            linked = linked && visits[index].previous_hash == visits[index - 1].hash;
        ok &= expect(linked, "a visit names the solution its move was applied to");

        // Without solution_visited the search computes no hash at all.
        std::mt19937 filtered_rng{7U};
        easylocal::trace::memory_recorder<int> filtered_trace;
        auto without_visits =
            easylocal::trace::without<easylocal::trace::event::solution_visited>(
                filtered_trace);
        LineManager::calls = 0;
        const auto filtered = aspired.bind(instance).run(
            Position{2},
            filtered_rng,
            easylocal::with(without_visits));
        const auto filtered_visits =
            std::ranges::count_if(filtered_trace.records(), [](const auto& record) {
                using recorder = easylocal::trace::memory_recorder<int>;
                return std::holds_alternative<recorder::solution_visited_record>(record);
            });
        ok &= expect(
            filtered.iterations == traced.iterations && filtered_visits == 0
                && LineManager::calls == 0 && filtered_trace.records().size() > 0,
            "a tracer without solution_visited costs no solution hash");
    }

    {
        // From 5 both steps reach cost 1: over several seeds both are chosen.
        using Twin = Profile<9, 9, 9, 9, 1, 3, 1, 9, 9, 9, 9>;
        bool left = false;
        bool right = false;
        for (unsigned seed = 0; seed < 32; ++seed)
        {
            auto runner = line_runner<TabuSearch<>, Twin>(
                {.max_iterations = 1, .tabu_list = {.tenure = 2}});
            std::mt19937 rng{seed};
            const auto result = runner.bind(instance).run(Position{5}, rng);
            left = left || result.solution.value == 4;
            right = right || result.solution.value == 6;
        }
        ok &= expect(left && right, "ties between the best moves are broken at random");
    }

    {
        // From 5, the left step improves (2) and comes first, the right one
        // improves more (0).
        using Uneven = Profile<9, 9, 9, 9, 2, 3, 0, 9, 9, 9, 9>;
        std::mt19937 rng{7U};
        const auto best =
            line_runner<TabuSearch<>, Uneven>(
                {.max_iterations = 1, .tabu_list = {.tenure = 2}})
                .bind(instance)
                .run(Position{5}, rng);
        const auto first =
            line_runner<FirstImprovementTabuSearch<>, Uneven>(
                {.max_iterations = 1, .tabu_list = {.tenure = 2}})
                .bind(instance)
                .run(Position{5}, rng);
        const auto on_best =
            line_runner<FirstImprovementTabuSearch<>, Uneven>(
                {.max_iterations = 1,
                    .tabu_list = {.tenure = 2},
                    .candidates = {.improve_on_best = true}})
                .bind(instance)
                .run(Position{5}, rng);
        ok &= expect(
            best.solution.value == 6 && first.solution.value == 4
                && first.evaluations == 2 && on_best.solution.value == 4,
            "first improvement stops the scan at the first improving move");

        // Aspiration plus: from 5 the left step (2) is under the level (the
        // best, 3); with plus 0 the scan stops there, with plus 1 it also
        // examines the right step (0).
        const auto stop_at_first =
            line_runner<AspirationPlusTabuSearch<>, Uneven>(
                {.max_iterations = 1,
                    .tabu_list = {.tenure = 2},
                    .candidates = {.min_moves = 1, .max_moves = 10, .plus = 0}})
                .bind(instance)
                .run(Position{5}, rng);
        const auto one_more =
            line_runner<AspirationPlusTabuSearch<>, Uneven>(
                {.max_iterations = 1,
                    .tabu_list = {.tenure = 2},
                    .candidates = {.min_moves = 1, .max_moves = 10, .plus = 1}})
                .bind(instance)
                .run(Position{5}, rng);
        const auto capped =
            line_runner<AspirationPlusTabuSearch<>, Uneven>(
                {.max_iterations = 1,
                    .tabu_list = {.tenure = 2},
                    .candidates = {.min_moves = 1, .max_moves = 1, .plus = 5}})
                .bind(instance)
                .run(Position{5}, rng);
        ok &= expect(
            stop_at_first.solution.value == 4 && one_more.solution.value == 6
                && capped.solution.value == 4,
            "aspiration plus examines plus moves after the first under the level, up to max_moves");

        // With a negative best cost (-6 at 5) the level stays above it: at
        // factor 1.5 it is -3, so the left step (-8) is under it and the scan
        // stops there; 1.5 * -6 = -9 would have skipped it for the right one.
        using NegativeUneven = Profile<-1, -1, -1, -1, -8, -6, -10, -1, -1, -1, -1>;
        const auto negative =
            line_runner<AspirationPlusTabuSearch<>, NegativeUneven>(
                {.max_iterations = 1,
                    .tabu_list = {.tenure = 2},
                    .candidates =
                        {.min_moves = 1,
                            .max_moves = 10,
                            .plus = 0,
                            .aspiration_level = 1.5}})
                .bind(instance)
                .run(Position{5}, rng);
        ok &= expect(
            negative.solution.value == 4 && negative.evaluations == 2,
            "the aspiration level is above a negative best cost");

        // From 1 nothing improves: the best admissible move is applied.
        const auto worse =
            line_runner<FirstImprovementTabuSearch<>, Valley>(
                {.max_iterations = 1, .tabu_list = {.tenure = 2}})
                .bind(instance)
                .run(Position{1}, rng);
        ok &= expect(
            worse.iterations == 1 && worse.evaluations == 3,
            "without an improving move first improvement scans the whole neighborhood");
    }

    {
        auto runner = line_runner<TabuSearch<>, Valley>(
            {.max_idle_iterations = 3, .tabu_list = {.tenure = 2}});
        std::mt19937 rng{7U};
        easylocal::trace::memory_recorder<int> trace;
        const auto result =
            runner.bind(instance).run(Position{1}, rng, easylocal::with(trace));
        std::size_t accepted = 0;
        std::size_t incumbents = 0;
        for (const auto& record : trace.records())
        {
            using recorder = easylocal::trace::memory_recorder<int>;
            accepted += std::holds_alternative<recorder::move_accepted_record>(record);
            incumbents +=
                std::holds_alternative<recorder::incumbent_updated_record>(record);
        }
        ok &= expect(
            accepted == result.iterations && incumbents == 2,
            "tabu search traces every applied move and the best-cost updates");

        const auto targeted =
            runner.bind(instance).run(Position{1}, rng, easylocal::stop_at(2));
        ok &= expect(
            targeted.termination == termination_reason::target_reached
                && targeted.cost == 2,
            "tabu search stops at a reached target");
    }

    {
        // Every list leaves the local minimum at 1 for the valley at 5.
        std::mt19937 rng{7U};
        const auto random_tenure =
            line_runner<TabuSearch<tabu::RandomTenure>, Valley>(
                {.max_idle_iterations = 3,
                    .tabu_list = {.min_tenure = 2, .max_tenure = 3}})
                .bind(instance)
                .run(Position{1}, rng);
        const auto cyclic =
            line_runner<TabuSearch<tabu::Cyclic>, Valley>(
                {.max_idle_iterations = 3, .tabu_list = {.period = 2, .tenures = {2, 3}}})
                .bind(instance)
                .run(Position{1}, rng);
        const auto reactive =
            line_runner<TabuSearch<tabu::Reactive>, Valley>(
                {.max_idle_iterations = 3, .tabu_list = {}})
                .bind(instance)
                .run(Position{1}, rng);
        ok &= expect(
            random_tenure.cost == 0 && cyclic.cost == 0 && reactive.cost == 0,
            "random tenure, cyclic and reactive lists leave the local minimum");

        const auto lim_dynamic =
            line_runner<TabuSearch<tabu::LimDynamic>, Valley>(
                {.max_idle_iterations = 3,
                    .tabu_list = {.min_tenure = 2, .max_tenure = 4, .idle_threshold = 2}})
                .bind(instance)
                .run(Position{1}, rng);
        const auto foo =
            line_runner<TabuSearch<tabu::Foo>, Valley>(
                {.max_idle_iterations = 3,
                    .tabu_list = {.window = 3, .increment = 2, .fluctuation = 1.0}})
                .bind(instance)
                .run(Position{1}, rng);
        const auto random_foo =
            line_runner<TabuSearch<tabu::RandomFoo>, Valley>(
                {.max_idle_iterations = 3,
                    .tabu_list = {.min_window = 2, .max_window = 4, .min_increment = 2}})
                .bind(instance)
                .run(Position{1}, rng);
        ok &= expect(
            lim_dynamic.cost == 0 && foo.cost == 0 && random_foo.cost == 0,
            "lim dynamic and fluctuation lists leave the local minimum");

        const auto objective =
            line_runner<TabuSearch<tabu::ObjectiveBased>, Valley>(
                {.max_idle_iterations = 5, .tabu_list = {.tenure = 2}})
                .bind(instance)
                .run(Position{1}, rng);
        ok &= expect(
            objective.cost <= 3
                && objective.termination == termination_reason::idle_limit_reached,
            "the objective-based list runs on the candidates' costs");

        const auto frequency =
            line_runner<TabuSearch<tabu::Frequency>, Valley>(
                {.max_idle_iterations = 5, .tabu_list = {.threshold = 0.6}})
                .bind(instance)
                .run(Position{1}, rng);
        ok &= expect(
            frequency.cost <= 3
                && frequency.termination == termination_reason::idle_limit_reached,
            "the frequency list runs on the moves' attributes");
    }

    {
        // A bowl around 5: the search runs to a border and bounces, revisiting
        // solutions; with no repetition allowed the first revisit escapes with
        // random moves, which cost one evaluation each instead of a scan.
        using Bowl = Profile<5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5>;
        auto runner = line_runner<TabuSearch<tabu::Reactive>, Bowl>(
            {.max_idle_iterations = 100,
                .max_iterations = 40,
                .tabu_list = {.repetitions = 0, .chaos = 0, .cycle_length = 100}});
        std::mt19937 rng{7U};
        easylocal::trace::memory_recorder<int> trace;
        const auto result =
            runner.bind(instance).run(Position{5}, rng, easylocal::with(trace));
        using recorder = easylocal::trace::memory_recorder<int>;
        std::size_t accepted = 0;
        std::size_t escapes = 0;
        // An escape is traced after its moves, with those applied: a scan
        // came first, and the iteration limit may cut the last one short.
        bool escapes_applied = true;
        for (const auto& record : trace.records())
        {
            accepted += std::holds_alternative<recorder::move_accepted_record>(record);
            if (const auto* escape = std::get_if<recorder::tabu_escape_record>(&record))
            {
                ++escapes;
                escapes_applied = escapes_applied && escape->moves > 0
                    && escape->iterations > escape->moves && escape->iterations <= 40;
            }
        }
        auto calm = line_runner<TabuSearch<tabu::Reactive>, Bowl>(
            {.max_idle_iterations = 100,
                .max_iterations = 40,
                .tabu_list = {.repetitions = 1000, .cycle_length = 100}});
        std::mt19937 calm_rng{7U};
        easylocal::trace::memory_recorder<int> calm_trace;
        const auto without_escape =
            calm.bind(instance).run(Position{5}, calm_rng, easylocal::with(calm_trace));

        // Without escapes the cycles make the tenure grow.
        std::vector<recorder::tabu_tenure_changed_record> tenures;
        for (const auto& record : calm_trace.records())
            if (const auto* change =
                    std::get_if<recorder::tabu_tenure_changed_record>(&record))
                tenures.push_back(*change);
        bool consistent = tenures.size() > 1 && tenures.front().previous_tenure == 0
            && tenures.front().tenure == 1 && tenures.front().iterations == 0;
        for (std::size_t index = 1; index < tenures.size(); ++index)
            consistent = consistent
                && tenures[index].previous_tenure == tenures[index - 1].tenure
                && tenures[index].tenure != tenures[index].previous_tenure;
        ok &= expect(consistent, "the tenure is traced at the start and at each change");
        ok &= expect(
            result.iterations == 40 && accepted == 40 && escapes > 0
                && result.evaluations < without_escape.evaluations && result.cost == 0,
            "the reactive list escapes with random moves, counted as iterations");
        ok &= expect(escapes_applied, "an escape is traced with the moves it applied");
    }

    {
        // With a pareto cost only equivalent candidates are ties: (0, 5) does
        // not replace (2, 2), so (3, 3), which (2, 2) dominates, is never
        // applied.
        auto runner = easylocal::make_runner<TabuSearch<>>({.max_iterations = 1})
            | (solution_manager<ForkManager>()
                | easylocal::cost::objectives(
                    component<ForkObjective<0>>(),
                    component<ForkObjective<1>>()))
            | neighborhood<ForkExplorer>();
        auto bound = runner.bind(instance);
        bool never_dominated = true;
        for (unsigned seed = 0; seed < 50; ++seed)
        {
            std::mt19937 rng{seed};
            const auto result = bound.run(Position{0}, rng);
            never_dominated = never_dominated && result.solution.value != 3;
        }
        ok &= expect(
            never_dominated,
            "tabu search applies the best candidate, not one dominated by it");
    }

    {
        // RandomFoo draws its first tenure at the first move: no tenure is
        // traced before it.
        auto runner = line_runner<TabuSearch<tabu::RandomFoo>, Valley>(
            {.max_idle_iterations = 3,
                .tabu_list = {.min_increment = 3, .max_increment = 3}});
        std::mt19937 rng{7U};
        easylocal::trace::memory_recorder<int> trace;
        (void)runner.bind(instance).run(Position{1}, rng, easylocal::with(trace));
        using recorder = easylocal::trace::memory_recorder<int>;
        std::vector<recorder::tabu_tenure_changed_record> tenures;
        for (const auto& record : trace.records())
            if (const auto* change =
                    std::get_if<recorder::tabu_tenure_changed_record>(&record))
                tenures.push_back(*change);
        ok &= expect(
            !tenures.empty() && tenures.front().previous_tenure == 0
                && tenures.front().tenure == 3 && tenures.front().iterations == 1,
            "random foo traces its first tenure when it draws it");
    }

    {
        // The line has a hash but no equality: verify_equality cannot work,
        // and the run throws before it starts, leaving no unfinished trace.
        auto runner = line_runner<TabuSearch<tabu::Reactive>, Valley>(
            {.max_idle_iterations = 3, .tabu_list = {.verify_equality = true}});
        std::mt19937 rng{7U};
        easylocal::trace::memory_recorder<int> trace;
        bool rejected = false;
        try
        {
            (void)runner.bind(instance).run(Position{1}, rng, easylocal::with(trace));
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        ok &= expect(
            rejected && trace.records().empty(),
            "a list that cannot run throws before the run starts");
    }

    return ok ? 0 : 1;
}
