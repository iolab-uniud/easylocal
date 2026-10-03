// Writes the same events as ELTR and as JSON Lines, for the decoder test
// (tests/eltr_decode.py): every core event with an integral cost, a run with
// a structured cost, and an application event in the binary trace only.
#include <easylocal/trace.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>

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
};

struct temperature_changed
{
    std::uint64_t iteration{};
    double temperature{};
};

constexpr auto binary_event_tag(const temperature_changed&) noexcept -> std::uint8_t
{
    return easylocal::trace::user_binary_event_tag<3>();
}

void encode_binary_event(
    easylocal::trace::binary_record_writer& out,
    const temperature_changed& value)
{
    out.u64(value.iteration);
    out.f64(value.temperature);
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
    easylocal::trace::binary_recorder<long> binary{binary_file};
    easylocal::trace::jsonl_recorder<long> json{json_file};

    const easylocal::trace::neighborhood_route_node outer{.child = 2};
    const easylocal::trace::neighborhood_route_node inner{.child = 1, .parent = &outer};

    // Two runs: the second revisits the first run's solutions, which are
    // trajectory edges of their own run only.
    for (long run = 0; run < 2; ++run)
    {
        emit_all(event::run_started<long>{40 + run}, binary, json);
        emit_all(
            event::solution_visited<long>{0, 0, 0xfeedfacecafebeefULL, 40 + run},
            binary,
            json);
        emit_all(
            event::neighborhood_selection{
                .attempt = 0,
                .child = 1,
                .bias = 3.0,
                .active_bias_total = 4.0,
                .conditional_probability = 0.75,
                .produced_move = true,
                .neighborhood = &inner,
            },
            binary,
            json);
        emit_all(event::move_evaluated<long>{1, 0, 40 + run, -7, &inner}, binary, json);
        emit_all(event::move_accepted<long>{1, 1, 40 + run, -7, &inner}, binary, json);
        emit_all(event::move_accepted<long>{2, 2, -7, -7, nullptr}, binary, json);
        emit_all(event::solution_visited<long>{2, 2, 7, -7}, binary, json);
        emit_all(event::incumbent_updated<long>{2, 2, 40 + run, -7}, binary, json);
        emit_all(event::aspiration_applied<long>{2, 2, -7}, binary, json);
        emit_all(event::tabu_escape{3, 3, 5}, binary, json);
        emit_all(event::local_optimum<long>{4, 3, -7}, binary, json);
        emit_all(
            event::solution_visited<long>{4, 3, 0xfeedfacecafebeefULL, 40},
            binary,
            json);
        easylocal::trace::emit(binary, temperature_changed{3, 0.5});
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
    return 0;
}
