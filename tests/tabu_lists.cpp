// The tabu lists, through their states: a candidate forbidden by the moves of
// opposite sign, whose attribute is its absolute value, and steps that carry a
// move, an iteration and a solution hash.
#include <easylocal/runners/tabu_search.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <string_view>

namespace
{

using namespace easylocal::runners;

struct Candidate
{
    int move{};
    int value{};

    [[nodiscard]] auto cost() const -> const int&
    {
        return value;
    }

    [[nodiscard]] auto forbidden_by(const int tabu_move) const -> bool
    {
        return tabu_move == -move;
    }

    [[nodiscard]] auto attribute() const -> int
    {
        return std::abs(move);
    }
};

struct Step
{
    int applied{};
    std::size_t at{};
    std::uint64_t hash{};
    int value{};
    bool improved{};

    [[nodiscard]] auto cost() const -> const int&
    {
        return value;
    }

    [[nodiscard]] auto improved_best() const -> bool
    {
        return improved;
    }

    [[nodiscard]] auto move() const -> int
    {
        return applied;
    }

    [[nodiscard]] auto iteration() const -> std::size_t
    {
        return at;
    }

    [[nodiscard]] auto attribute() const -> int
    {
        return std::abs(applied);
    }

    [[nodiscard]] auto solution_hash() const -> std::uint64_t
    {
        return hash;
    }
};

// A run type with int moves and costs, for make_state<Run>().
struct IntRun
{
    using move_type = int;
    using cost_type = int;
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

[[nodiscard]]
auto tenure(const auto& state, const int move) -> std::optional<std::size_t>
{
    return state.tabu_tenure(Candidate{move});
}

} // namespace

int main()
{
    bool ok = true;
    std::mt19937 rng{7U};

    {
        auto state = tabu::FixedLength{{.tenure = 2}}.make_state<IntRun>();
        state.update(Step{.applied = 1, .at = 1}, rng);
        ok &= expect(
            tenure(state, -1) == 2 && !tenure(state, 1).has_value(),
            "fixed length: a move forbids its inverse for tenure iterations");
        state.update(Step{.applied = 5, .at = 2}, rng);
        ok &= expect(tenure(state, -1) == 1, "fixed length: the tenure left decreases");
        state.update(Step{.applied = 6, .at = 3}, rng);
        ok &= expect(
            !tenure(state, -1).has_value(),
            "fixed length: the oldest move leaves");
    }

    {
        auto state =
            tabu::RandomTenure{{.min_tenure = 3, .max_tenure = 3}}.make_state<IntRun>();
        state.update(Step{.applied = 1, .at = 1}, rng);
        ok &= expect(tenure(state, -1) == 3, "random tenure: the tenure drawn applies");
        state.update(Step{.applied = 5, .at = 2}, rng);
        state.update(Step{.applied = 6, .at = 3}, rng);
        ok &= expect(tenure(state, -1) == 1, "random tenure: the tenure left decreases");
        state.update(Step{.applied = 7, .at = 4}, rng);
        ok &= expect(
            !tenure(state, -1).has_value(),
            "random tenure: a move leaves when due");

        auto wide =
            tabu::RandomTenure{{.min_tenure = 2, .max_tenure = 6}}.make_state<IntRun>();
        bool within = true;
        for (std::size_t iteration = 1; iteration <= 50; ++iteration)
        {
            const auto move = static_cast<int>(iteration);
            wide.update(Step{.applied = move, .at = iteration}, rng);
            const auto left = tenure(wide, -move);
            within = within && left.has_value() && *left >= 2 && *left <= 6;
        }
        ok &= expect(within, "random tenure: tenures stay in [min_tenure, max_tenure]");
        ok &= expect(
            !tabu::RandomTenureParameters{.min_tenure = 5, .max_tenure = 4}.validate()
                && !tabu::RandomTenureParameters{.min_tenure = 0}.validate(),
            "random tenure: the bounds are validated");
    }

    {
        // Each tenure for two iterations: 1, 1, then 3.
        auto state = tabu::Cyclic{{.period = 2, .tenures = {1, 3}}}.make_state<IntRun>();
        ok &=
            expect(state.current_tenure() == 1, "cyclic: the first tenure applies first");
        state.update(Step{.applied = 1, .at = 1}, rng);
        state.update(Step{.applied = 2, .at = 2}, rng);
        ok &= expect(
            state.current_tenure() == 3 && !tenure(state, -1).has_value()
                && tenure(state, -2) == 1,
            "cyclic: after period iterations the next tenure applies");
        state.update(Step{.applied = 3, .at = 3}, rng);
        ok &= expect(tenure(state, -3) == 3, "cyclic: moves take the current tenure");
        ok &= expect(
            !tabu::CyclicParameters{.tenures = {}}.validate()
                && !tabu::CyclicParameters{.tenures = {4, 0}}.validate()
                && !tabu::CyclicParameters{.period = 0}.validate(),
            "cyclic: the tenures and the period are validated");
    }

    {
        // Attributes 1, 1, 2 in three iterations: 1 is above half of them.
        tabu::Frequency::state<int> state{{.threshold = 0.5}};
        ok &=
            expect(!tenure(state, 1).has_value(), "frequency: nothing is tabu at first");
        state.update(Step{.applied = 1, .at = 1}, rng);
        state.update(Step{.applied = -1, .at = 2}, rng);
        state.update(Step{.applied = 2, .at = 3}, rng);
        ok &= expect(
            tenure(state, 1) == 1 && tenure(state, -1) == 1
                && !tenure(state, 2).has_value(),
            "frequency: an attribute applied too often is tabu until its frequency drops");
    }

    {
        tabu::Reactive reactive{{
            .increase = 2.0,
            .decrease = 0.5,
            .repetitions = 1,
            .chaos = 1,
            .cycle_length = 10,
        }};
        auto state = reactive.make_state<IntRun>();
        ok &= expect(state.current_tenure() == 1, "reactive: the tenure starts at 1");
        state.update(Step{.applied = 1, .at = 1, .hash = 11}, rng);
        state.update(Step{.applied = 2, .at = 2, .hash = 22}, rng);
        // Back to 11 after 2 iterations: a cycle (and the first chaos count).
        state.update(Step{.applied = 3, .at = 3, .hash = 11}, rng);
        ok &= expect(
            state.current_tenure() == 2 && tenure(state, -3) == 2
                && tenure(state, -2) == 1 && state.escape_moves() == 0,
            "reactive: a cycle increases the tenure");
        // 11 again: a second chaos count, over the threshold, escapes; the
        // average cycle is 0.9 * 10 + 0.1 * 2 = 9.2, so 5 to 10 moves.
        state.update(Step{.applied = 4, .at = 4, .hash = 11}, rng);
        const auto escape = state.escape_moves();
        ok &= expect(
            escape >= 5 && escape <= 10 && state.escape_moves() == 0
                && state.current_tenure() == 1 && !tenure(state, -4).has_value(),
            "reactive: chaos escapes with random moves and resets the memory");

        auto calm =
            tabu::Reactive{{.increase = 2.0, .decrease = 0.5, .cycle_length = 4}}
                .make_state<IntRun>();
        calm.update(Step{.applied = 1, .at = 1, .hash = 1}, rng);
        calm.update(Step{.applied = 2, .at = 2, .hash = 1}, rng);
        ok &= expect(
            calm.current_tenure() == 2,
            "reactive: a revisit within cycle_length is a cycle");
        // The average cycle is now 0.9 * 4 + 0.1 * 1 = 3.7: four quiet
        // iterations later the tenure decreases.
        for (std::size_t iteration = 3; iteration <= 6; ++iteration)
            calm.update(
                Step{.applied = 10, .at = iteration, .hash = iteration * 100},
                rng);
        ok &= expect(
            calm.current_tenure() == 1,
            "reactive: without cycles for longer than the average, the tenure decreases");
        ok &= expect(
            !tabu::ReactiveParameters{.increase = 1.0}.validate()
                && !tabu::ReactiveParameters{.decrease = 1.0}.validate(),
            "reactive: the factors are validated");
    }

    {
        // The costs 7 then 9 were reached; a candidate reaching 7 is tabu.
        auto state = tabu::ObjectiveBased{{.tenure = 2}}.make_state<IntRun>();
        static_assert(decltype(state)::needs_cost);
        state.update(Step{.applied = 1, .at = 1, .value = 7}, rng);
        state.update(Step{.applied = 2, .at = 2, .value = 9}, rng);
        ok &= expect(
            state.tabu_tenure(Candidate{.move = 5, .value = 7}) == 1
                && state.tabu_tenure(Candidate{.move = 5, .value = 9}) == 2
                && !state.tabu_tenure(Candidate{.move = 5, .value = 8}).has_value(),
            "objective based: a reached cost is tabu for tenure iterations");
        state.update(Step{.applied = 3, .at = 3, .value = 4}, rng);
        ok &= expect(
            !state.tabu_tenure(Candidate{.move = 5, .value = 7}).has_value(),
            "objective based: the oldest cost leaves");
    }

    {
        // Grows after 2 idle iterations, falls back at 4 or on improvement.
        auto state =
            tabu::LimDynamic{{.min_tenure = 2, .max_tenure = 4, .idle_threshold = 2}}
                .make_state<IntRun>();
        state.update(Step{.applied = 1, .at = 1}, rng);
        ok &= expect(state.current_tenure() == 2, "lim dynamic: starts at min_tenure");
        state.update(Step{.applied = 2, .at = 2}, rng);
        ok &= expect(
            state.current_tenure() == 3 && tenure(state, -1) == 2,
            "lim dynamic: grows after idle_threshold idle iterations");
        state.update(Step{.applied = 3, .at = 3}, rng);
        ok &=
            expect(state.current_tenure() == 4, "lim dynamic: keeps growing while idle");
        state.update(Step{.applied = 4, .at = 4}, rng);
        ok &= expect(
            state.current_tenure() == 2 && !tenure(state, -1).has_value(),
            "lim dynamic: falls back to min_tenure at max_tenure");
        state.update(Step{.applied = 5, .at = 5}, rng);
        state.update(Step{.applied = 6, .at = 6, .improved = true}, rng);
        ok &=
            expect(state.current_tenure() == 2, "lim dynamic: an improvement resets it");
    }

    {
        // Windows of 2: costs 5, 5 spread 0 < 1 (grow by 3), then 5, 9 spread
        // 4 (shrink by 1).
        auto state =
            tabu::Foo{{.window = 2, .increment = 3, .fluctuation = 1.0}}
                .make_state<IntRun>();
        ok &= expect(
            state.current_tenure() == 3,
            "foo: the initial tenure is the increment");
        state.update(Step{.applied = 1, .at = 1, .value = 5}, rng);
        state.update(Step{.applied = 2, .at = 2, .value = 5}, rng);
        ok &= expect(
            state.current_tenure() == 6 && tenure(state, -1) == 5,
            "foo: a stuck search grows the tenure");
        state.update(Step{.applied = 3, .at = 3, .value = 5}, rng);
        state.update(Step{.applied = 4, .at = 4, .value = 9}, rng);
        ok &= expect(state.current_tenure() == 5, "foo: a fluctuating search shrinks it");

        auto random = tabu::RandomFoo{
            {
                .min_window = 2,
                .max_window = 2,
                .min_increment = 3,
                .max_increment = 3,
                .min_fluctuation = 1.0,
                .max_fluctuation = 1.0,
            }}.make_state<IntRun>();
        random.update(Step{.applied = 1, .at = 1, .value = 5}, rng);
        random.update(Step{.applied = 2, .at = 2, .value = 5}, rng);
        ok &= expect(
            random.current_tenure() == 6,
            "random foo: with degenerate ranges it is foo");
        ok &= expect(
            !tabu::RandomFooParameters{.min_window = 5, .max_window = 4}.validate()
                && !tabu::FooParameters{.window = 0}.validate(),
            "foo: the parameters are validated");
    }

    return ok ? 0 : 1;
}
