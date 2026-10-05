#include <easylocal/cost.hpp>
#include <easylocal/trace.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace
{

auto expect(bool condition, std::string_view message) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

struct structured_cost
{
    int hard{};
    int soft{};
};

struct structured_cost_writer
{
    void operator()(std::ostream& out, const structured_cost& cost) const
    {
        out << "{\"hard\":" << cost.hard << ",\"soft\":" << cost.soft << '}';
    }
};

struct structured_binary_cost_writer
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

// The size of an ELTR stream's magic, version and header: the records start
// there.
auto records_offset(const std::string& data) -> std::size_t
{
    std::uint32_t size = 0;
    for (std::size_t index = 0; index < 4; ++index)
        size |= static_cast<std::uint32_t>(static_cast<unsigned char>(data[8 + index]))
            << (8 * index);
    return 12 + size;
}

namespace polli_extension
{

struct temperature_changed
{
    std::uint64_t iteration{};
    double temperature{};
};

constexpr auto binary_event_tag(const temperature_changed&) noexcept -> std::uint8_t
{
    return easylocal::trace::user_binary_event_tag<0>();
}

inline void encode_binary_event(
    easylocal::trace::binary_record_writer& out,
    const temperature_changed& value)
{
    out.u64(value.iteration);
    out.f64(value.temperature);
}

inline auto describe_binary_event(std::type_identity<temperature_changed>)
    -> easylocal::trace::binary_event_schema
{
    using enum easylocal::trace::binary_type;
    return {"temperature_changed", {{"iteration", u64}, {"temperature", f64}}};
}

} // namespace polli_extension

class sync_counting_streambuf : public std::stringbuf
{
public:
    int sync_calls{};

protected:
    auto sync() -> int override
    {
        ++sync_calls;
        return std::stringbuf::sync();
    }
};

// A stream buffer that fails its writes once broken, or its flushes.
class failing_streambuf : public std::stringbuf
{
public:
    std::atomic_bool broken{false};
    std::atomic_bool failing_sync{false};

protected:
    auto xsputn(const char* data, std::streamsize count) -> std::streamsize override
    {
        return broken ? 0 : std::stringbuf::xsputn(data, count);
    }

    auto overflow(int_type character) -> int_type override
    {
        return broken ? traits_type::eof() : std::stringbuf::overflow(character);
    }

    auto sync() -> int override
    {
        return failing_sync ? -1 : std::stringbuf::sync();
    }
};

// Cost writers that write part of a cost 13, then throw.
struct throwing_cost_error
{
};

struct throwing_binary_cost_writer
{
    void operator()(easylocal::trace::binary_record_writer& out, const int cost) const
    {
        out.i32(cost);
        if (cost == 13)
            throw throwing_cost_error{};
    }

    [[nodiscard]] static auto fields() -> std::vector<easylocal::trace::binary_field>
    {
        return {{"", easylocal::trace::binary_type::i32}};
    }
};

struct throwing_json_cost_writer
{
    void operator()(std::ostream& out, const int cost) const
    {
        out << cost;
        if (cost == 13)
            throw throwing_cost_error{};
    }
};

// An application event whose encoding throws for a value of 13.
struct fragile_event
{
    int value{};
};

constexpr auto binary_event_tag(const fragile_event&) noexcept -> std::uint8_t
{
    return easylocal::trace::user_binary_event_tag<1>();
}

inline void encode_binary_event(
    easylocal::trace::binary_record_writer& out,
    const fragile_event& event)
{
    out.i32(event.value);
    if (event.value == 13)
        throw throwing_cost_error{};
}

inline auto describe_binary_event(std::type_identity<fragile_event>)
    -> easylocal::trace::binary_event_schema
{
    return {"fragile", {{"value", easylocal::trace::binary_type::i32}}};
}

// Emits value, and whether its encoding threw.
template<class Tracer, class Event>
auto emit_throws(Tracer& tracer, const Event& value) -> bool
{
    try
    {
        easylocal::trace::emit(tracer, value);
    }
    catch (const throwing_cost_error&)
    {
        return true;
    }
    return false;
}

// A locale that groups the digits of numbers by three, with commas.
struct comma_grouping : std::numpunct<char>
{
    char do_thousands_sep() const override
    {
        return ',';
    }

    std::string do_grouping() const override
    {
        return "\3";
    }
};

// Emits one event of every kind, with routes, to a tracer.
template<class Tracer>
void emit_every_event(Tracer& tracer)
{
    namespace event = easylocal::trace::event;
    const easylocal::trace::neighborhood_route_node outer{.child = 2};
    const easylocal::trace::neighborhood_route_node inner{.child = 1, .parent = &outer};

    easylocal::trace::emit(tracer, event::run_started<int>{.cost = 10});
    easylocal::trace::emit(
        tracer,
        event::neighborhood_selection{
            .attempt = 1,
            .child = 1,
            .bias = 3.0,
            .active_bias_total = 4.0,
            .conditional_probability = 0.75,
            .produced_move = true,
            .neighborhood = &inner,
        });
    easylocal::trace::emit(
        tracer,
        event::move_evaluated<int>{
            .evaluations = 1,
            .iterations = 0,
            .current_cost = 10,
            .candidate_cost = 7,
            .neighborhood = &outer,
        });
    easylocal::trace::emit(
        tracer,
        event::move_accepted<int>{
            .evaluations = 1,
            .iterations = 1,
            .previous_cost = 10,
            .cost = 7,
            .neighborhood = nullptr,
        });
    easylocal::trace::emit(
        tracer,
        event::incumbent_updated<int>{
            .evaluations = 1,
            .iterations = 1,
            .previous_cost = 10,
            .cost = 7,
        });
    easylocal::trace::emit(
        tracer,
        event::solution_visited<int>{
            .evaluations = 1,
            .iterations = 1,
            .hash = 42,
            .cost = 7,
        });
    easylocal::trace::emit(
        tracer,
        event::aspiration_applied<int>{.evaluations = 2, .iterations = 2, .cost = 6});
    easylocal::trace::emit(
        tracer,
        event::tabu_escape{.evaluations = 3, .iterations = 3, .moves = 5});
    easylocal::trace::emit(
        tracer,
        event::tabu_tenure_changed{
            .evaluations = 3,
            .iterations = 3,
            .previous_tenure = 4,
            .tenure = 6,
        });
    easylocal::trace::emit(
        tracer,
        event::local_optimum<int>{.evaluations = 4, .iterations = 4, .cost = 6});
    easylocal::trace::emit(
        tracer,
        event::run_finished<int>{.evaluations = 4, .iterations = 4, .cost = 6});
}

} // namespace

