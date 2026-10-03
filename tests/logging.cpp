#include <easylocal/utils/logging.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

namespace
{

struct capture_state
{
    int calls{};
    easylocal::logging::level severity{};
    easylocal::logging::origin source{};
    bool category_matches{};
    bool message_matches{};
    bool location_matches{};
};

capture_state capture;

void capture_sink(const easylocal::logging::record& entry) noexcept
{
    ++capture.calls;
    capture.severity = entry.severity;
    capture.source = entry.source;
    capture.category_matches = entry.category == "application.search";
    capture.message_matches = entry.message == "candidate accepted";
    capture.location_matches = entry.location.line() != 0;
}

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
    using namespace easylocal::logging;

    bool ok = true;

    const auto original = set_sink(&capture_sink);
    ok &= expect(original == &stderr_sink, "stderr is the default logging sink");
    ok &= expect(current_sink() == &capture_sink, "a custom sink can be installed");

    emit(
        level::info,
        "application.search",
        "candidate accepted");

    ok &= expect(capture.calls == 1, "emit dispatches synchronously to the active sink");
    ok &= expect(capture.severity == level::info, "the log level is preserved");
    ok &= expect(capture.source == origin::application, "application is the default log origin");
    ok &= expect(capture.category_matches, "the category is preserved");
    ok &= expect(capture.message_matches, "the message is preserved");
    ok &= expect(capture.location_matches, "source location is captured at the emit call");

    const auto previous = set_sink(nullptr);
    ok &= expect(previous == &capture_sink, "set_sink returns the previous sink");

    emit(level::error, "application.search", "candidate accepted");
    ok &= expect(capture.calls == 1, "a null sink disables logging");

    ok &= expect(level_name(level::trace) == "trace", "trace has a name");
    ok &= expect(level_name(level::debug) == "debug", "debug has a name");
    ok &= expect(level_name(level::info) == "info", "info has a name");
    ok &= expect(level_name(level::warning) == "warning", "warning has a name");
    ok &= expect(level_name(level::error) == "error", "error has a name");
    ok &= expect(
        level_name(static_cast<level>(42)) == "unknown",
        "an out-of-range level is unknown");

    // The default sink writes warnings and errors to stderr and drops the
    // rest; stderr goes to a file for the rest of the test.
    const auto stderr_path =
        std::filesystem::temp_directory_path() / "easylocal-logging-test.txt";
    ok &= expect(
        std::freopen(stderr_path.string().c_str(), "w", stderr) != nullptr,
        "stderr can be redirected");
    (void)set_sink(original);
    emit(level::info, "application.search", "dropped");
    emit(level::warning, "application.search", "low on time");
    emit(level::error, "application.search", "no solution");
    std::fflush(stderr);

    std::ifstream written{stderr_path};
    const std::string output{
        std::istreambuf_iterator<char>{written},
        std::istreambuf_iterator<char>{}};
    ok &= expect(
        output == "EasyLocal warning: low on time\nEasyLocal error: no solution\n",
        "stderr_sink prints warnings and errors, one per line");
    written.close();
    std::error_code ignored;
    std::filesystem::remove(stderr_path, ignored);

    return ok ? 0 : 1;
}
