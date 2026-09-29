#include <easylocal/trace.hpp>

#include <iostream>
#include <sstream>
#include <streambuf>
#include <string_view>

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

    return ok ? 0 : 1;
}