int main()
{
    bool ok = true;
    easylocal::trace::memory_recorder<int> recorder;

    const easylocal::trace::neighborhood_route_node outer{.child = 2};
    const easylocal::trace::neighborhood_route_node inner{.child = 1, .parent = &outer};

    easylocal::trace::emit(
        recorder,
        easylocal::trace::event::run_started<int>{10});
    easylocal::trace::emit(
        recorder,
        easylocal::trace::event::neighborhood_selection{
            .attempt = 0,
            .child = 1,
            .bias = 3.0,
            .active_bias_total = 4.0,
            .conditional_probability = 0.75,
            .produced_move = true,
            .neighborhood = &inner,
        });
    easylocal::trace::emit(
        recorder,
        easylocal::trace::event::move_evaluated<int>{
            .evaluations = 2,
            .iterations = 0,
            .current_cost = 10,
            .candidate_cost = 7,
            .neighborhood = &inner,
        });

    ok &= expect(recorder.records().size() == 3, "memory recorder stores typed events");

    const auto* selection = std::get_if<
        easylocal::trace::memory_recorder<int>::neighborhood_selection_record>(
            &recorder.records()[1]);
    ok &= expect(selection != nullptr, "selection event keeps its record type");
    ok &= expect(
        selection != nullptr && selection->neighborhood == std::vector<std::size_t>{2, 1},
        "recorder copies hierarchical neighborhood provenance");

    std::ostringstream output;
    easylocal::trace::write_jsonl(output, recorder);
    const auto jsonl = output.str();
    ok &= expect(
        jsonl.find("\"event\":\"neighborhood_selection\"") != std::string::npos &&
            jsonl.find("\"neighborhood\":[2,1]") != std::string::npos,
        "JSONL serialization preserves semantic event and route");

    easylocal::trace::memory_recorder<int> search;
    easylocal::trace::emit(
        search,
        easylocal::trace::event::move_accepted<int>{
            .evaluations = 3,
            .iterations = 1,
            .previous_cost = 10,
            .cost = 7,
            .neighborhood = &inner,
        });
    easylocal::trace::emit(
        search,
        easylocal::trace::event::incumbent_updated<int>{
            .evaluations = 3,
            .iterations = 1,
            .previous_cost = 10,
            .cost = 7,
        });
    easylocal::trace::emit(
        search,
        easylocal::trace::event::local_optimum<int>{
            .evaluations = 9,
            .iterations = 4,
            .cost = 7});
    easylocal::trace::emit(
        search,
        easylocal::trace::event::run_finished<int>{
            .evaluations = 9,
            .iterations = 4,
            .cost = 7});

    std::ostringstream search_output;
    easylocal::trace::write_jsonl(search_output, search);
    ok &= expect(
        search_output.str()
            == "{\"event\":\"trace\",\"version\":1,\"metadata\":{}}\n"
               "{\"event\":\"move_accepted\",\"evaluations\":3,\"iterations\":1,"
               "\"previous_cost\":10,\"cost\":7,\"neighborhood\":[2,1]}\n"
               "{\"event\":\"incumbent_updated\",\"evaluations\":3,\"iterations\":1,"
               "\"previous_cost\":10,\"cost\":7}\n"
               "{\"event\":\"local_optimum\",\"evaluations\":9,\"iterations\":4,\"cost\":7}\n"
               "{\"event\":\"run_finished\",\"evaluations\":9,\"iterations\":4,\"cost\":7,"
               "\"termination\":\"completed\"}\n",
        "JSONL serialization writes one line per search event");

    std::ostringstream streamed_output;
    easylocal::trace::jsonl_recorder<int> streamed{streamed_output};
    easylocal::trace::emit(
        streamed,
        easylocal::trace::event::move_evaluated<int>{
            .evaluations = 3,
            .iterations = 1,
            .current_cost = 7,
            .candidate_cost = 6,
            .neighborhood = &inner,
        });
    easylocal::trace::emit(
        streamed,
        easylocal::trace::event::run_finished<int>{
            .evaluations = 3,
            .iterations = 1,
            .cost = 6,
        });
    const auto streamed_jsonl = streamed_output.str();
    ok &= expect(
        streamed_jsonl.find("\"event\":\"move_evaluated\"") != std::string::npos &&
            streamed_jsonl.find("\"neighborhood\":[2,1]") != std::string::npos &&
            streamed_jsonl.find("\"event\":\"run_finished\"") != std::string::npos,
        "streaming JSONL recorder serializes events incrementally");
    ok &= expect(streamed.good(), "streaming JSONL recorder exposes stream state");

    // Numbers keep every digit, and JSON has no NaN: it is written as null.
    std::ostringstream precise_output;
    easylocal::trace::jsonl_recorder<double> precise{precise_output};
    easylocal::trace::emit(
        precise,
        easylocal::trace::event::run_finished<double>{
            .evaluations = 1,
            .iterations = 1,
            .cost = 1234567.5,
        });
    easylocal::trace::emit(
        precise,
        easylocal::trace::event::neighborhood_selection{
            .attempt = 1,
            .child = 0,
            .bias = std::numeric_limits<double>::quiet_NaN(),
            .active_bias_total = 0.1 + 0.2,
            .conditional_probability = 1.0,
            .produced_move = true,
            .neighborhood = &inner,
        });
    const auto precise_jsonl = precise_output.str();
    ok &= expect(
        precise_jsonl.find("\"cost\":1234567.5") != std::string::npos
            && precise_jsonl.find("\"bias\":null") != std::string::npos
            && precise_jsonl.find("\"active_bias_total\":0.30000000000000004")
                != std::string::npos,
        "JSONL writes numbers that read back to the same value, and NaN as null");

    // The stream's locale does not reach the numbers: grouped digits are not
    // JSON.
    std::ostringstream grouped_output;
    grouped_output.imbue(std::locale(grouped_output.getloc(), new comma_grouping));
    easylocal::trace::jsonl_recorder<long> grouped{grouped_output};
    easylocal::trace::emit(
        grouped,
        easylocal::trace::event::run_finished<long>{
            .evaluations = 1234567,
            .iterations = 7654321,
            .cost = 9876543,
        });
    const auto grouped_jsonl = grouped_output.str();
    ok &= expect(
        grouped_jsonl.find(
            "\"evaluations\":1234567,\"iterations\":7654321,"
            "\"cost\":9876543,")
            != std::string::npos,
        "JSONL numbers ignore a digit-grouping locale of the stream");

    sync_counting_streambuf buffered_storage;
    std::ostream buffered_output{&buffered_storage};
    easylocal::trace::jsonl_recorder<int> buffered{buffered_output};
    easylocal::trace::emit(
        buffered,
        easylocal::trace::event::run_started<int>{4});
    ok &= expect(
        buffered_storage.sync_calls == 0,
        "streaming recorder does not flush per event");
    buffered.flush();
    ok &= expect(
        buffered_storage.sync_calls == 1,
        "streaming recorder flush is explicit");

    easylocal::trace::memory_recorder<structured_cost> structured_memory;
    easylocal::trace::emit(
        structured_memory,
        easylocal::trace::event::run_started<structured_cost>{{3, 7}});
    std::ostringstream structured_post_run;
    easylocal::trace::write_jsonl(
        structured_post_run,
        structured_memory,
        structured_cost_writer{});
    ok &= expect(
        structured_post_run.str().find(
            "\"cost\":{\"hard\":3,\"soft\":7}") != std::string::npos,
        "post-run JSONL accepts a custom structured cost writer");

    std::ostringstream live_output;
    easylocal::trace::jsonl_recorder<int> live{live_output};
    emit_every_event(live);
    easylocal::trace::memory_recorder<int> stored;
    emit_every_event(stored);
    std::ostringstream post_run_output;
    easylocal::trace::write_jsonl(post_run_output, stored);
    ok &= expect(
        post_run_output.str() == live_output.str()
            && post_run_output.str().find("\"neighborhood\":[2,1]") != std::string::npos,
        "post-run JSONL writes every event as the streaming recorder does");

    // The header line, first, with the metadata as a JSON object.
    std::ostringstream headed_output;
    const easylocal::trace::jsonl_recorder<int> headed{
        headed_output,
        {.metadata = {{"instance", "a\"b"}, {"seed", "7"}}}};
    ok &= expect(
        headed_output.str()
            == "{\"event\":\"trace\",\"version\":1,\"metadata\":{\"instance\":"
               "\"a\\\"b\",\"seed\":\"7\"}}\n",
        "the JSONL recorder starts with a header line with its metadata");

    // With timestamps, each event line, not the header, ends with elapsed_ns.
    std::ostringstream timed_output;
    easylocal::trace::jsonl_recorder<int> timed{timed_output, {.timestamps = true}};
    easylocal::trace::emit(timed, easylocal::trace::event::run_started<int>{4});
    const auto timed_jsonl = timed_output.str();
    const auto header_end = timed_jsonl.find('\n');
    ok &= expect(
        timed_jsonl.rfind("\"elapsed_ns\":", header_end) == std::string::npos
            && timed_jsonl.find("{\"event\":\"run_started\",\"cost\":4,\"elapsed_ns\":")
                == header_end + 1
            && timed_jsonl.ends_with("}\n"),
        "a JSONL recorder with timestamps ends each event with elapsed_ns");

    std::ostringstream structured_stream;
    easylocal::trace::jsonl_recorder<structured_cost, structured_cost_writer>
        structured_recorder{structured_stream, structured_cost_writer{}};
    easylocal::trace::emit(
        structured_recorder,
        easylocal::trace::event::run_finished<structured_cost>{
            .evaluations = 9,
            .iterations = 2,
            .cost = {1, 5},
        });
    ok &= expect(
        structured_stream.str().find(
            "\"cost\":{\"hard\":1,\"soft\":5}") != std::string::npos,
        "streaming JSONL accepts a custom structured cost writer");

    std::ostringstream binary_stream;
    easylocal::trace::binary_recorder<int> binary{binary_stream};
    easylocal::trace::emit(
        binary,
        easylocal::trace::event::move_evaluated<int>{
            .evaluations = 3,
            .iterations = 1,
            .current_cost = 7,
            .candidate_cost = 6,
            .neighborhood = &inner,
        });
    binary.flush();
    const auto binary_data = binary_stream.str();
    const auto binary_records = records_offset(binary_data);
    ok &= expect(
        binary_data.size() == binary_records + 5 + 44,
        "binary recorder writes versioned header and compact event payload");
    ok &= expect(
        binary_data.size() >= 8 && binary_data.substr(0, 4) == "ELTR" &&
            static_cast<unsigned char>(binary_data[4]) == 1,
        "binary recorder writes ELTR format version 1");
    ok &= expect(
        static_cast<unsigned char>(binary_data[binary_records]) == 2,
        "binary recorder preserves the event type tag");
    ok &= expect(
        binary_data.find("move_evaluated") < binary_records
            && binary_data.find("candidate_cost") < binary_records,
        "the header names the events and their fields");

    std::ostringstream described_stream;
    easylocal::trace::binary_recorder<easylocal::cost::hierarchical<int, double>>
        described{described_stream, {.metadata = {{"instance", "ta001"}}}};
    described.flush();
    const auto described_data = described_stream.str();
    ok &= expect(
        described_data.size() == records_offset(described_data)
            && described_data.find("ta001") != std::string::npos
            && described_data.find("hard") != std::string::npos
            && described_data.find("soft") != std::string::npos,
        "the header holds the metadata and the default layout of a hierarchical cost");

    sync_counting_streambuf binary_buffered_storage;
    std::ostream binary_buffered_output{&binary_buffered_storage};
    easylocal::trace::binary_recorder<int> binary_buffered{binary_buffered_output};
    ok &= expect(
        binary_buffered_storage.sync_calls == 1,
        "binary recorder flushes its header at construction");
    easylocal::trace::emit(
        binary_buffered,
        easylocal::trace::event::run_started<int>{4});
    ok &= expect(
        binary_buffered_storage.sync_calls == 1,
        "binary recorder does not flush per event");
    binary_buffered.flush();
    ok &= expect(
        binary_buffered_storage.sync_calls == 2,
        "binary recorder flush is explicit");

    // The header reaches the stream at construction: a run that ends before
    // the first block is written still leaves a decodable trace.
    {
        std::ostringstream stream;
        easylocal::trace::binary_recorder<int> recorder{stream};
        easylocal::trace::emit(recorder, easylocal::trace::event::run_started<int>{4});
        const auto data = stream.str();
        ok &= expect(
            data.size() > 12 && data.size() == records_offset(data),
            "binary recorder writes its header at construction");
    }
    {
        std::ostringstream stream;
        easylocal::trace::async_binary_recorder<int> recorder{stream};
        const auto data = stream.str();
        ok &= expect(
            data.size() > 12 && data.size() == records_offset(data),
            "async binary recorder writes its header at construction");
    }

    std::ostringstream structured_binary_stream;
    easylocal::trace::binary_recorder<structured_cost, structured_binary_cost_writer>
        structured_binary{structured_binary_stream, structured_binary_cost_writer{}};
    easylocal::trace::emit(
        structured_binary,
        easylocal::trace::event::run_finished<structured_cost>{
            .evaluations = 9,
            .iterations = 2,
            .cost = {1, 5},
        });
    structured_binary.flush();
    ok &= expect(
        structured_binary_stream.str().size()
            // The record header, then evaluations, iterations and the cost (24
            // bytes) and the termination ("completed": a u32 size and 9 bytes).
            == records_offset(structured_binary_stream.str()) + 5 + 24 + 4 + 9,
        "binary recorder accepts a custom structured cost writer");

    std::ostringstream custom_event_stream;
    easylocal::trace::binary_recorder<int> custom_event_recorder{custom_event_stream};
    easylocal::trace::emit(
        custom_event_recorder,
        polli_extension::temperature_changed{.iteration = 42, .temperature = 0.75});
    custom_event_recorder.flush();
    const auto custom_event_data = custom_event_stream.str();
    const auto custom_records = records_offset(custom_event_data);
    ok &= expect(
        custom_event_data.size() > custom_records
            && static_cast<unsigned char>(custom_event_data[custom_records]) == 0
            && custom_event_data.find("temperature_changed") > custom_records
            && static_cast<unsigned char>(
                   custom_event_data[custom_event_data.size() - 21])
                == easylocal::trace::user_binary_event_tag<0>(),
        "application events extend ELTR via ADL, described before their first record");

    std::ostringstream sync_equivalent_stream;
    easylocal::trace::buffered_binary_recorder<int> sync_equivalent{
        sync_equivalent_stream,
        easylocal::trace::binary_buffer_options{.block_size = 17}};
    easylocal::trace::emit(
        sync_equivalent,
        easylocal::trace::event::move_evaluated<int>{
            .evaluations = 3,
            .iterations = 1,
            .current_cost = 7,
            .candidate_cost = 6,
            .neighborhood = &inner,
        });
    sync_equivalent.flush();

    std::ostringstream async_equivalent_stream;
    {
        easylocal::trace::async_binary_recorder<int> async_equivalent{
            async_equivalent_stream,
            easylocal::trace::binary_buffer_options{
                .block_size = 17,
                .async_queue_blocks = 2,
            }};
        easylocal::trace::emit(
            async_equivalent,
            easylocal::trace::event::move_evaluated<int>{
                .evaluations = 3,
                .iterations = 1,
                .current_cost = 7,
                .candidate_cost = 6,
                .neighborhood = &inner,
            });
        async_equivalent.flush();
        ok &= expect(async_equivalent.good(), "async binary recorder reports writer health");
    }
    ok &= expect(
        sync_equivalent_stream.str() == async_equivalent_stream.str() &&
            sync_equivalent_stream.str() == binary_data,
        "buffered and asynchronous recorders preserve identical ELTR bytes");

    // An output error stops the recording, not the search: emit() goes on
    // without throwing, and good() and flush() report the error.
    {
        failing_streambuf storage;
        std::ostream output{&storage};
        easylocal::trace::async_binary_recorder<int> recorder{output, {.block_size = 1}};
        storage.broken = true;
        bool thrown = false;
        try
        {
            for (int index = 0; index < 1000; ++index)
                easylocal::trace::emit(
                    recorder,
                    easylocal::trace::event::run_started<int>{index});
        }
        catch (const std::ios_base::failure&)
        {
            thrown = true;
        }
        ok &= expect(
            !thrown,
            "async binary recorder does not throw from emit after an error");
        bool flush_thrown = false;
        try
        {
            recorder.flush();
        }
        catch (const std::ios_base::failure&)
        {
            flush_thrown = true;
        }
        ok &= expect(
            flush_thrown && !recorder.good(),
            "async binary recorder reports a write error through flush and good");
    }
    {
        failing_streambuf storage;
        std::ostream output{&storage};
        easylocal::trace::async_binary_recorder<int> recorder{output};
        easylocal::trace::emit(recorder, easylocal::trace::event::run_started<int>{1});
        storage.failing_sync = true;
        bool flush_thrown = false;
        try
        {
            recorder.flush();
        }
        catch (const std::ios_base::failure&)
        {
            flush_thrown = true;
        }
        ok &= expect(
            flush_thrown && !recorder.good(),
            "async binary recorder reports a failed final flush through flush and good");
    }

    // An encoding that throws leaves no part of its record behind.
    {
        namespace event = easylocal::trace::event;
        std::ostringstream stream;
        easylocal::trace::binary_recorder<int, throwing_binary_cost_writer> recorder{
            stream,
            throwing_binary_cost_writer{}};
        const auto records = stream.str().size();
        const bool threw = !emit_throws(recorder, event::run_started<int>{1})
            && emit_throws(recorder, event::run_started<int>{13})
            && !emit_throws(recorder, event::run_started<int>{2});
        recorder.flush();
        const auto data = stream.str();
        ok &= expect(
            threw && data.size() == records + 2 * (5 + 4)
                && static_cast<unsigned char>(data[records + 5]) == 1
                && static_cast<unsigned char>(data[records + 9 + 5]) == 2,
            "binary recorder drops the whole record whose cost writer throws");

        std::ostringstream described_stream;
        easylocal::trace::binary_recorder<int> described{described_stream};
        const auto start = described_stream.str().size();
        const bool described_threw = emit_throws(described, fragile_event{13})
            && !emit_throws(described, fragile_event{2});
        described.flush();
        const auto described_data = described_stream.str();
        ok &= expect(
            described_threw && static_cast<unsigned char>(described_data[start]) == 0
                && described_data.find("fragile") != std::string::npos
                && described_data.size()
                    == start + 5 + (1 + 4 + 7 + 4 + 4 + 5 + 1) + 5 + 4,
            "binary recorder writes the schema again after a record that threw");

        std::ostringstream jsonl;
        easylocal::trace::jsonl_recorder<int, throwing_json_cost_writer> json{
            jsonl,
            throwing_json_cost_writer{}};
        const bool json_threw = !emit_throws(json, event::run_started<int>{1})
            && emit_throws(json, event::run_started<int>{13})
            && !emit_throws(json, event::run_started<int>{2});
        ok &= expect(
            json_threw
                && jsonl.str()
                    == "{\"event\":\"trace\",\"version\":1,\"metadata\":{}}\n"
                       "{\"event\":\"run_started\",\"cost\":1}\n"
                       "{\"event\":\"run_started\",\"cost\":2}\n",
            "JSONL recorder writes no part of a line whose cost writer throws");
    }

    // The queued blocks are allocated at construction: an unlimited queue is
    // rejected.
    {
        std::ostringstream stream;
        bool rejected = false;
        try
        {
            easylocal::trace::async_binary_recorder<int> recorder{
                stream,
                {.async_queue_blocks = std::numeric_limits<std::size_t>::max()}};
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        ok &= expect(rejected, "async binary recorder rejects an unlimited queue");
    }

    // The trajectory and tabu events, in ELTR: tags 8, 9 and 10, with the
    // hash as a little-endian u64 after the counters.
    {
        std::ostringstream stream;
        easylocal::trace::binary_recorder<int> recorder{stream};
        easylocal::trace::emit(
            recorder,
            easylocal::trace::event::solution_visited<int>{
                .evaluations = 5,
                .iterations = 2,
                .hash = 0x0102030405060708ULL,
                .cost = 9,
            });
        easylocal::trace::emit(
            recorder,
            easylocal::trace::event::aspiration_applied<int>{
                .evaluations = 6,
                .iterations = 3,
                .cost = 4,
            });
        easylocal::trace::emit(
            recorder,
            easylocal::trace::event::tabu_escape{
                .evaluations = 7,
                .iterations = 4,
                .moves = 3});
        recorder.flush();
        const auto data = stream.str();
        const auto records = records_offset(data);
        const auto byte = [&data, records](const std::size_t index) {
            return static_cast<unsigned char>(data[records + index]);
        };
        std::uint64_t hash = 0;
        for (std::size_t index = 0; index < 8; ++index)
            hash |= static_cast<std::uint64_t>(byte(5 + 16 + index)) << (8 * index);
        ok &= expect(
            data.size() == records + (5 + 32) + (5 + 24) + (5 + 24) && byte(0) == 8
                && byte(37) == 9 && byte(66) == 10 && hash == 0x0102030405060708ULL,
            "binary recorder encodes solution_visited, aspiration_applied and tabu_escape");

        std::ostringstream jsonl;
        easylocal::trace::jsonl_recorder<int> json{jsonl};
        easylocal::trace::emit(
            json,
            easylocal::trace::event::solution_visited<int>{
                .evaluations = 5,
                .iterations = 2,
                .hash = 42,
                .cost = 9,
            });
        ok &= expect(
            jsonl.str().find("\"event\":\"solution_visited\"") != std::string::npos
                && jsonl.str().find("\"hash\":\"000000000000002a\"") != std::string::npos,
            "JSONL recorder writes solution_visited with its hash in hexadecimal");
    }

    // The run_context of a run, kept by the memory recorder with its stage
    // name copied, and written as JSON with the name escaped.
    {
        easylocal::trace::memory_recorder<int> memory;
        {
            const std::string stage{"a \"b\"\n"};
            easylocal::trace::emit(
                memory,
                easylocal::trace::event::run_context{
                    .stage = stage,
                    .stage_index = 2,
                    .attempt = 3});
        }
        std::ostringstream jsonl;
        easylocal::trace::write_jsonl(jsonl, memory);
        ok &= expect(
            jsonl.str()
                == "{\"event\":\"trace\",\"version\":1,\"metadata\":{}}\n"
                   "{\"event\":\"run_context\",\"stage\":\"a \\\"b\\\"\\u000a\","
                   "\"stage_index\":2,\"attempt\":3}\n",
            "a run_context is recorded and written as JSON");
    }

    return ok ? 0 : 1;
}
