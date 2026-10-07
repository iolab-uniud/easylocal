// Writes the same events as ELTR and as JSON Lines, for the decoder test
// (tests/eltr_decode.py): every core event with an integral cost, a run with
// a structured cost, application events in the binary trace only (one with a
// schema, one without), a hierarchical cost with the default writers, and the
// events of a real pipeline solve.
#include <easylocal/cost.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/hill_climbing.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/solvers.hpp>
#include <easylocal/trace.hpp>
#include <easylocal/utils/generator.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <ostream>
#include <random>
#include <type_traits>
#include <vector>

namespace
{

struct structured_cost
{
    int hard{};
    int soft{};
};

struct structured_json_writer
{
    void operator()(std::ostream& out, const structured_cost& cost) const
    {
        out << "{\"hard\":" << cost.hard << ",\"soft\":" << cost.soft << '}';
    }
};

struct structured_binary_writer
{
    void operator()(
        easylocal::trace::binary_record_writer& out,
        const structured_cost& cost) const
    {
        out.i32(cost.hard);
        out.i32(cost.soft);
    }

    [[nodiscard]] static auto fields() -> std::vector<easylocal::trace::binary_field>
    {
        using enum easylocal::trace::binary_type;
        return {{"hard", i32}, {"soft", i32}};
    }
};

struct weight_changed
{
    std::uint64_t iteration{};
    double weight{};
};

constexpr auto binary_event_tag(const weight_changed&) noexcept -> std::uint8_t
{
    return easylocal::trace::user_binary_event_tag<3>();
}

void encode_binary_event(
    easylocal::trace::binary_record_writer& out,
    const weight_changed& value)
{
    out.u64(value.iteration);
    out.f64(value.weight);
}

auto describe_binary_event(std::type_identity<weight_changed>)
    -> easylocal::trace::binary_event_schema
{
    using enum easylocal::trace::binary_type;
    return {"weight_changed", {{"iteration", u64}, {"weight", f64}}};
}

// An application event without a schema.
struct opaque_event
{
    std::uint16_t code{};
};

constexpr auto binary_event_tag(const opaque_event&) noexcept -> std::uint8_t
{
    return easylocal::trace::user_binary_event_tag<4>();
}

void encode_binary_event(
    easylocal::trace::binary_record_writer& out,
    const opaque_event& value)
{
    out.u16(value.code);
}

template<class Event, class... Recorders>
void emit_all(const Event& value, Recorders&... recorders)
{
    (easylocal::trace::emit(recorders, value), ...);
}

void write_integral(const std::filesystem::path& directory)
{
    namespace event = easylocal::trace::event;
    std::ofstream binary_file{directory / "integral.eltr", std::ios::binary};
    std::ofstream json_file{directory / "integral.jsonl"};
    easylocal::trace::binary_recorder<long> binary{
        binary_file,
        {.metadata = {{"instance", "fixture"}, {"runner", "none"}}}};
    easylocal::trace::jsonl_recorder<long> json{
        json_file,
        {.metadata = {{"instance", "fixture"}, {"runner", "none"}}}};

    const easylocal::trace::neighborhood_route_node outer{.child = 2};
    const easylocal::trace::neighborhood_route_node inner{.child = 1, .parent = &outer};

    // Two runs: the second revisits the first run's solutions, which are
    // trajectory edges of their own run only.
    for (long run = 0; run < 2; ++run)
    {
        // The stage name is escaped in JSON.
        emit_all(
            event::run_context{
                .stage = "anneal \"hot\"",
                .stage_index = 1,
                .attempt = static_cast<std::size_t>(run),
            },
            binary,
            json);
        emit_all(event::run_started<long>{40 + run}, binary, json);
        emit_all(
            event::solution_visited<long>{0, 0, 0xfeedfacecafebeefULL, 40 + run},
            binary,
            json);
        emit_all(
            event::neighborhood_selection{
                .attempt = 0,
                .child = 1,
                // Values a float cannot hold, and a NaN, which both formats
                // write as null.
                .bias = 0.1,
                .active_bias_total = 0.30000000000000004,
                .conditional_probability =
                    run == 0 ? 1.0 / 3.0 : std::numeric_limits<double>::quiet_NaN(),
                .produced_move = true,
                .neighborhood = &inner,
            },
            binary,
            json);
        emit_all(event::move_evaluated<long>{1, 0, 40 + run, -7, &inner}, binary, json);
        emit_all(event::move_accepted<long>{1, 1, 40 + run, -7, &inner}, binary, json);
        emit_all(event::move_accepted<long>{2, 2, -7, -7, nullptr}, binary, json);
        emit_all(
            event::solution_visited<long>{2, 2, 7, -7, 0xfeedfacecafebeefULL},
            binary,
            json);
        emit_all(event::incumbent_updated<long>{2, 2, 40 + run, -7}, binary, json);
        emit_all(event::aspiration_applied<long>{2, 2, -7}, binary, json);
        emit_all(event::tabu_escape{3, 3, 5}, binary, json);
        emit_all(event::tabu_tenure_changed{3, 3, 1, 2}, binary, json);
        emit_all(event::temperature_changed{3, 3, 1.5, 0.75}, binary, json);
        emit_all(event::local_optimum<long>{4, 3, -7}, binary, json);
        emit_all(
            event::solution_visited<long>{4, 3, 0xfeedfacecafebeefULL, 40, 7},
            binary,
            json);
        easylocal::trace::emit(binary, weight_changed{3, 0.5});
        easylocal::trace::emit(binary, opaque_event{0x0102});
        emit_all(event::run_finished<long>{4, 3, -7}, binary, json);
    }
    binary.flush();
}

void write_structured(const std::filesystem::path& directory)
{
    namespace event = easylocal::trace::event;
    std::ofstream binary_file{directory / "structured.eltr", std::ios::binary};
    std::ofstream json_file{directory / "structured.jsonl"};
    easylocal::trace::binary_recorder<structured_cost, structured_binary_writer> binary{
        binary_file,
        structured_binary_writer{}};
    easylocal::trace::jsonl_recorder<structured_cost, structured_json_writer> json{
        json_file,
        structured_json_writer{}};

    emit_all(event::run_started<structured_cost>{{2, 30}}, binary, json);
    emit_all(
        event::incumbent_updated<structured_cost>{5, 1, {2, 30}, {0, -4}},
        binary,
        json);
    emit_all(event::run_finished<structured_cost>{9, 2, {0, -4}}, binary, json);
    binary.flush();
}

// A hierarchical cost with the default cost writers of both recorders.
void write_hierarchical(const std::filesystem::path& directory)
{
    using cost_type =
        easylocal::cost::hierarchical<easylocal::cost::lexicographic<int, int>, double>;
    std::ofstream binary_file{directory / "hierarchical.eltr", std::ios::binary};
    std::ofstream json_file{directory / "hierarchical.jsonl"};
    easylocal::trace::binary_recorder<cost_type> binary{binary_file};
    easylocal::trace::jsonl_recorder<cost_type> json{json_file};
    emit_all(
        easylocal::trace::event::run_started<cost_type>{
            cost_type{easylocal::cost::lexicographic<int, int>{1, 2}, 0.5}},
        binary,
        json);
    binary.flush();
}

// Core events with timestamps, and an application event, which has none.
void write_timed(const std::filesystem::path& directory)
{
    namespace event = easylocal::trace::event;
    std::ofstream binary_file{directory / "timed.eltr", std::ios::binary};
    std::ofstream json_file{directory / "timed.jsonl"};
    easylocal::trace::binary_recorder<long> binary{binary_file, {.timestamps = true}};
    easylocal::trace::jsonl_recorder<long> json{json_file, {.timestamps = true}};
    emit_all(
        event::run_context{.stage = "", .stage_index = 0, .attempt = 0},
        binary,
        json);
    emit_all(event::run_started<long>{3}, binary, json);
    easylocal::trace::emit(binary, weight_changed{1, 0.25});
    emit_all(event::run_finished<long>{1, 1, 3}, binary, json);
    binary.flush();
}

// The problem of the pipeline solve: a counter, whose cost is its value,
// lowered by one at each move down to 0.
struct Counter
{
    int start{};
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

// Sends each event to both recorders, which write the same solve.
template<class First, class Second>
struct both_recorders
{
    template<class Event>
    static constexpr bool observes = easylocal::trace::observes<First, Event>
        || easylocal::trace::observes<Second, Event>;

