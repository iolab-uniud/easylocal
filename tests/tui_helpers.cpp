#include <easylocal/adapters/tui/tester.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <locale>
#include <ostream>
#include <string>
#include <string_view>

namespace
{

struct member_described
{
    [[nodiscard]] std::string describe() const
    {
        return "member";
    }
};

[[maybe_unused]] inline std::ostream& operator<<(
    std::ostream& out,
    const member_described&)
{
    return out << "stream";
}

namespace adl_case
{
struct value
{
};

[[nodiscard]] inline std::string describe(const value&)
{
    return "adl";
}

[[maybe_unused]] inline std::ostream& operator<<(std::ostream& out, const value&)
{
    return out << "stream";
}
} // namespace adl_case

struct stream_only
{
};

inline std::ostream& operator<<(std::ostream& out, const stream_only&)
{
    return out << "stream";
}

struct context_probe
{
    bool input{};
    bool solution{};
    bool valid{true};

    [[nodiscard]] bool has_input() const noexcept
    {
        return input;
    }
    [[nodiscard]] bool has_solution() const noexcept
    {
        return solution;
    }
    [[nodiscard]] bool is_valid() const noexcept
    {
        return valid;
    }
};

struct named_value
{
    [[nodiscard]] static constexpr std::string_view name() noexcept
    {
        return "named";
    }
};

struct unnamed_value
{
};

// A decimal comma, as some locales write numbers.
struct comma_numbers : std::numpunct<char>
{
    char do_decimal_point() const override
    {
        return ',';
    }
};

} // namespace

