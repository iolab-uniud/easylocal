#include <easylocal/trace.hpp>

#include <iostream>
#include <sstream>
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

    return ok ? 0 : 1;
}