    First& first;
    Second& second;

    template<class Event>
    void emit(const Event& value)
    {
        easylocal::trace::emit(first, value);
        easylocal::trace::emit(second, value);
    }
};

// A two-stage pipeline from 6: two attempts of Hill Climbing, each stopped
// at 3 by its budget of 4 evaluations, then First Improvement down to 0. Every
// move improves, so the trace does not depend on the random numbers.
void write_pipeline(const std::filesystem::path& directory)
{
    namespace el = easylocal;
    std::ofstream binary_file{directory / "pipeline.eltr", std::ios::binary};
    std::ofstream json_file{directory / "pipeline.jsonl"};
    el::trace::binary_recorder<int> binary{binary_file};
    el::trace::jsonl_recorder<int> json{json_file};
    both_recorders<decltype(binary), decltype(json)> recorders{binary, json};

    const auto sm = el::solution_manager<CounterManager>() | el::component<CountValue>();
    const auto nhe = el::neighborhood<DecrementExplorer>();
    auto pipeline = el::solvers::pipeline(
        el::solvers::stage(
            "climb",
            el::make_runner<el::runners::HillClimbing>({.max_evaluations = 4}) | sm | nhe)
            & el::solvers::attempts(2),
        el::solvers::stage(
            "descend",
            el::make_runner<el::runners::FirstImprovement>() | sm | nhe));
    const Counter input{.start = 6};
    static_cast<void>(pipeline.initialization(el::initialization::initial)
            .seed(1)
            .solve(input, el::with(recorders)));
    binary.flush();
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: easylocal_eltr_fixture <directory>\n";
        return 2;
    }
    const std::filesystem::path directory{argv[1]};
    write_integral(directory);
    write_structured(directory);
    write_hierarchical(directory);
    write_timed(directory);
    write_pipeline(directory);
    return 0;
}