int main()
{
    using easylocal::tui::path_display_mode;
    using easylocal::tui::detail::context_pages_available;
    using easylocal::tui::detail::count_text;
    using easylocal::tui::detail::directory_entries;
    using easylocal::tui::detail::display_path;
    using easylocal::tui::detail::editable_path;
    using easylocal::tui::detail::initial_input_file;
    using easylocal::tui::detail::object_name;
    using easylocal::tui::detail::page_after_solution_change;
    using easylocal::tui::detail::page_index;
    using easylocal::tui::detail::page_scroll_selection;
    using easylocal::tui::detail::path_basename;
    using easylocal::tui::detail::progress_mode;
    using easylocal::tui::detail::progress_ratio;
    using easylocal::tui::detail::progress_snapshot;
    using easylocal::tui::detail::seconds_label;
    using easylocal::tui::detail::seconds_text;
    using easylocal::tui::detail::solution_stage;
    using easylocal::tui::detail::solution_stage_of;
    using easylocal::tui::detail::split_text_lines;
    using easylocal::tui::detail::tester_page;
    using easylocal::tui::detail::value_text;
    using easylocal::tui::detail::wrap_text_lines;

    // The limits of the Run page: spaces around a number are allowed.
    assert(seconds_text("2.5") == 2.5);
    assert(seconds_text(" 10 ") == 10.0);
    assert(seconds_text("0") == 0.0);
    assert(!seconds_text("-1"));
    assert(!seconds_text("soon"));
    assert(!seconds_text("inf"));
    assert(!seconds_text(""));
    assert(count_text("1000") == 1000U);
    assert(count_text("\t7 ") == 7U);
    assert(!count_text("-1"));
    assert(!count_text("1.5"));
    assert(!count_text("many"));
    assert(seconds_label(12.34) == "12.3s");
    // Whatever the global locale.
    {
        const auto previous =
            std::locale::global(std::locale{std::locale::classic(), new comma_numbers});
        const auto label = seconds_label(1.5);
        std::locale::global(previous);
        assert(label == "1.5s");
    }

    assert(value_text(member_described{}) == "member");
    assert(value_text(adl_case::value{}) == "adl");
    assert(value_text(stream_only{}) == "stream");
    const auto lexicographic = easylocal::cost::lexicographic{1, 2};
    assert(value_text(lexicographic) == "[1, 2]");
    const auto hierarchical = easylocal::cost::hierarchical{
        lexicographic,
        3};
    assert(value_text(hierarchical) == "[[1, 2], 3]");
    assert(value_text(easylocal::cost::hierarchical{2, 5}) == "[2, 5]");
    assert(value_text(0.1) == "0.1");
    assert(object_name(named_value{}) == "named");
    assert(object_name(unnamed_value{}) == "<unnamed neighborhood>");

    assert(solution_stage_of(context_probe{}) == solution_stage::needs_input);
    assert(solution_stage_of(context_probe{.input = true}) ==
           solution_stage::needs_solution);
    assert(solution_stage_of(
               context_probe{.input = true, .solution = true, .valid = false}) ==
           solution_stage::invalid_solution);
    assert(solution_stage_of(
               context_probe{.input = true, .solution = true}) ==
           solution_stage::ready);

    assert(!context_pages_available(context_probe{}));
    assert(!context_pages_available(context_probe{.input = true}));
    assert(!context_pages_available(context_probe{.solution = true}));
    assert(context_pages_available(context_probe{.input = true, .solution = true}));
    assert(!context_pages_available(
        context_probe{.input = true, .solution = true, .valid = false}));

    const context_probe empty_context{};
    const context_probe ready_context{.input = true, .solution = true};
    assert(page_after_solution_change(empty_context) == tester_page::solution);
    assert(page_after_solution_change(ready_context) == tester_page::move);
    assert(page_after_solution_change(
               context_probe{.input = true, .solution = true, .valid = false}) ==
           tester_page::solution);
    assert(page_index(tester_page::solution) == 0);
    assert(page_index(tester_page::move) == 1);
    assert(page_index(tester_page::run) == 2);
    assert(path_basename("/tmp/instances/berlin52.tsp") == "berlin52.tsp");
    assert(path_basename("berlin52.tsp") == "berlin52.tsp");
    assert(path_basename("").empty());

    assert((split_text_lines("a\nb") == std::vector<std::string>{"a", "b"}));
    assert((split_text_lines("a\n\nb") == std::vector<std::string>{"a", "", "b"}));
    assert((split_text_lines("a\n") == std::vector<std::string>{"a", ""}));
    assert((split_text_lines("") == std::vector<std::string>{""}));

    assert((wrap_text_lines("0, 1, 2, 3", 5) ==
            std::vector<std::string>{"0, 1,", "2, 3"}));
    assert((wrap_text_lines("hello world", 7) ==
            std::vector<std::string>{"hello", "world"}));
    assert((wrap_text_lines("abc\ndef", 80) ==
            std::vector<std::string>{"abc", "def"}));
    assert((wrap_text_lines("测试测试", 4) ==
            std::vector<std::string>{"测试", "测试"}));

    assert(page_scroll_selection(0, 0, 10) == 0);
    assert(page_scroll_selection(0, 25, 10) == 10);
    assert(page_scroll_selection(10, 25, 10) == 20);
    assert(page_scroll_selection(20, 25, 10) == 24);
    assert(page_scroll_selection(20, 25, -10) == 10);
    assert(page_scroll_selection(5, 25, -10) == 0);

    assert(progress_ratio(progress_snapshot{}) == 0.0F);
    assert(progress_ratio(progress_snapshot{
               .mode = progress_mode::indeterminate,
               .current = 3,
           }) == 0.0F);
    assert(progress_ratio(progress_snapshot{
               .mode = progress_mode::determinate,
               .current = 3,
               .total = 4,
           }) == 0.75F);
    assert(progress_ratio(progress_snapshot{
               .mode = progress_mode::determinate,
               .current = 8,
               .total = 4,
           }) == 1.0F);
    assert(progress_ratio(progress_snapshot{
               .mode = progress_mode::determinate,
               .current = 1,
               .total = 0,
           }) == 0.0F);

    const auto fixture =
        std::filesystem::current_path() / "easylocal-tui-helpers-fixture";
    std::filesystem::remove_all(fixture);
    std::filesystem::create_directories(fixture / "z-dir");
    std::filesystem::create_directories(fixture / "a-dir");
    std::ofstream{fixture / "z.txt"} << 'z';
    std::ofstream{fixture / "a.txt"} << 'a';

    const auto entries = directory_entries(fixture);
    assert(entries.size() == 4);
    assert(entries[0].directory);
    assert(entries[0].path.filename() == "a-dir");
    assert(entries[1].directory);
    assert(entries[1].path.filename() == "z-dir");
    assert(!entries[2].directory);
    assert(entries[2].path.filename() == "a.txt");
    assert(!entries[3].directory);
    assert(entries[3].path.filename() == "z.txt");

    const auto target = fixture / "a.txt";
    assert(display_path(target, path_display_mode::relative, fixture) == "a.txt");
    assert(display_path(target, path_display_mode::absolute, fixture) ==
           target.lexically_normal().string());
    assert(display_path(target, path_display_mode::both, fixture) ==
           "a.txt  [" + target.lexically_normal().string() + "]");
    assert(editable_path(target, path_display_mode::relative, fixture) == "a.txt");
    assert(editable_path(target, path_display_mode::both, fixture) == "a.txt");
    assert(editable_path(target, path_display_mode::absolute, fixture) ==
           target.lexically_normal().string());
    assert(
        initial_input_file({.input_path = "a.txt", .path_base = fixture})
        == target.lexically_normal());
    assert(
        initial_input_file({.input_path = target.string(), .path_base = "elsewhere"})
        == target.lexically_normal());

    std::filesystem::remove_all(fixture);

    // The parameters window: fields from a parameter set, the changes of the
    // edited ones, and the errors with the paths as the window shows them.
    {
        using easylocal::tui::detail::changed_parameters;
        using easylocal::tui::detail::parameter_errors;
        using easylocal::tui::detail::parameter_fields;

        easylocal::runners::SimulatedAnnealingParameters<
            easylocal::runners::temperature::ClassicParameters>
            parameters{};
        easylocal::config::parameter_set set;
        set.add("runners.sa", parameters);

        auto fields = parameter_fields(set, "runners.sa.");
        const auto found = std::ranges::find(
            fields,
            std::string{"runners.sa.temperature.cooling_rate"},
            &easylocal::tui::detail::parameter_field::path);
        assert(found != fields.end());
        auto& cooling = *found;
        assert(cooling.label == "temperature.cooling_rate");
        assert(cooling.text == "0.95");
        assert(cooling.cursor == 4);
        assert(!cooling.description.empty());
        assert(changed_parameters(fields).empty());
        assert(parameter_fields(set, "runners.fi.").empty());

        cooling.text = "2";
        const auto changes = changed_parameters(fields);
        assert(changes.size() == 1);
        assert(changes[0].path == "runners.sa.temperature.cooling_rate");

        const auto rejected = set.apply(changes);
        assert(!rejected);
        assert(parameter_errors(rejected, "runners.sa.")
                .starts_with("temperature.cooling_rate: expected a value in (0, 1)"));
        assert(parameters.temperature.cooling_rate == 0.95);

        cooling.text = "abc";
        const auto malformed = set.apply(changed_parameters(fields));
        assert(parameter_errors(malformed, "runners.sa.")
                .starts_with("temperature.cooling_rate: "));
    }
    return 0;
}
