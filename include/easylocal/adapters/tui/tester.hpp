#pragma once

/// \file
/// The interactive tester (TextUI): an FTXUI frontend on a Session.
///
/// It loads and saves Inputs and Solutions, shows the current solution and its
/// cost, explores and applies moves, edits the parameters and runs the
/// registered runners with progress and cancellation.

#include <easylocal/app/check.hpp>
#include <easylocal/app/io.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameters.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/runners/search_run.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <ftxui/ftxui.hpp>
#include <ftxui/screen/string.hpp>
#include <functional>
#include <future>
#include <iomanip>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <random>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::tui
{

/// How the tester shows a path.
enum class path_display_mode
{
    /// Relative to the base of the paths.
    relative,
    /// Absolute.
    absolute,
    /// Relative, followed by the absolute path in brackets.
    both,
};

/// The options of the interactive tester.
///
/// input_path and solution_path are the initial paths of the Input/Output page,
/// and run() loads the Input from input_path; a relative path is relative to
/// path_base, or to the working directory when path_base is empty.
struct options
{
    /// The title of the window.
    std::string title{"EasyLocal Tester"};
    /// The seed of the session's random generator.
    std::uint64_t seed{};
    /// The Input file, loaded at the start when it is set.
    std::string input_path{};
    /// The initial path of the solution file on the Input/Output page.
    std::string solution_path{};
    /// How paths are shown.
    path_display_mode path_display{path_display_mode::relative};
    /// The directory relative paths start from; empty: the working directory.
    std::filesystem::path path_base{};
    /// The longest text of a solution or a move shown, in characters; longer
    /// ones are truncated.
    std::size_t max_render_chars{4096};
    /// The random moves drawn per valid move by the random distribution check.
    std::size_t random_distribution_rounds{20};
    /// The most entries a diagnostic, such as the list of neighbors, shows.
    std::size_t max_diagnostic_entries{256};
    /// The label of the q key, which leaves the tester.
    std::string exit_label{"quit"};
};

namespace detail
{

template<class T>
concept tuple_like =
    requires { typename std::tuple_size<std::remove_cvref_t<T>>::type; };

template<class T>
concept named_object =
    requires(const T& value) {
        { value.name() } -> std::convertible_to<std::string_view>;
    };

template<class T>
[[nodiscard]] std::string object_name(const T& value)
{
    if constexpr (named_object<T>)
    {
        return std::string{value.name()};
    }
    else
    {
        return "<unnamed neighborhood>";
    }
}

enum class tester_page : int
{
    solution = 0,
    move = 1,
    run = 2,
};

// Where a tester is opened. A launcher's root shows only the Input/Output
// page; its children, one per app, are complete testers. Both start from the
// launcher's shared Input and solution, and what they leave is shared next.
enum class frontend_role
{
    standalone,
    launcher_root,
    launcher_child,
};

enum class progress_mode
{
    unavailable,
    indeterminate,
    determinate,
};

struct progress_snapshot
{
    progress_mode mode{progress_mode::unavailable};
    std::size_t current{};
    std::optional<std::size_t> total{};
    std::string label{};
};

[[nodiscard]] inline float progress_ratio(const progress_snapshot& progress) noexcept
{
    if (progress.mode != progress_mode::determinate ||
        !progress.total.has_value() || *progress.total == 0)
    {
        return 0.0F;
    }
    const auto bounded = (std::min)(progress.current, *progress.total);
    return static_cast<float>(bounded) / static_cast<float>(*progress.total);
}

[[nodiscard]] inline std::string path_basename(std::string_view path)
{
    if (path.empty())
    {
        return {};
    }
    return std::filesystem::path{std::string{path}}.filename().string();
}

enum class solution_stage
{
    needs_input,
    needs_solution,
    invalid_solution,
    ready,
};

[[nodiscard]] constexpr int page_index(const tester_page page) noexcept
{
    return static_cast<int>(page);
}

template<class Tester>
[[nodiscard]] solution_stage solution_stage_of(const Tester& tester)
{
    if (!tester.has_input())
    {
        return solution_stage::needs_input;
    }
    if (!tester.has_solution())
    {
        return solution_stage::needs_solution;
    }
    if constexpr (requires { tester.is_valid(); })
    {
        if (!static_cast<bool>(tester.is_valid()))
        {
            return solution_stage::invalid_solution;
        }
    }
    return solution_stage::ready;
}

template<class Tester>
[[nodiscard]] bool context_pages_available(const Tester& tester)
{
    return solution_stage_of(tester) == solution_stage::ready;
}

template<class Tester>
[[nodiscard]] bool page_available(const Tester& tester, const tester_page page)
{
    return page == tester_page::solution || context_pages_available(tester);
}

template<class Tester>
[[nodiscard]] tester_page page_after_solution_change(const Tester& tester)
{
    return context_pages_available(tester)
               ? tester_page::move
               : tester_page::solution;
}

template<class T>
[[nodiscard]] std::string value_text(const T& value)
{
    if constexpr (easylocal::has_describe<T>)
    {
        return easylocal::describe(value);
    }
    // A cost as the target field reads it: [hard, soft], [v1, v2, ...].
    else if constexpr (easylocal::cost::text_readable<T>)
    {
        return easylocal::cost::to_text(value);
    }
    else if constexpr (easylocal::cost::hierarchical_type<T>)
    {
        return "[" + value_text(value.hard()) + ", " + value_text(value.soft()) + "]";
    }
    else if constexpr (easylocal::cost::lexicographic_type<T>
        || easylocal::cost::pareto_type<T>)
    {
        std::string result{"["};
        [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            std::size_t emitted = 0;
            ((result += (emitted++ == 0 ? "" : ", ") +
                        value_text(value.template get<Index>())),
             ...);
        }(std::make_index_sequence<std::remove_cvref_t<T>::levels>{});
        result += ']';
        return result;
    }
    else if constexpr (easylocal::describable<T>)
    {
        return easylocal::describe(value);
    }
    else if constexpr (requires { value.hard(); value.soft(); })
    {
        return "[" + value_text(value.hard()) + ", " + value_text(value.soft()) + "]";
    }
    else if constexpr (tuple_like<T>)
    {
        std::string result{"["};
        std::size_t index = 0;
        std::apply(
            [&](const auto&... entries) {
                ((result += (index++ == 0 ? "" : ", ") +
                            value_text(entries)),
                 ...);
            },
            value);
        result += ']';
        return result;
    }
    else
    {
        return "<not printable; add describe() or operator<<>";
    }
}

[[nodiscard]] inline std::string truncate_text(std::string value, const std::size_t limit)
{
    if (limit == 0 || value.size() <= limit)
    {
        return value;
    }
    value.resize(limit);
    value += "\n... <truncated>";
    return value;
}

[[nodiscard]] inline std::vector<std::string> split_text_lines(std::string_view value)
{
    std::vector<std::string> lines;
    std::istringstream input{std::string{value}};
    for (std::string line; std::getline(input, line);)
    {
        lines.push_back(std::move(line));
    }
    if (!value.empty() && value.back() == '\n')
    {
        lines.emplace_back();
    }
    if (lines.empty())
    {
        lines.emplace_back();
    }
    return lines;
}

// A text as a column of paragraphs, one per line.
inline ftxui::Element text_lines(const std::string_view value)
{
    ftxui::Elements lines;
    for (auto& line : split_text_lines(value))
        lines.push_back(ftxui::paragraph(std::move(line)));
    return ftxui::vbox(std::move(lines));
}

[[nodiscard]] inline std::vector<std::string> wrap_text_lines(
    std::string_view value,
    const std::size_t width)
{
    const auto effective_width = static_cast<int>(
        std::max<std::size_t>(1, width));
    std::vector<std::string> wrapped;

    const auto trim_left = [](std::string& text) {
        while (!text.empty() &&
               (text.front() == ' ' || text.front() == '\t'))
        {
            text.erase(text.begin());
        }
    };
    const auto trim_right = [](std::string& text) {
        while (!text.empty() &&
               (text.back() == ' ' || text.back() == '\t'))
        {
            text.pop_back();
        }
    };

    for (auto logical_line : split_text_lines(value))
    {
        if (logical_line.empty())
        {
            wrapped.emplace_back();
            continue;
        }

        std::string current;
        std::size_t last_break{};
        for (const auto& glyph : ftxui::Utf8ToGlyphs(logical_line))
        {
            if (current.empty() && (glyph == " " || glyph == "\t"))
            {
                continue;
            }
            while (!current.empty() &&
                   ftxui::string_width(current + glyph) > effective_width)
            {
                if (last_break != 0)
                {
                    auto line = current.substr(0, last_break);
                    trim_right(line);
                    wrapped.push_back(std::move(line));
                    current.erase(0, last_break);
                    trim_left(current);
                }
                else
                {
                    wrapped.push_back(std::move(current));
                    current.clear();
                }
                last_break = 0;
            }

            // The overflow above may have emitted the whole current line.
            // In that case a whitespace glyph that triggered the wrap belongs
            // to the separator, not to the beginning of the next visual line.
            if (current.empty() && (glyph == " " || glyph == "\t"))
            {
                continue;
            }

            current += glyph;
            if (glyph == " " || glyph == "\t" || glyph == "," ||
                glyph == ";")
            {
                last_break = current.size();
            }
        }

        if (!current.empty())
        {
            trim_right(current);
            wrapped.push_back(std::move(current));
        }
    }

    if (wrapped.empty())
    {
        wrapped.emplace_back();
    }
    return wrapped;
}

[[nodiscard]] inline int terminal_available_width(const int margin = 4) noexcept
{
    const auto dimensions = ftxui::Terminal::Size();
    return (std::max)(1, dimensions.dimx - margin);
}

[[nodiscard]] inline int terminal_available_height(const int margin = 2) noexcept
{
    const auto dimensions = ftxui::Terminal::Size();
    return (std::max)(1, dimensions.dimy - margin);
}

[[nodiscard]] constexpr int page_scroll_selection(
    const int selected,
    const std::size_t count,
    const int delta) noexcept
{
    if (count == 0)
    {
        return 0;
    }
    const auto last = static_cast<int>(count - 1);
    return std::clamp(selected + delta, 0, last);
}

struct file_entry
{
    std::filesystem::path path;
    bool directory{};
};

[[nodiscard]] inline std::filesystem::path absolute_path_from(
    const std::filesystem::path& path,
    const std::filesystem::path& base = {})
{
    std::error_code error;
    auto effective_base = base;
    if (effective_base.empty())
    {
        effective_base = std::filesystem::current_path(error);
        if (error)
        {
            return path.lexically_normal();
        }
    }
    else if (!effective_base.is_absolute())
    {
        effective_base = std::filesystem::absolute(effective_base, error);
        if (error)
        {
            return path.lexically_normal();
        }
    }

    if (path.is_absolute())
    {
        return path.lexically_normal();
    }
    return (effective_base / path).lexically_normal();
}

// The Input file the tester loads at start: options.input_path, relative to
// options.path_base as every path of the Input/Output page.
[[nodiscard]] inline std::filesystem::path initial_input_file(const options& settings)
{
    return absolute_path_from(
        std::filesystem::path{settings.input_path},
        settings.path_base);
}

[[nodiscard]] inline std::filesystem::path relative_path_from(
    const std::filesystem::path& path,
    const std::filesystem::path& base = {})
{
    const auto absolute = absolute_path_from(path, base);
    const auto absolute_base = absolute_path_from({}, base);
    const auto relative = absolute.lexically_relative(absolute_base);
    return relative.empty() ? absolute : relative;
}

[[nodiscard]] inline std::string display_path(
    const std::filesystem::path& path,
    const path_display_mode mode,
    const std::filesystem::path& base = {})
{
    if (path.empty())
    {
        return {};
    }

    const auto absolute = absolute_path_from(path, base);
    const auto relative = relative_path_from(path, base);
    switch (mode)
    {
    case path_display_mode::absolute:
        return absolute.string();
    case path_display_mode::both:
        return relative.string() + "  [" + absolute.string() + ']';
    case path_display_mode::relative:
    default:
        return relative.string();
    }
}

[[nodiscard]] inline std::string editable_path(
    const std::filesystem::path& path,
    const path_display_mode mode,
    const std::filesystem::path& base = {})
{
    if (mode == path_display_mode::absolute)
    {
        return absolute_path_from(path, base).string();
    }
    return relative_path_from(path, base).string();
}

[[nodiscard]] inline std::vector<file_entry> directory_entries(
    const std::filesystem::path& directory)
{
    std::error_code error;
    std::filesystem::directory_iterator iterator{directory, error};
    if (error)
    {
        throw std::filesystem::filesystem_error{
            "cannot browse directory",
            directory,
            error};
    }

    std::vector<file_entry> entries;
    for (const auto& entry : iterator)
    {
        std::error_code status_error;
        const bool is_directory = entry.is_directory(status_error);
        if (status_error)
        {
            continue;
        }
        entries.push_back({entry.path(), is_directory});
    }

    std::ranges::sort(
        entries,
        [](const file_entry& lhs, const file_entry& rhs) {
            if (lhs.directory != rhs.directory)
            {
                return lhs.directory > rhs.directory;
            }
            return lhs.path.filename().string() < rhs.path.filename().string();
        });
    return entries;
}

enum class status_kind
{
    info,
    success,
    warning,
    error,
};

enum class file_target
{
    input,
    solution,
};

template<class Solution>
struct async_runner_result
{
    bool cancelled{};
    // Why the run ended and its evaluations, when the runner reports them.
    std::optional<easylocal::run_effort> effort;
    std::optional<Solution> solution;
    std::string error;
};

// How a run ended, for the Last run box: " (target reached, 12 evaluations)";
// " (stopped)" for a cancelled run without an effort, empty otherwise.
[[nodiscard]] inline std::string run_ending(
    const std::optional<easylocal::run_effort>& effort,
    const bool cancelled)
{
    if (!effort)
        return cancelled ? " (stopped)" : "";
    return " (" + std::string{easylocal::to_string(effort->termination)}
    + ", " + std::to_string(effort->evaluations)
        + (effort->evaluations == 1 ? " evaluation)" : " evaluations)");
}

// A number of seconds as text: empty when it is not a non-negative number.
[[nodiscard]] inline std::optional<double> seconds_text(const std::string_view text)
{
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos)
        return std::nullopt;
    const auto last = text.find_last_not_of(" \t");
    double seconds{};
    const auto* const end = text.data() + last + 1;
    const auto [parsed, error] = std::from_chars(text.data() + first, end, seconds);
    if (error != std::errc{} || parsed != end || !(seconds >= 0.0)
        || !std::isfinite(seconds))
        return std::nullopt;
    return seconds;
}

// A count as text: empty when it is not a non-negative whole number.
[[nodiscard]] inline std::optional<std::size_t> count_text(const std::string_view text)
{
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos)
        return std::nullopt;
    const auto last = text.find_last_not_of(" \t");
    std::size_t count{};
    const auto* const end = text.data() + last + 1;
    const auto [parsed, error] = std::from_chars(text.data() + first, end, count);
    if (error != std::errc{} || parsed != end)
        return std::nullopt;
    return count;
}

// Seconds as the progress label shows them: one decimal, such as 12.3s.
[[nodiscard]] inline std::string seconds_label(const double seconds)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << seconds << 's';
    return out.str();
}

// A scrollable text wrapped to a width: the Input and solution windows. The
// lines are wrapped again only when the width changes.
class text_viewer
{
public:
    // Shows a new text from its first line.
    void show(std::string value, const std::size_t width)
    {
        text_ = std::move(value);
        wrap_width_ = 0;
        refresh(width);
        selected_ = 0;
    }

    void refresh(const std::size_t width)
    {
        if (wrap_width_ == width)
            return;
        wrap_width_ = width;
        lines_ = wrap_text_lines(text_, width);
        scroll(0);
    }

    void scroll(const int delta)
    {
        selected_ = page_scroll_selection(selected_, lines_.size(), delta);
    }

    // The lines as a vertical menu, which moves the selected line.
    [[nodiscard]] ftxui::Component menu()
    {
        return ftxui::Menu(&lines_, &selected_, ftxui::MenuOption::Vertical());
    }

private:
    std::string text_;
    std::vector<std::string> lines_{""};
    std::size_t wrap_width_{};
    int selected_{};
};

// One editable parameter of the parameters window: its full configuration
// path, the label shown (the path without its first segment), the value when
// the tester started, the value when the window opened, and the edited text.
struct parameter_field
{
    std::string path;
    std::string label;
    std::string description;
    std::string original;
    std::string current;
    std::string text;
    int cursor{}; // the input's cursor, at the end of the text when it opens
};

// The parameters of a set whose path starts with prefix, as editable fields;
// the label is the path without the prefix when strip is set.
[[nodiscard]] inline std::vector<parameter_field> parameter_fields(
    const easylocal::config::parameter_set& parameters,
    const std::string_view prefix,
    const bool strip = true)
{
    std::vector<parameter_field> fields;
    for (const auto& parameter : parameters.parameters())
    {
        if (!parameter.path.starts_with(prefix))
            continue;
        fields.push_back(
            parameter_field{
                .path = parameter.path,
                .label = strip ? parameter.path.substr(prefix.size()) : parameter.path,
                .description = std::string{parameter.description},
                .original = parameter.value,
                .current = parameter.value,
                .text = parameter.value,
                .cursor = static_cast<int>(parameter.value.size()),
            });
    }
    return fields;
}

// The overrides of the fields whose text was edited; they refer to the
// fields' strings.
[[nodiscard]] inline std::vector<easylocal::config::text_override> changed_parameters(
    const std::vector<parameter_field>& fields)
{
    std::vector<easylocal::config::text_override> overrides;
    for (const auto& field : fields)
        if (field.text != field.current)
            overrides.push_back({.path = field.path, .value = field.text});
    return overrides;
}

// The diagnostics of a rejected change, one per line, with the paths as the
// window shows them (without prefix); a diagnostic about the whole block the
// window shows has no path left, only its message.
[[nodiscard]] inline std::string parameter_errors(
    const easylocal::config::override_result& result,
    const std::string_view prefix = {})
{
    std::string text;
    for (const auto& diagnostic : result.diagnostics)
    {
        if (!text.empty())
            text += '\n';
        std::string_view path = diagnostic.path;
        if (path.starts_with(prefix))
            path.remove_prefix(prefix.size());
        else if (!prefix.empty() && prefix.substr(0, prefix.size() - 1) == path)
            path = {};
        if (!path.empty())
            text += std::string{path} + ": ";
        text += diagnostic.message;
    }
    return text;
}

// What a tester remembers from one opening of the same session to the next:
// its seed and the parameters it started with, which mark the changes.
struct frontend_memory
{
    std::uint64_t seed{};
    std::map<std::string, std::string> original_parameters;
};

template<class App>
class tester_frontend
{
public:
    using tester_type = easylocal::Session<App>;
    static constexpr bool supports_parameters = requires(const tester_type& session) {
        session.configuration();
    };
    // A target cost needs the costs as text: generic, or the problem's read_cost.
    static constexpr bool supports_target = requires(const tester_type& session) {
        session.read_cost(std::string_view{});
    };

    tester_frontend(
        tester_type& tester,
        tui::options options,
        const frontend_role role = frontend_role::standalone,
        const frontend_memory* memory = nullptr)
        : tester_{tester},
          options_{std::move(options)},
          role_{role},
          seed_{options_.seed},
          seed_text_{std::to_string(options_.seed)}
    {
        if (!options_.input_path.empty())
        {
            input_path_ = editable_path(
                options_.input_path,
                options_.path_display,
                options_.path_base);
            add_known_path(file_target::input, input_path_);
            // The session was given the Input of options.input_path.
            if (tester_.has_input())
                loaded_input_path_ = initial_input_file(options_);
        }
        if (!options_.solution_path.empty())
        {
            solution_path_ = editable_path(
                options_.solution_path,
                options_.path_display,
                options_.path_base);
            add_known_path(file_target::solution, solution_path_);
        }
        for (const auto name : tester_.runner_names())
        {
            runner_names_.emplace_back(name);
        }
        if (memory != nullptr)
        {
            original_parameters_ = memory->original_parameters;
            refresh_changed_parameters();
        }
        else
            remember_original_parameters();
        page_selected_ = page_index(page_after_solution_change());
    }

    // What the next opening of the same session starts from.
    [[nodiscard]] frontend_memory memory() const
    {
        return frontend_memory{
            .seed = seed_,
            .original_parameters = original_parameters_};
    }

    // The file of the Input loaded, absolute; empty when the Input was not
    // read from a file.
    [[nodiscard]] const std::filesystem::path& loaded_input_path() const noexcept
    {
        return loaded_input_path_;
    }

    void run()
    {
        using namespace ftxui;

        auto app = ftxui::App::Fullscreen();
        auto input_viewer = viewer_component(
            input_viewer_,
            input_visible_,
            [this](const Component& menu, const Component& close) {
                return render_input_viewer(menu, close);
            },
            [](const Event& event) {
                return event == Event::Escape || event == Event::F1;
            });
        auto solution_viewer = viewer_component(
            solution_viewer_,
            solution_visible_,
            [this](const Component& menu, const Component& close) {
                return render_solution_viewer(menu, close);
            },
            [](const Event& event) {
                return event == Event::Escape || event == Event::F2 || event == Event::s
                    || event == Event::S;
            });

        Component root = Modal(main_window(), browser_window(), &browser_visible_);
        root = Modal(root, help_window(), &help_visible_);
        root = Modal(root, diagnostic_window(), &diagnostic_visible_);
        root = Modal(root, progress_window(), &progress_visible_);
        root = Modal(root, input_viewer, &input_visible_);
        root = Modal(root, solution_viewer, &solution_visible_);
        root = Modal(root, parameters_window(), &parameters_visible_);
        root = CatchEvent(root, [this, &app](const Event& event) {
            return handle_key(app, event);
        });

        // The progress modal normally keeps the loop running until the run
        // completes. However the loop ends, an exception included, the worker
        // is stopped and joined before app is destroyed, since it posts its
        // events to app.
        const worker_guard guard{*this};
        event_app_ = &app;
        app.Loop(root);
    }

private:
    // Stops and joins the run worker, and forgets the event loop, when the
    // loop is left.
    class worker_guard
    {
    public:
        explicit worker_guard(tester_frontend& frontend) noexcept : frontend_{frontend} {}

        worker_guard(const worker_guard&) = delete;
        worker_guard& operator=(const worker_guard&) = delete;

        ~worker_guard()
        {
            if (frontend_.run_worker_.joinable())
            {
                frontend_.run_worker_.request_stop();
                frontend_.run_worker_.join();
            }
            frontend_.event_app_ = nullptr;
        }

    private:
        tester_frontend& frontend_;
    };

    // A bold label over a group of controls.
    [[nodiscard]] static ftxui::Component section_label(std::string label)
    {
        return ftxui::Renderer([label = std::move(label)] {
            return ftxui::text(label) | ftxui::bold;
        });
    }

    // The Solution page: the instances, the solutions created, loaded and
    // saved, and the check.
    [[nodiscard]] ftxui::Component solution_page()
    {
        using namespace ftxui;
        auto solution_controls = Container::Vertical({});
        if constexpr (tester_type::supports_input_loading)
        {
            solution_controls->Add(section_label("Instances"));
            auto instance_menu_option = MenuOption::Vertical();
            instance_menu_option.on_change = [this] { select_known_input(); };
            instance_menu_option.on_enter = [this] { load_input(); };
            auto instance_menu = Menu(
                &known_input_labels_,
                &known_input_selected_,
                instance_menu_option);
            solution_controls->Add(instance_menu);
            solution_controls->Add(Container::Horizontal({
                Button(
                    "L Load selected",
                    [this] { load_input(); },
                    ButtonOption::Ascii()),
                Button(
                    "Browse...",
                    [this] { open_browser(file_target::input); },
                    ButtonOption::Ascii()),
            }));
        }

        if constexpr (tester_type::supports_initial_solution ||
                      tester_type::supports_random_solution)
        {
            solution_controls->Add(section_label("Create solution"));
            std::vector<Component> source_actions;
            if constexpr (tester_type::supports_initial_solution)
            {
                source_actions.push_back(Button(
                    "I Initial",
                    [this] { use_initial_solution(); },
                    ButtonOption::Ascii()));
            }
            if constexpr (tester_type::supports_random_solution)
            {
                source_actions.push_back(Button(
                    "R Random",
                    [this] { use_random_solution(); },
                    ButtonOption::Ascii()));
            }
            solution_controls->Add(Container::Horizontal(std::move(source_actions)));
        }

        if constexpr (tester_type::supports_solution_loading ||
                      tester_type::supports_solution_saving)
        {
            solution_controls->Add(section_label("Solutions"));
            auto solution_menu_option = MenuOption::Vertical();
            solution_menu_option.on_change = [this] { select_known_solution(); };
            if constexpr (tester_type::supports_solution_loading)
            {
                solution_menu_option.on_enter = [this] { load_solution(); };
            }
            auto solution_menu = Menu(
                &known_solution_labels_,
                &known_solution_selected_,
                solution_menu_option);
            solution_controls->Add(solution_menu);

            std::vector<Component> file_actions;
            file_actions.push_back(Button(
                "Browse...",
                [this] { open_browser(file_target::solution); },
                ButtonOption::Ascii()));
            if constexpr (tester_type::supports_solution_loading)
            {
                file_actions.push_back(Button(
                    "Shift-L Load",
                    [this] { load_solution(); },
                    ButtonOption::Ascii()));
            }
            if constexpr (tester_type::supports_solution_saving)
            {
                file_actions.push_back(Button(
                    "W Save",
                    [this] { save_solution(); },
                    ButtonOption::Ascii()));
            }
            solution_controls->Add(Container::Horizontal(std::move(file_actions)));
        }

        solution_controls->Add(section_label("Diagnostics"));
        solution_controls->Add(Button(
            "C Check",
            [this] { check(); },
            ButtonOption::Ascii()));

        return Renderer(solution_controls, [this, solution_controls] {
            return render_solution_page(solution_controls);
        });
    }

    // The Move page: the selection and application of a move, and the
    // neighborhood diagnostics.
    [[nodiscard]] ftxui::Component move_page()
    {
        using namespace ftxui;
        auto move_controls = Container::Vertical({});
        move_controls->Add(section_label("Select move"));
        if constexpr (tester_type::supports_improvement_selection)
        {
            move_controls->Add(Button(
                "B Best",
                [this] { best_move(); },
                ButtonOption::Ascii()));
            move_controls->Add(Button(
                "I First improving",
                [this] { first_improving_move(); },
                ButtonOption::Ascii()));
        }
        if constexpr (tester_type::supports_deterministic_moves)
        {
            move_controls->Add(Button(
                "F First",
                [this] { first_move(); },
                ButtonOption::Ascii()));
            move_controls->Add(Button(
                "N Next",
                [this] { next_move(); },
                ButtonOption::Ascii()));
        }
        if constexpr (tester_type::supports_random_moves)
        {
            move_controls->Add(Button(
                "R Random",
                [this] { random_move(); },
                ButtonOption::Ascii()));
        }
        move_controls->Add(section_label("Commit"));
        move_controls->Add(Button(
            "A Apply",
            [this] { apply_move(); },
            ButtonOption::Ascii()));

        auto move_diagnostics = Container::Horizontal({});
        if constexpr (tester_type::supports_deterministic_moves)
        {
            move_diagnostics->Add(
                Button("P List", [this] { preview_neighbors(); }, ButtonOption::Ascii()));
        }
        if constexpr (tester_type::supports_improvement_selection)
        {
            move_diagnostics->Add(Button(
                "T Stats",
                [this] { neighborhood_statistics(); },
                ButtonOption::Ascii()));
        }
        if constexpr (tester_type::supports_cost_consistency_check)
        {
            move_diagnostics->Add(Button(
                "C Costs",
                [this] { check_neighborhood_costs(); },
                ButtonOption::Ascii()));
        }
        if constexpr (tester_type::supports_move_independence_check)
        {
            move_diagnostics->Add(Button(
                "D Indep",
                [this] { check_move_independence(); },
                ButtonOption::Ascii()));
        }
        if constexpr (tester_type::supports_random_distribution_check)
        {
            move_diagnostics->Add(Button(
                "U Distribution",
                [this] { check_random_distribution(); },
                ButtonOption::Ascii()));
        }

        auto move_page_controls = Container::Vertical({move_controls, move_diagnostics});
        return Renderer(move_page_controls, [this, move_controls, move_diagnostics] {
            return render_move_page(move_controls, move_diagnostics);
        });
    }

    // The Run page: the runners, the problem's parameters, the seed and the
    // target cost.
    [[nodiscard]] ftxui::Component run_page()
    {
        using namespace ftxui;
        auto run_controls = Container::Vertical({});
        if (!runner_names_.empty())
        {
            run_controls->Add(section_label("Runner"));
            auto menu_option = MenuOption::Vertical();
            menu_option.on_enter = [this] { run_runner(); };
            run_controls->Add(Menu(&runner_names_, &runner_selected_, menu_option));
            run_controls->Add(section_label("Execute"));
            run_controls->Add(Button(
                "G Run selected",
                [this] { run_runner(); },
                ButtonOption::Ascii()));
            run_controls->Add(Button(
                "X Stop running",
                [this] { stop_runner(); },
                ButtonOption::Ascii()));
        }
        else
        {
            run_controls->Add(Renderer([] {
                return text("No runner registered");
            }));
        }
        if constexpr (supports_parameters)
        {
            run_controls->Add(section_label("Problem"));
            run_controls->Add(Button(
                "P Problem parameters",
                [this] { open_problem_parameters(); },
                ButtonOption::Ascii()));
        }
        // The seed restarts the RNG used for random solutions, random moves
        // and stochastic runners.
        run_controls->Add(section_label("Random seed"));
        auto seed_input_option = InputOption::Default();
        seed_input_option.multiline = false;
        seed_input_option.on_enter = [this] { apply_seed(); };
        seed_input_ = Input(&seed_text_, "seed", seed_input_option);
        run_controls->Add(
            Container::Horizontal({
                seed_input_,
                Button(
                    "Apply seed",
                    [this] { apply_seed(); },
                    ButtonOption::Ascii()),
            }));

        // A run stops when its solution reaches the target cost; empty: none.
        if constexpr (supports_target)
        {
            run_controls->Add(section_label("Target cost"));
            auto target_input_option = InputOption::Default();
            target_input_option.multiline = false;
            target_input_ = Input(&target_text_, "none", target_input_option);
            run_controls->Add(target_input_);
            // The current cost as the field reads it, as an example of the syntax.
            if constexpr (easylocal::cost::text_readable<typename tester_type::cost_type>)
            {
                run_controls->Add(Renderer([this] {
                    if (!tester_.has_solution() || !tester_.is_valid())
                        return text("");
                    return text(
                               "current cost: "
                               + easylocal::cost::to_text(tester_.evaluate()))
                        | dim;
                }));
            }
        }

        // A run stops after this many seconds, or this many evaluations; empty:
        // no limit beyond the runner's own. One row, so that the page keeps its
        // height.
        run_controls->Add(section_label("Stop after"));
        auto limit_input_option = InputOption::Default();
        limit_input_option.multiline = false;
        timeout_input_ = Input(&timeout_text_, "none", limit_input_option);
        evaluations_input_ = Input(&evaluations_text_, "none", limit_input_option);
        auto limit_inputs = Container::Horizontal({timeout_input_, evaluations_input_});
        run_controls->Add(Renderer(limit_inputs, [this] {
            return hbox({
                text("seconds "),
                timeout_input_->Render() | size(WIDTH, EQUAL, 12),
                text("  evaluations "),
                evaluations_input_->Render() | size(WIDTH, EQUAL, 14),
            });
        }));

        return Renderer(run_controls, [this, run_controls] {
            return render_run_page(run_controls);
        });
    }

    // The three pages under their menu.
    [[nodiscard]] ftxui::Component main_window()
    {
        using namespace ftxui;
        auto pages =
            Container::Tab({solution_page(), move_page(), run_page()}, &page_selected_);
        refresh_page_labels();
        auto page_menu_option = MenuOption::Horizontal();
        page_menu_option.on_change = [this] {
            if (page_selected_ != 0 && !context_pages_available())
            {
                page_selected_ = 0;
                set_status(
                    status_kind::warning,
                    role_ == frontend_role::launcher_root
                        ? "Move and Run are in the applications: go back and open one"
                        : "Move and Run require a loaded instance and a valid solution");
            }
        };
        auto page_menu = Menu(&page_labels_, &page_selected_, page_menu_option);
        auto main_controls = Container::Vertical({page_menu, pages});

        return Renderer(main_controls, [this, page_menu, pages] {
            return render_main(page_menu, pages);
        });
    }

    // The file browser of the Input/Output page.
    [[nodiscard]] ftxui::Component browser_window()
    {
        using namespace ftxui;
        auto browser_menu_option = MenuOption::Vertical();
        browser_menu_option.on_enter = [this] { accept_browser_selection(); };
        auto browser_menu = Menu(
            &browser_labels_,
            &browser_selected_,
            browser_menu_option);
        auto browser_buttons = Container::Horizontal({
            Button(
                "Open",
                [this] { accept_browser_selection(); },
                ButtonOption::Ascii()),
            Button(
                "Up",
                [this] { browser_up(); },
                ButtonOption::Ascii()),
            Button(
                "Cancel",
                [this] { browser_visible_ = false; },
                ButtonOption::Ascii()),
        });
        auto browser_controls = Container::Vertical({browser_menu, browser_buttons});
        auto browser_renderer =
            Renderer(browser_controls, [this, browser_menu, browser_buttons] {
                return render_browser(browser_menu, browser_buttons);
            });
        browser_renderer = CatchEvent(browser_renderer, [this](Event event) {
            if (event == Event::Escape)
            {
                browser_visible_ = false;
                return true;
            }
            if (event == Event::Backspace)
            {
                browser_up();
                return true;
            }
            return false;
        });
        return browser_renderer;
    }

    [[nodiscard]] ftxui::Component help_window()
    {
        using namespace ftxui;
        auto help_controls = Container::Vertical({
            Button(
                "Close",
                [this] { help_visible_ = false; },
                ButtonOption::Ascii()),
        });
        auto help_renderer = Renderer(help_controls, [this, help_controls] {
            return render_help(help_controls);
        });
        help_renderer = CatchEvent(help_renderer, [this](Event event) {
            if (event == Event::Escape || event == Event::Character('?') ||
                event == Event::h || event == Event::H)
            {
                help_visible_ = false;
                return true;
            }
            return false;
        });
        return help_renderer;
    }

    // The result of a diagnostic, one line per row.
    [[nodiscard]] ftxui::Component diagnostic_window()
    {
        using namespace ftxui;
        auto diagnostic_menu_option = MenuOption::Vertical();
        auto diagnostic_menu = Menu(
            &diagnostic_lines_,
            &diagnostic_selected_,
            diagnostic_menu_option);
        auto diagnostic_close = Button(
            "Close",
            [this] { diagnostic_visible_ = false; },
            ButtonOption::Ascii());
        auto diagnostic_controls = Container::Vertical({
            diagnostic_menu,
            diagnostic_close,
        });
        auto diagnostic_renderer = Renderer(
            diagnostic_controls,
            [this, diagnostic_menu, diagnostic_close] {
                return render_diagnostic_viewer(
                    diagnostic_menu, diagnostic_close);
            });
        diagnostic_renderer = CatchEvent(diagnostic_renderer, [this](Event event) {
            if (event == Event::PageUp || event == Event::PageDown)
            {
                diagnostic_selected_ = detail::page_scroll_selection(
                    diagnostic_selected_,
                    diagnostic_lines_.size(),
                    event == Event::PageUp ? -10 : 10);
                return true;
            }
            if (event == Event::Escape || event == Event::q || event == Event::Q)
            {
                diagnostic_visible_ = false;
                return true;
            }
            return false;
        });
        return diagnostic_renderer;
    }

    // The progress of a running runner, with its Stop button.
    [[nodiscard]] ftxui::Component progress_window()
    {
        using namespace ftxui;
        auto progress_stop = Button(
            "X Stop",
            [this] { stop_runner(); },
            ButtonOption::Ascii());
        auto progress_controls = Container::Vertical({progress_stop});
        auto progress_renderer = Renderer(
            progress_controls,
            [this, progress_stop] {
                return render_progress_modal(progress_stop);
            });
        progress_renderer = CatchEvent(progress_renderer, [this](Event event) {
            if (event == Event::x || event == Event::X || event == Event::Escape)
            {
                stop_runner();
                return true;
            }
            return false;
        });
        return progress_renderer;
    }

    // The parameters window, filled when it opens.
    [[nodiscard]] ftxui::Component parameters_window()
    {
        using namespace ftxui;
        parameter_inputs_ = Container::Vertical({});
        auto parameter_submit = Button(
            &parameter_action_,
            [this] { submit_parameters(); },
            ButtonOption::Ascii());
        auto parameter_cancel = Button(
            "Cancel",
            [this] { parameters_visible_ = false; },
            ButtonOption::Ascii());
        auto parameter_buttons =
            Container::Horizontal({parameter_submit, parameter_cancel});
        auto parameter_controls =
            Container::Vertical({parameter_inputs_, parameter_buttons});
        auto parameter_renderer = Renderer(parameter_controls, [this, parameter_buttons] {
            return render_parameters(parameter_buttons);
        });
        parameter_renderer = CatchEvent(parameter_renderer, [this](Event event) {
            if (event == Event::Escape)
            {
                parameters_visible_ = false;
                return true;
            }
            return false;
        });
        return parameter_renderer;
    }

    // The keys of the whole tester: progress events, help, the windows (F1,
    // F2), the pages (F3-F5), quit, and the shortcuts of the current page.
    bool handle_key(ftxui::App& app, const ftxui::Event& event)
    {
        using namespace ftxui;
        if (event == Event::Custom && run_future_.valid())
        {
            refresh_runner_progress();
            if (run_future_.wait_for(std::chrono::seconds{0})
                == std::future_status::ready)
            {
                finish_runner_run();
            }
            return true;
        }
        if (parameters_visible_)
            return false;
        const bool help_key =
            event == Event::Character('?') || event == Event::h || event == Event::H;
        if (help_visible_ && help_key)
        {
            help_visible_ = false;
            return true;
        }
        if (browser_visible_ || help_visible_ || diagnostic_visible_ || progress_visible_)
            return false;

        if (input_visible_ || solution_visible_)
            return false;

        const bool editing_text = (seed_input_ && seed_input_->Focused())
            || (target_input_ && target_input_->Focused())
            || (timeout_input_ && timeout_input_->Focused())
            || (evaluations_input_ && evaluations_input_->Focused());

        // In a text field, ? and h are characters of the text.
        if (help_key && !editing_text)
        {
            help_visible_ = true;
            return true;
        }

        if (event == Event::F1)
        {
            show_input();
            return true;
        }
        if (event == Event::F2)
        {
            show_solution();
            return true;
        }
        if (event == Event::F3)
        {
            select_page(tester_page::solution);
            return true;
        }
        if (event == Event::F4)
        {
            select_page(tester_page::move);
            return true;
        }
        if (event == Event::F5)
        {
            select_page(tester_page::run);
            return true;
        }

        if (editing_text)
            return false;

        if (event == Event::q || event == Event::Q)
        {
            app.Exit();
            return true;
        }
        if (event == Event::s || event == Event::S)
        {
            show_solution();
            return true;
        }

        return handle_page_shortcut(event);
    }
    void add_known_path(file_target target, const std::string& path)
    {
        if (path.empty())
            return;
        auto& paths = target == file_target::input ? known_input_paths_ : known_solution_paths_;
        auto& labels = target == file_target::input ? known_input_labels_ : known_solution_labels_;
        auto& selected = target == file_target::input ? known_input_selected_ : known_solution_selected_;
        const auto found = std::find(paths.begin(), paths.end(), path);
        if (found == paths.end())
        {
            paths.push_back(path);
            const auto basename = path_basename(path);
            labels.push_back(basename.empty() ? path : basename);
            selected = static_cast<int>(paths.size() - 1);
        }
        else
        {
            selected = static_cast<int>(found - paths.begin());
        }
    }

    void select_known_input()
    {
        if (known_input_selected_ >= 0 &&
            static_cast<std::size_t>(known_input_selected_) < known_input_paths_.size())
            input_path_ = known_input_paths_[static_cast<std::size_t>(known_input_selected_)];
    }

    void select_known_solution()
    {
        if (known_solution_selected_ >= 0 &&
            static_cast<std::size_t>(known_solution_selected_) < known_solution_paths_.size())
            solution_path_ = known_solution_paths_[static_cast<std::size_t>(known_solution_selected_)];
    }

    [[nodiscard]] tester_page current_page() const noexcept
    {
        switch (page_selected_)
        {
        case 1:
            return tester_page::move;
        case 2:
            return tester_page::run;
        case 0:
        default:
            return tester_page::solution;
        }
    }

    [[nodiscard]] std::string current_page_shortcuts() const
    {
        std::string result;
        const auto append = [&result](std::string_view item) {
            if (!result.empty())
            {
                result += "  ";
            }
            result += item;
        };

        switch (current_page())
        {
        case tester_page::solution:
            if constexpr (tester_type::supports_input_loading)
                append("L Load input");
            if constexpr (tester_type::supports_initial_solution)
                append("I Initial");
            if constexpr (tester_type::supports_random_solution)
                append("R Random");
            if constexpr (tester_type::supports_solution_loading)
                append("Shift-L Load solution");
            if constexpr (tester_type::supports_solution_saving)
                append("W Save");
            append("C Check");
            break;
        case tester_page::move:
            if constexpr (tester_type::supports_improvement_selection)
            {
                append("B Best");
                append("I Improve");
            }
            if constexpr (tester_type::supports_deterministic_moves)
            {
                append("F First");
                append("N Next");
            }
            if constexpr (tester_type::supports_random_moves)
                append("R Random");
            append("A Apply");
            if constexpr (tester_type::supports_deterministic_moves)
                append("P List");
            if constexpr (tester_type::supports_improvement_selection)
                append("T Stats");
            if constexpr (tester_type::supports_cost_consistency_check)
                append("C Costs");
            if constexpr (tester_type::supports_move_independence_check)
                append("D Indep");
            if constexpr (tester_type::supports_random_distribution_check)
                append("U Dist");
            break;
        case tester_page::run:
            append("G Run selected");
            append("Enter Run");
            if constexpr (supports_parameters)
                append("P Problem parameters");
            break;
        }
        return result;
    }

    [[nodiscard]] bool handle_page_shortcut(const ftxui::Event& event)
    {
        switch (current_page())
        {
        case tester_page::solution:
            return handle_solution_shortcut(event);
        case tester_page::move:
            return handle_move_shortcut(event);
        case tester_page::run:
            return handle_run_shortcut(event);
        }
        return false;
    }

    [[nodiscard]] bool handle_solution_shortcut(const ftxui::Event& event)
    {
        if constexpr (tester_type::supports_input_loading)
        {
            if (event == ftxui::Event::l)
            {
                load_input();
                return true;
            }
        }
        if constexpr (tester_type::supports_initial_solution)
        {
            if (event == ftxui::Event::i || event == ftxui::Event::I)
            {
                use_initial_solution();
                return true;
            }
        }
        if constexpr (tester_type::supports_random_solution)
        {
            if (event == ftxui::Event::r || event == ftxui::Event::R)
            {
                use_random_solution();
                return true;
            }
        }
        if constexpr (tester_type::supports_solution_loading)
        {
            if (event == ftxui::Event::L)
            {
                load_solution();
                return true;
            }
        }
        if constexpr (tester_type::supports_solution_saving)
        {
            if (event == ftxui::Event::w || event == ftxui::Event::W)
            {
                save_solution();
                return true;
            }
        }
        if (event == ftxui::Event::c || event == ftxui::Event::C)
        {
            check();
            return true;
        }
        return false;
    }

    [[nodiscard]] bool handle_move_shortcut(const ftxui::Event& event)
    {
        if constexpr (tester_type::supports_improvement_selection)
        {
            if (event == ftxui::Event::b || event == ftxui::Event::B)
            {
                best_move();
                return true;
            }
            if (event == ftxui::Event::i || event == ftxui::Event::I)
            {
                first_improving_move();
                return true;
            }
        }
        if constexpr (tester_type::supports_deterministic_moves)
        {
            if (event == ftxui::Event::f || event == ftxui::Event::F)
            {
                first_move();
                return true;
            }
            if (event == ftxui::Event::n || event == ftxui::Event::N)
            {
                next_move();
                return true;
            }
        }
        if constexpr (tester_type::supports_random_moves)
        {
            if (event == ftxui::Event::r || event == ftxui::Event::R)
            {
                random_move();
                return true;
            }
        }
        if constexpr (tester_type::supports_deterministic_moves)
        {
            if (event == ftxui::Event::p || event == ftxui::Event::P)
            {
                preview_neighbors();
                return true;
            }
        }
        if constexpr (tester_type::supports_improvement_selection)
        {
            if (event == ftxui::Event::t || event == ftxui::Event::T)
            {
                neighborhood_statistics();
                return true;
            }
        }
        if constexpr (tester_type::supports_cost_consistency_check)
        {
            if (event == ftxui::Event::c || event == ftxui::Event::C)
            {
                check_neighborhood_costs();
                return true;
            }
        }
        if constexpr (tester_type::supports_move_independence_check)
        {
            if (event == ftxui::Event::d || event == ftxui::Event::D)
            {
                check_move_independence();
                return true;
            }
        }
        if constexpr (tester_type::supports_random_distribution_check)
        {
            if (event == ftxui::Event::u || event == ftxui::Event::U)
            {
                check_random_distribution();
                return true;
            }
        }
        if (event == ftxui::Event::a || event == ftxui::Event::A)
        {
            apply_move();
            return true;
        }
        return false;
    }

    [[nodiscard]] bool handle_run_shortcut(const ftxui::Event& event)
    {
        if (event == ftxui::Event::g || event == ftxui::Event::G)
        {
            run_runner();
            return true;
        }
        if constexpr (supports_parameters)
        {
            if (event == ftxui::Event::p || event == ftxui::Event::P)
            {
                open_problem_parameters();
                return true;
            }
        }
        return false;
    }

    template<class Function>
    void perform(std::string_view label, Function&& function)
    {
        try
        {
            std::forward<Function>(function)();
        }
        catch (const std::exception& error)
        {
            set_status(
                status_kind::error,
                std::string{label} + ": " + error.what());
        }
        catch (...)
        {
            set_status(
                status_kind::error,
                std::string{label} + ": unknown error");
        }
    }

    void set_status(status_kind kind, std::string text)
    {
        status_kind_ = kind;
        status_ = std::move(text);
    }

    [[nodiscard]] bool require_input(std::string_view action)
    {
        if (tester_.has_input())
        {
            return true;
        }
        set_status(
            status_kind::warning,
            std::string{action} + ": load an input first");
        return false;
    }

    [[nodiscard]] bool context_pages_available() const noexcept
    {
        return role_ != frontend_role::launcher_root
            && detail::context_pages_available(tester_);
    }

    [[nodiscard]] tester_page page_after_solution_change() const
    {
        return role_ == frontend_role::launcher_root
            ? tester_page::solution
            : detail::page_after_solution_change(tester_);
    }

    void refresh_page_labels()
    {
        const bool enabled = context_pages_available();
        page_labels_[0] = "Input/Output";
        page_labels_[1] = enabled ? "Move" : "Move [disabled]";
        page_labels_[2] = enabled ? "Run" : "Run [disabled]";
        if (!enabled && page_selected_ != 0)
        {
            page_selected_ = 0;
        }
    }

    void select_page(const tester_page page)
    {
        if (page == tester_page::solution)
        {
            page_selected_ = page_index(page);
            return;
        }
        if (!context_pages_available())
        {
            page_selected_ = page_index(tester_page::solution);
            set_status(
                status_kind::warning,
                role_ == frontend_role::launcher_root
                    ? "Move and Run are in the applications: go back and open one"
                    : "Move and Run require a loaded instance and a valid solution");
            return;
        }
        page_selected_ = page_index(page);
    }

    [[nodiscard]] bool require_solution(std::string_view action)
    {
        if (tester_.has_solution())
        {
            return true;
        }
        set_status(
            status_kind::warning,
            std::string{action} + ": choose or load a solution first");
        return false;
    }

    [[nodiscard]] bool require_move(std::string_view action)
    {
        if (tester_.has_move())
        {
            return true;
        }
        set_status(
            status_kind::warning,
            std::string{action} + ": select a move first");
        return false;
    }

    void load_input()
        requires tester_type::supports_input_loading
    {
        if (input_path_.empty())
        {
            set_status(status_kind::warning, "Load input: enter or browse a file name");
            return;
        }
        perform("Load input", [this] {
            const auto path = resolve_path(input_path_);
            tester_.load_input(path);
            loaded_input_path_ = path;
            last_move_result_.clear();
            last_run_result_.clear();
            refresh_page_labels();
            page_selected_ = page_index(tester_page::solution);
            set_status(
                status_kind::success,
                "Loaded input: " + format_path(path) + "; choose a solution");
        });
    }

    void use_initial_solution()
        requires tester_type::supports_initial_solution
    {
        if (!require_input("Initial solution"))
        {
            return;
        }
        perform("Initial solution", [this] {
            tester_.use_initial_solution();
            after_solution_change();
            set_status(status_kind::success, solution_status("Initial solution selected"));
        });
    }

    void use_random_solution()
        requires tester_type::supports_random_solution
    {
        if (!require_input("Random solution"))
        {
            return;
        }
        perform("Random solution", [this] {
            tester_.use_random_solution(tester_.rng());
            after_solution_change();
            set_status(status_kind::success, solution_status("Random solution selected"));
        });
    }

    void load_solution()
        requires tester_type::supports_solution_loading
    {
        if (!require_input("Load solution"))
        {
            return;
        }
        if (solution_path_.empty())
        {
            set_status(status_kind::warning, "Load solution: enter or browse a file name");
            return;
        }
        perform("Load solution", [this] {
            const auto path = resolve_path(solution_path_);
            tester_.load_solution(path);
            after_solution_change();
            set_status(
                status_kind::success,
                solution_status("Loaded solution: " + format_path(path)));
        });
    }

    void save_solution()
        requires tester_type::supports_solution_saving
    {
        if (!require_solution("Save solution"))
        {
            return;
        }
        if (solution_path_.empty())
        {
            set_status(status_kind::warning, "Save solution: enter or browse a file name");
            return;
        }
        perform("Save solution", [this] {
            const auto path = resolve_path(solution_path_);
            tester_.save_solution(path);
            set_status(status_kind::success, "Saved solution: " + format_path(path));
        });
    }

    [[nodiscard]] std::filesystem::path resolve_path(std::string_view value) const
    {
        return absolute_path_from(std::filesystem::path{value}, options_.path_base);
    }

    [[nodiscard]] std::string format_path(const std::filesystem::path& path) const
    {
        return display_path(path, options_.path_display, options_.path_base);
    }

    void after_solution_change()
    {
        last_move_result_.clear();
        last_run_result_.clear();
        refresh_page_labels();
        page_selected_ = page_index(page_after_solution_change());
    }

    void show_input()
    {
        input_viewer_.show(input_text(), viewer_wrap_width());
        solution_visible_ = false;
        input_visible_ = true;
    }

    void show_solution()
    {
        solution_viewer_.show(solution_text(), viewer_wrap_width());
        input_visible_ = false;
        solution_visible_ = true;
    }

    [[nodiscard]] std::size_t viewer_wrap_width() const noexcept
    {
        // Reserve room for the modal borders, menu selection marker, and the
        // vertical scroll indicator.  The value is recomputed on every redraw
        // after a terminal resize.
        const auto available = detail::terminal_available_width();
        return static_cast<std::size_t>((std::max)(1, available - 8));
    }

    // The window of a text viewer: its lines as a scrollable menu and a Close
    // button, drawn by render(menu, close) and closed by the keys of closes.
    template<class Render, class Closes>
    [[nodiscard]] ftxui::Component viewer_component(
        detail::text_viewer& viewer,
        bool& visible,
        Render render,
        Closes closes)
    {
        using namespace ftxui;
        auto menu = viewer.menu();
        auto close =
            Button("Close", [&visible] { visible = false; }, ButtonOption::Ascii());
        auto component = Renderer(
            Container::Vertical({menu, close}),
            [this, &viewer, menu, close, render] {
                viewer.refresh(viewer_wrap_width());
                return render(menu, close);
            });
        return CatchEvent(component, [&viewer, &visible, closes](Event event) {
            if (event == Event::PageUp || event == Event::PageDown)
            {
                viewer.scroll(event == Event::PageUp ? -10 : 10);
                return true;
            }
            if (closes(event))
            {
                visible = false;
                return true;
            }
            return false;
        });
    }

    void check()
    {
        if (!require_solution("Check"))
        {
            return;
        }
        perform("Check", [this] {
            const auto report = tester_.check();
            if (report.passed())
            {
                set_status(
                    status_kind::success,
                    "Check passed: " + std::to_string(report.checks()) +
                        " semantic checks");
                return;
            }

            std::string message =
                "Check FAILED: " + std::to_string(report.failures().size()) +
                " failure(s) in " + std::to_string(report.checks()) + " checks";
            if (!report.failures().empty())
            {
                const auto& first = report.failures().front();
                message += "; first: " + first.check + " - " + first.message;
            }
            set_status(status_kind::error, std::move(message));
        });
    }

    void best_move()
        requires tester_type::supports_improvement_selection
    {
        if (!require_solution("Best move"))
            return;
        perform("Best move", [this] {
            if (tester_.use_best_move())
            {
                last_move_result_.clear();
                set_status(status_kind::success, move_status("Selected best move"));
            }
            else
                set_status(status_kind::warning, "Best move: neighborhood is empty");
        });
    }

    void first_improving_move()
        requires tester_type::supports_improvement_selection
    {
        if (!require_solution("First improving move"))
            return;
        perform("First improving move", [this] {
            if (tester_.use_first_improving_move())
            {
                last_move_result_.clear();
                set_status(status_kind::success, move_status("Selected first improving move"));
            }
            else
                set_status(status_kind::warning, "First improving move: none found");
        });
    }

    void first_move()
        requires tester_type::supports_deterministic_moves
    {
        if (!require_solution("First move"))
        {
            return;
        }
        perform("First move", [this] {
            if (tester_.use_first_move())
            {
                last_move_result_.clear();
                set_status(status_kind::success, move_status("Selected first move"));
            }
            else
            {
                set_status(status_kind::warning, "First move: neighborhood is empty");
            }
        });
    }

    void next_move()
        requires tester_type::supports_deterministic_moves
    {
        if (!require_solution("Next move"))
        {
            return;
        }
        if (!tester_.has_move())
        {
            first_move();
            return;
        }
        perform("Next move", [this] {
            if (tester_.use_next_move())
            {
                last_move_result_.clear();
                set_status(status_kind::success, move_status("Selected next move"));
            }
            else
            {
                set_status(
                    status_kind::warning,
                    "Next move: no deterministic successor; use First to restart");
            }
        });
    }

    void random_move()
        requires tester_type::supports_random_moves
    {
        if (!require_solution("Random move"))
        {
            return;
        }
        perform("Random move", [this] {
            if (tester_.use_random_move(tester_.rng()))
            {
                last_move_result_.clear();
                set_status(status_kind::success, move_status("Selected random move"));
            }
            else
            {
                set_status(
                    status_kind::warning,
                    "Random move: neighborhood produced no move");
            }
        });
    }

    void preview_neighbors()
        requires tester_type::supports_deterministic_moves
    {
        if (!require_solution("List neighbors"))
            return;
        perform("List neighbors", [this] {
            const auto result = tester_.neighborhood_preview(
                options_.max_diagnostic_entries);
            std::ostringstream out;
            out << "Neighbors: " << result.moves;
            if (result.invalid > 0)
                out << " (" << result.invalid << " invalid, not listed)";
            for (const auto& entry : result.entries)
            {
                out << '\n' << value_text(entry.move) << " => " << value_text(entry.cost);
            }
            const auto valid = result.moves - result.invalid;
            if (result.entries.size() < valid)
            {
                out << "\n... " << (valid - result.entries.size()) << " more";
            }
            show_diagnostic("Neighborhood list", out.str());
            set_status(status_kind::success, "Neighborhood listed");
        });
    }

    void neighborhood_statistics()
        requires tester_type::supports_improvement_selection
    {
        if (!require_solution("Neighborhood statistics"))
            return;
        perform("Neighborhood statistics", [this] {
            const auto result = tester_.neighborhood_statistics();
            show_diagnostic(
                "Neighborhood statistics",
                "Moves: " + std::to_string(result.moves) + "\n" +
                    "Improving: " + std::to_string(result.improving) + "\n" +
                    "Sideways: " + std::to_string(result.sideways) + "\n" +
                    "Worsening: " + std::to_string(result.worsening) + "\n" +
                    "Invalid: " + std::to_string(result.invalid));
            set_status(status_kind::success, "Neighborhood statistics computed");
        });
    }

    void check_neighborhood_costs()
        requires tester_type::supports_cost_consistency_check
    {
        if (!require_solution("Check neighborhood costs"))
            return;
        perform("Check neighborhood costs", [this] {
            const auto result = tester_.check_neighborhood_costs();
            show_diagnostic(
                "Neighborhood cost check",
                "Moves: " + std::to_string(result.moves) + "\n" +
                    "Mismatches: " + std::to_string(result.mismatches) + "\n" +
                    "Invalid: " + std::to_string(result.invalid));
            set_status(
                result.mismatches == 0 && result.invalid == 0
                    ? status_kind::success
                    : status_kind::error,
                result.mismatches == 0 && result.invalid == 0
                    ? "Neighborhood costs consistent"
                    : "Neighborhood cost check found errors");
        });
    }

    void check_move_independence()
        requires tester_type::supports_move_independence_check
    {
        if (!require_solution("Check move independence"))
            return;
        perform("Check move independence", [this] {
            const auto result = tester_.check_move_independence();
            show_diagnostic(
                "Move independence",
                "Moves: " + std::to_string(result.moves) + "\n" +
                    "Null moves: " + std::to_string(result.null_moves) + "\n" +
                    "Repeated states: " + std::to_string(result.repeated_states) + "\n" +
                    "Invalid: " + std::to_string(result.invalid));
            set_status(
                result.null_moves == 0 && result.repeated_states == 0 && result.invalid == 0
                    ? status_kind::success
                    : status_kind::warning,
                "Move independence check completed");
        });
    }

    void check_random_distribution()
        requires tester_type::supports_random_distribution_check
    {
        if (!require_solution("Check random distribution"))
            return;
        perform("Check random distribution", [this] {
            const auto result = tester_.check_random_move_distribution(
                tester_.rng(),
                options_.random_distribution_rounds);
            show_diagnostic(
                "Random move distribution",
                "Neighborhood size: " + std::to_string(result.neighborhood_size) + "\n" +
                    "Samples: " + std::to_string(result.samples) + "\n" +
                    "Frequency range: " + std::to_string(result.min_frequency) +
                    ".." + std::to_string(result.max_frequency) + "\n" +
                    "Unseen moves: " + std::to_string(result.unseen) + "\n" +
                    "Outside neighborhood: " +
                    std::to_string(result.out_of_neighborhood));
            set_status(
                result.out_of_neighborhood == 0 && result.unseen == 0
                    ? status_kind::success
                    : status_kind::warning,
                "Random move distribution sampled");
        });
    }

    void apply_move()
    {
        if (!require_move("Apply move"))
        {
            return;
        }
        if (!tester_.move_is_valid())
        {
            set_status(status_kind::warning, "Apply move: selected move is invalid");
            return;
        }
        perform("Apply move", [this] {
            const auto before = tester_.evaluate();
            tester_.apply_move();
            // A faulty make_move may break the solution.
            if (!tester_.is_valid())
            {
                refresh_page_labels();
                last_move_result_ =
                    "Applied: " + value_text(before) + " -> INVALID solution";
                set_status(
                    status_kind::error,
                    "Move applied: the solution is INVALID; Move and Run disabled");
                return;
            }
            const auto after = tester_.evaluate();
            last_move_result_ =
                "Applied: " + value_text(before) + " -> " + value_text(after);
            set_status(status_kind::success, "Move applied");
        });
    }

    // Run asks for the selected runner's parameters first, when it has any;
    // the run starts when the window is confirmed.
    void run_runner()
    {
        if (!require_solution("Run runner"))
        {
            return;
        }
        if (runner_names_.empty())
        {
            set_status(status_kind::warning, "Run runner: no runner registered");
            return;
        }
        if (run_future_.valid())
        {
            set_status(status_kind::warning, "A runner is already executing");
            return;
        }

        const auto name = runner_names_.at(static_cast<std::size_t>(runner_selected_));
        std::vector<detail::parameter_field> fields;
        if constexpr (supports_parameters)
            fields = detail::parameter_fields(
                tester_.configuration(),
                "runners." + name + ".");
        if (fields.empty())
        {
            start_runner();
            return;
        }

        open_parameters(
            "Parameters of " + name,
            "Run",
            "runners." + name + ".",
            std::move(fields),
            [this](const std::span<const easylocal::config::text_override> changes) {
                return tester_.configure(changes);
            },
            [this] { start_runner(); });
    }

    void start_runner()
    {
        if (!require_solution("Run runner"))
            return;
        if (runner_names_.empty())
        {
            set_status(status_kind::warning, "Run runner: no runner registered");
            return;
        }
        if (run_future_.valid())
        {
            set_status(status_kind::warning, "A runner is already executing");
            return;
        }

        try
        {
            const auto& selected_name = runner_names_.at(
                static_cast<std::size_t>(runner_selected_));
            std::optional<typename tester_type::cost_type> target;
            if constexpr (supports_target)
            {
                if (target_text_.find_first_not_of(" \t") != std::string::npos)
                {
                    try
                    {
                        target.emplace(tester_.read_cost(target_text_));
                    }
                    // A problem's read_cost may throw any exception.
                    catch (const std::exception& error)

                    {
                        set_status(
                            status_kind::error,
                            "Target cost: " + std::string{error.what()});
                        return;
                    }
                }
            }
            std::optional<double> seconds;
            if (timeout_text_.find_first_not_of(" \t") != std::string::npos)
            {
                seconds = detail::seconds_text(timeout_text_);
                if (!seconds)
                {
                    set_status(
                        status_kind::error,
                        "Time limit: give a non-negative number of seconds, or nothing");
                    return;
                }
            }
            std::optional<std::size_t> evaluations;
            if (evaluations_text_.find_first_not_of(" \t") != std::string::npos)
            {
                evaluations = detail::count_text(evaluations_text_);
                if (!evaluations)
                {
                    set_status(
                        status_kind::error,
                        "Evaluations: give a non-negative whole number, or nothing");
                    return;
                }
            }
            auto application = tester_.app();
            auto input = tester_.input_handle();
            auto solution = tester_.solution();

            if (!input)
            {
                set_status(status_kind::error, "Run runner: Input is not available");
                return;
            }

            if (run_worker_.joinable())
            {
                run_worker_.join();
            }

            run_name_ = selected_name;
            run_started_ = std::chrono::steady_clock::now();
            run_before_ = value_text(tester_.evaluate());
            progress_ = progress_snapshot{
                .mode = progress_mode::indeterminate,
                .current = 0,
                .total = std::nullopt,
                .label = "Running " + run_name_,
            };
            progress_visible_ = true;
            set_status(status_kind::info, "Runner executing: " + run_name_);

            run_progress_state_ = std::make_shared<easylocal::shared_run_progress>();

            std::promise<async_runner_result<typename tester_type::solution_type>> promise;
            run_future_ = promise.get_future();
            auto* event_app = event_app_;
            const auto name = run_name_;

            const auto progress_state = run_progress_state_;
            // Each background run gets its own generator, seeded from the
            // session's RNG (itself seeded by options.seed), so runs are
            // reproducible and never share state with the UI thread.
            typename tester_type::rng_type run_rng{tester_.rng()()};
            run_worker_ = std::jthread(
                [application = std::move(application),
                    input = std::move(input),
                    solution = std::move(solution),
                    run_rng,
                    name,
                    target,
                    seconds,
                    evaluations,
                    promise = std::move(promise),
                    event_app,
                    progress_state](std::stop_token stop_token) mutable {
                    async_runner_result<typename tester_type::solution_type> completion;
                    std::size_t reports = 0;
                    auto observer = [&](const easylocal::run_progress& progress) {
                        progress_state->store(progress);

                        ++reports;
                        if (event_app != nullptr &&
                            (reports == 1 || reports % 64 == 0 ||
                             progress.evaluations ==
                                 progress.evaluation_limit.value_or(0)))
                        {
                            event_app->PostEvent(ftxui::Event::Custom);
                        }
                    };
                    const easylocal::run_control control{stop_token, observer};

                    try
                    {
                        auto options = easylocal::with(control);
                        if (seconds)
                            options = options.timeout(*seconds);
                        if (evaluations)
                            options = options.max_evaluations(*evaluations);
                        auto result = target
                            ? application.run(
                                  name,
                                  *input,
                                  std::move(solution),
                                  run_rng,
                                  options.stop_at(*target))
                            : application.run(
                                  name,
                                  *input,
                                  std::move(solution),
                                  run_rng,
                                  options);
                        if (result)
                        {
                            completion.solution.emplace(std::move(result->solution));
                            completion.cancelled = stop_token.stop_requested();
                            completion.effort = result->effort;
                        }
                    }
                    catch (const std::exception& error)
                    {
                        completion.error = error.what();
                    }
                    catch (...)
                    {
                        completion.error = "unknown error";
                    }

                    promise.set_value(std::move(completion));
                    if (event_app != nullptr)
                    {
                        event_app->PostEvent(ftxui::Event::Custom);
                    }
                });
        }
        catch (const std::exception& error)
        {
            // No thread will complete the promise: the next run must not wait
            // for it.
            run_future_ = {};
            progress_visible_ = false;
            progress_ = {};
            run_progress_state_.reset();
            set_status(status_kind::error, "Run runner: " + std::string{error.what()});
        }
        catch (...)
        {
            run_future_ = {};
            progress_visible_ = false;
            progress_ = {};
            run_progress_state_.reset();
            set_status(status_kind::error, "Run runner: unknown error");
        }
    }

    void apply_seed()
    {
        std::uint64_t seed{};
        const auto* first = seed_text_.data();
        const auto* last = first + seed_text_.size();
        const auto [end, error] = std::from_chars(first, last, seed);
        if (error != std::errc{} || end != last)
        {
            set_status(
                status_kind::error,
                "Seed must be a non-negative integer: " + seed_text_);
            seed_text_ = std::to_string(seed_);
            return;
        }

        seed_ = seed;
        tester_.set_seed(seed);
        set_status(
            status_kind::info,
            "Seed set to " + std::to_string(seed) +
                "; random solutions, moves and runs restart from it");
    }

    // The problem's parameters: the cost's, the SolutionManager's and the
    // neighborhood's, shared by every runner and by the Move page.
    void open_problem_parameters()
    {
        if constexpr (supports_parameters)
        {
            const auto parameters = tester_.configuration();
            auto fields = detail::parameter_fields(parameters, "cost.", false);
            auto solution_manager =
                detail::parameter_fields(parameters, "solution_manager.", false);
            fields.insert(fields.end(), solution_manager.begin(), solution_manager.end());
            auto neighborhood =
                detail::parameter_fields(parameters, "neighborhood.", false);
            fields.insert(fields.end(), neighborhood.begin(), neighborhood.end());
            if (fields.empty())
            {
                set_status(status_kind::info, "The problem has no parameters");
                return;
            }
            open_parameters(
                "Problem parameters",
                "Apply",
                "",
                std::move(fields),
                [this](const std::span<const easylocal::config::text_override> changes) {
                    return tester_.configure(changes);
                },
                [this] {
                    last_move_result_.clear();
                    set_status(
                        status_kind::success,
                        "Problem parameters applied: costs and moves follow them");
                });
        }
    }

    // Shows the parameters window: one field per parameter, the changes
    // applied by apply when confirmed (all or none), then after.
    void open_parameters(
        std::string title,
        std::string action,
        std::string prefix,
        std::vector<detail::parameter_field> fields,
        std::function<easylocal::config::override_result(
            std::span<const easylocal::config::text_override>)> apply,
        std::function<void()> after)
    {
        parameter_inputs_->DetachAllChildren();
        parameter_title_ = std::move(title);
        parameter_action_ = std::move(action);
        parameter_prefix_ = std::move(prefix);
        parameter_fields_ = std::move(fields);
        parameter_apply_ = std::move(apply);
        parameter_after_ = std::move(after);
        parameter_error_.clear();

        for (auto& field : parameter_fields_)
        {
            const auto original = original_parameters_.find(field.path);
            if (original != original_parameters_.end())
                field.original = original->second;

            auto option = ftxui::InputOption::Default();
            option.multiline = false;
            option.on_enter = [this] { submit_parameters(); };
            option.cursor_position = &field.cursor;
            auto input = ftxui::Input(&field.text, field.current, option);
            parameter_inputs_->Add(ftxui::Renderer(input, [&field, input] {
                using namespace ftxui;
                const auto marker = field.text != field.original ? "  *" : "";
                Elements rows{text(field.label + marker) | bold};
                rows.push_back(hbox({text("  "), input->Render() | flex}));
                if (!field.description.empty())
                    rows.push_back(text("  " + field.description) | dim);
                return vbox(std::move(rows));
            }));
        }
        parameters_visible_ = true;
        if (parameter_inputs_->ChildCount() > 0)
            parameter_inputs_->ChildAt(0)->TakeFocus();
    }

    void submit_parameters()
    {
        const auto changes = detail::changed_parameters(parameter_fields_);
        if (!changes.empty())
        {
            const auto result = parameter_apply_(changes);
            if (!result)
            {
                parameter_error_ = detail::parameter_errors(result, parameter_prefix_);
                return;
            }
        }
        parameters_visible_ = false;
        parameter_error_.clear();
        refresh_changed_parameters();
        if (parameter_after_)
            parameter_after_();
    }

    void remember_original_parameters()
    {
        if constexpr (supports_parameters)
            for (const auto& parameter : tester_.configuration().parameters())
                original_parameters_[parameter.path] = parameter.value;
    }

    // The parameters that differ from their values when the tester started,
    // shown on the Run page.
    void refresh_changed_parameters()
    {
        changed_parameter_lines_.clear();
        if constexpr (supports_parameters)
        {
            for (const auto& parameter : tester_.configuration().parameters())
            {
                const auto original = original_parameters_.find(parameter.path);
                if (original != original_parameters_.end()
                    && original->second != parameter.value)
                {
                    changed_parameter_lines_.push_back(
                        parameter.path + " = " + parameter.value + " (was "
                        + original->second + ")");
                }
            }
        }
    }

    [[nodiscard]] ftxui::Element render_parameters(const ftxui::Component& buttons) const
    {
        using namespace ftxui;
        Elements body{
            paragraph(
                "Enter " + parameter_action_
                + "  |  Tab next field  |  Esc cancel  |  * changed since start")
                | dim,
            separator(),
            parameter_inputs_->Render() | vscroll_indicator | frame | flex,
        };
        if (!parameter_error_.empty())
        {
            body.push_back(separator());
            body.push_back(text_lines(parameter_error_) | color(Color::Red));
        }
        body.push_back(separator());
        body.push_back(buttons->Render() | center);
        return window(text(" " + parameter_title_ + " "), vbox(std::move(body)))
            | size(WIDTH, EQUAL, (std::min)(76, detail::terminal_available_width()))
            | size(HEIGHT, LESS_THAN, detail::terminal_available_height()) | border;
    }

    void stop_runner()
    {
        if (!run_future_.valid() || !run_worker_.joinable())
        {
            set_status(status_kind::warning, "No runner is currently executing");
            return;
        }
        run_worker_.request_stop();
        progress_.label = "Stopping " + run_name_;
        set_status(status_kind::info, "Stop requested: " + run_name_);
    }

    void refresh_runner_progress()
    {
        if (!run_progress_state_)
        {
            return;
        }

        const auto progress = run_progress_state_->load();
        progress_.current = progress.evaluations;
        progress_.total = progress.evaluation_limit;
        progress_.mode = progress.evaluation_limit
            ? progress_mode::determinate
            : progress_mode::indeterminate;
        if (!run_worker_.get_stop_token().stop_requested())
        {
            const std::chrono::duration<double> elapsed =
                std::chrono::steady_clock::now() - run_started_;
            progress_.label = "Running " + run_name_
                + " [eval=" + std::to_string(progress.evaluations)
                + ", iter=" + std::to_string(progress.iterations) + ", "
                + detail::seconds_label(elapsed.count()) + "]";
        }
    }

    void finish_runner_run()
    {
        if (!run_future_.valid())
        {
            return;
        }

        auto completion = run_future_.get();
        if (run_worker_.joinable())
        {
            run_worker_.join();
        }

        progress_visible_ = false;
        progress_ = {};
        run_progress_state_.reset();

        // Every ending replaces the Last run box, a failure included.
        if (!completion.error.empty())
        {
            last_run_result_ = run_name_ + ": failed: " + completion.error;
            set_status(
                status_kind::error,
                "Runner failed: " + run_name_ + ": " + completion.error);
            return;
        }
        if (!completion.solution)
        {
            last_run_result_ = run_name_ + ": runner not found";
            set_status(
                status_kind::error,
                "Run runner: runner not found: " + run_name_);
            return;
        }

        tester_.set_solution(std::move(*completion.solution));
        refresh_page_labels();
        if (!tester_.is_valid())
        {
            last_run_result_ = run_name_ + ": " + run_before_ + " -> INVALID solution"
                + detail::run_ending(completion.effort, completion.cancelled);
            set_status(
                status_kind::error,
                "Runner completed: " + run_name_ +
                    " produced an INVALID solution; Move and Run disabled");
            return;
        }
        if (completion.cancelled)
        {
            const auto after = tester_.evaluate();
            last_run_result_ = run_name_ + ": " + run_before_ + " -> " + value_text(after)
                + detail::run_ending(completion.effort, true);
            set_status(status_kind::warning, "Runner stopped: " + last_run_result_);
            return;
        }

        const auto after = tester_.evaluate();
        last_run_result_ = run_name_ + ": " + run_before_ + " -> " + value_text(after)
            + detail::run_ending(completion.effort, false);
        set_status(status_kind::success, "Runner completed: " + last_run_result_);
    }

    [[nodiscard]] std::string solution_status(std::string prefix) const
    {
        if (!tester_.has_solution())
        {
            return prefix;
        }
        if (!tester_.is_valid())
        {
            return prefix + ": INVALID; Move and Run disabled";
        }
        return prefix;
    }

    [[nodiscard]] std::string move_status(std::string prefix) const
    {
        if (tester_.has_move() && !tester_.move_is_valid())
        {
            return prefix + ": INVALID";
        }
        return prefix;
    }

    [[nodiscard]] std::string solution_text() const
    {
        if (!tester_.has_solution())
        {
            return "<no solution selected>";
        }

        auto rendered = value_text(tester_.solution());
        if constexpr (tester_type::supports_solution_saving)
        {
            if (rendered.starts_with("<not printable"))
            {
                std::ostringstream out;
                tester_.save_solution(out);
                rendered = out.str();
            }
        }
        return truncate_text(std::move(rendered), options_.max_render_chars);
    }

    [[nodiscard]] std::string input_text() const
    {
        if (!tester_.has_input())
        {
            return "<no input loaded>";
        }
        return truncate_text(
            value_text(tester_.input()),
            options_.max_render_chars);
    }

    [[nodiscard]] std::string current_cost_text() const
    {
        if (!tester_.has_solution())
        {
            return "-";
        }
        if (!tester_.is_valid())
        {
            return "<invalid solution>";
        }
        return value_text(tester_.evaluate());
    }

    [[nodiscard]] std::string move_text() const
    {
        if (!tester_.has_move())
        {
            return "<no move selected>";
        }
        return truncate_text(value_text(tester_.move()), options_.max_render_chars);
    }

    [[nodiscard]] ftxui::Element render_solution_summary() const
    {
        using namespace ftxui;
        Elements lines;
        lines.push_back(text(
            std::string{"Instance: "} +
            (tester_.has_input() ? "loaded" : "not loaded")));
        if (tester_.has_input() && !loaded_input_path_.empty())
        {
            lines.push_back(
                paragraph("Instance file: " + format_path(loaded_input_path_)));
        }
        lines.push_back(text(
            std::string{"Solution: "} +
            (tester_.has_solution() ? "available" : "not selected")));
        if (tester_.has_solution())
        {
            const bool valid = tester_.is_valid();
            lines.push_back(text(std::string{"Valid: "} + (valid ? "yes" : "NO")));
            if (valid)
            {
                lines.push_back(text("Cost: " + value_text(tester_.evaluate())));
            }
        }
        return vbox(std::move(lines));
    }

    [[nodiscard]] ftxui::Element render_progress(const progress_snapshot& progress) const
    {
        using namespace ftxui;

        switch (progress.mode)
        {
        case progress_mode::determinate:
            if (progress.total.has_value() && *progress.total != 0)
            {
                const auto count = std::to_string(progress.current) + "/" +
                                   std::to_string(*progress.total);
                return hbox({
                    text(progress.label.empty() ? "Progress " : progress.label + " "),
                    gauge(progress_ratio(progress)) | flex,
                    text(" " + count),
                });
            }
            [[fallthrough]];
        case progress_mode::indeterminate:
            return text(
                progress.label.empty() ? "working" : progress.label);
        case progress_mode::unavailable:
        default:
            return text(
                       progress.label.empty() ? "not reported" : progress.label) |
                   dim;
        }
    }

    [[nodiscard]] ftxui::Element render_move_summary() const
    {
        using namespace ftxui;
        Elements lines;
        if (!tester_.has_move())
        {
            lines.push_back(text("No move selected") | dim);
            if (!last_move_result_.empty())
            {
                lines.push_back(separator());
                lines.push_back(text(last_move_result_) | bold);
            }
            return vbox(std::move(lines));
        }

        const bool valid = tester_.move_is_valid();
        lines.push_back(text(std::string{"Valid: "} + (valid ? "yes" : "NO")));
        lines.push_back(separator());
        lines.push_back(text_lines(move_text()));
        if (valid)
        {
            lines.push_back(separator());
            lines.push_back(text("Candidate (incremental): " + value_text(tester_.evaluate_move())));
            lines.push_back(text("Candidate (full):        " + value_text(tester_.evaluate_move_fully())));
            if constexpr (requires(const tester_type& tester) {
                              { tester.move_evaluation_matches_full() }
                                  -> std::same_as<bool>;
                          })
            {
                lines.push_back(text(
                    std::string{"Delta check: "} +
                    (tester_.move_evaluation_matches_full() ? "OK" : "FAILED")));
            }
        }
        if (!last_move_result_.empty())
        {
            lines.push_back(separator());
            lines.push_back(text(last_move_result_) | bold);
        }
        return vbox(std::move(lines));
    }

    // Below this width the Move page puts its actions above the move.
    static constexpr int narrow_move_page = 64;

    // The controls of a page in the lines left to them: they scroll to keep
    // the focused one in view.
    [[nodiscard]] static ftxui::Element scrolling(ftxui::Element controls)
    {
        using namespace ftxui;
        return std::move(controls) | vscroll_indicator | yframe | yflex;
    }

    [[nodiscard]] std::string_view status_prefix() const
    {
        switch (status_kind_)
        {
        case status_kind::success:
            return "OK";
        case status_kind::warning:
            return "WARN";
        case status_kind::error:
            return "ERROR";
        case status_kind::info:
        default:
            return "INFO";
        }
    }

    [[nodiscard]] ftxui::Element render_status() const
    {
        using namespace ftxui;
        return hbox({
            text(std::string{status_prefix()} + " ") | bold,
            text_lines(status_) | flex,
        });
    }

    [[nodiscard]] ftxui::Element render_solution_page(
        const ftxui::Component& controls) const
    {
        using namespace ftxui;

        Elements summary;
        if (role_ == frontend_role::launcher_child)
            summary.push_back(
                text("Input and solution shared with the launcher's applications") | dim);
        else if (role_ == frontend_role::launcher_root)
            summary.push_back(
                text("Input and solution for all the applications of the launcher")
                | dim);
        switch (detail::solution_stage_of(tester_))
        {
        case solution_stage::needs_input:
            summary.push_back(text("Load an input to begin") | bold);
            break;
        case solution_stage::needs_solution:
            summary.push_back(text("Input ready - choose or load a solution") | bold);
            break;
        case solution_stage::invalid_solution:
            summary.push_back(text("Solution INVALID - replace it or run diagnostics") | bold);
            break;
        case solution_stage::ready:
            summary.push_back(
                text(
                    role_ == frontend_role::launcher_root
                        ? "Setup complete - open an application for Move and Run"
                        : "Setup complete - use F4 Move or F5 Run")
                | dim);
            break;
        }

        summary.push_back(separator());
        summary.push_back(scrolling(controls->Render()));
        return window(text(" Input / Output "), vbox(std::move(summary))) | flex;
    }

    [[nodiscard]] ftxui::Element render_move_page(
        const ftxui::Component& controls,
        const ftxui::Component& diagnostics) const
    {
        using namespace ftxui;
        const auto neighborhood = detail::object_name(tester_.bound_app().neighborhood());
        auto details = window(
                           text(" Move - " + neighborhood + " "),
                           render_move_summary()) |
                       flex;
        auto diagnostic_actions = window(
                                      text(" Diagnostics "),
                                      diagnostics->Render()) |
                                  size(HEIGHT, EQUAL, 3);
        // Narrow terminals put the actions in rows above the move, wrapped to
        // the width.
        if (ftxui::Terminal::Size().dimx < narrow_move_page)
        {
            Elements buttons;
            for (std::size_t i = 0; i < controls->ChildCount(); ++i)
                buttons.push_back(controls->ChildAt(i)->Render());
            auto rows = flexbox(std::move(buttons), FlexboxConfig{}.SetGap(2, 0));
            return vbox({
                       window(text(" Actions "), scrolling(std::move(rows)))
                           | size(HEIGHT, LESS_THAN, 5),
                       details,
                       diagnostic_actions,
                   })
                | flex;
        }
        auto actions = window(text(" Actions "), scrolling(controls->Render()))
            | size(WIDTH, LESS_THAN, 30);
        return vbox({
            hbox({actions, details}) | flex,
            diagnostic_actions,
        }) | flex;
    }

    [[nodiscard]] ftxui::Element render_run_page(const ftxui::Component& controls) const
    {
        using namespace ftxui;
        // The controls scroll; the changed parameters (their first lines) and
        // the last run stay below them.
        Elements body{scrolling(controls->Render())};
        if (!changed_parameter_lines_.empty())
        {
            Elements lines;
            for (const auto& line : changed_parameter_lines_)
                lines.push_back(paragraph(line));
            body.push_back(separator());
            body.push_back(text("Changed parameters") | bold);
            body.push_back(
                vbox(std::move(lines)) | vscroll_indicator | yframe
                | size(HEIGHT, LESS_THAN, 3));
        }
        if (!last_run_result_.empty())
        {
            body.push_back(separator());
            body.push_back(text("Last run") | bold);
            body.push_back(paragraph(last_run_result_));
        }
        return window(text(" Runners "), vbox(std::move(body))) | flex;
    }

    [[nodiscard]] ftxui::Element render_main(
        const ftxui::Component& page_menu,
        const ftxui::Component& pages) const
    {
        using namespace ftxui;

        const auto title = options_.title + "  |  " + current_instance_name() +
                           "  [seed=" + std::to_string(seed_) + "]";
        const auto cost = "COST " + current_cost_text();
        const auto header = ftxui::Terminal::Size().dimx < 80
            ? vbox({paragraph(title) | bold, text(cost) | bold})
            : hbox({text(title) | bold, filler(), text(cost) | bold});

        return vbox({
                   header,
                   page_menu->Render() | center,
                   separator(),
                   pages->Render() | flex,
                   separator(),
                   render_status(),
                   paragraphAlignCenter(current_page_shortcuts()),
                   paragraphAlignCenter(
                       "F3 I/O  F4 Move  F5 Run  |  F1 Input  F2 Solution  S Show solution  |  ? Help  |  q " +
                       options_.exit_label +
                       "  |  Tab/Shift-Tab focus") | dim,
               }) |
               size(HEIGHT, EQUAL, detail::terminal_available_height()) |
               border;
    }

    [[nodiscard]] std::string current_instance_name() const
    {
        const auto basename =
            tester_.has_input() ? loaded_input_path_.filename().string() : std::string{};
        if (!basename.empty())
        {
            return basename;
        }
        return tester_.has_input() ? "<memory>" : "<no instance>";
    }

    void show_diagnostic(std::string title, std::string body)
    {
        diagnostic_title_ = std::move(title);
        diagnostic_lines_ = body.empty()
            ? std::vector<std::string>{"<no output>"}
            : split_text_lines(body);
        diagnostic_selected_ = 0;
        diagnostic_visible_ = true;
    }

    [[nodiscard]] ftxui::Element render_diagnostic_viewer(
        const ftxui::Component& menu,
        const ftxui::Component& close) const
    {
        using namespace ftxui;
        const auto width = (std::min)(76, detail::terminal_available_width());
        const auto height = (std::min)(20, detail::terminal_available_height());
        return window(
                   text(" " + diagnostic_title_ + " "),
                   vbox({
                       paragraph("Up/Down/PgUp/PgDn scroll  |  Esc/q close") | dim,
                       separator(),
                       menu->Render() | vscroll_indicator | frame | flex,
                       separator(),
                       close->Render() | center,
                   })) |
               size(WIDTH, EQUAL, width) |
               size(HEIGHT, EQUAL, height) | border;
    }

    [[nodiscard]] ftxui::Element render_progress_modal(const ftxui::Component& stop) const
    {
        using namespace ftxui;
        const auto width = (std::min)(60, detail::terminal_available_width());
        return window(
                   text(" Progress "),
                   vbox({
                       render_progress(progress_),
                       progress_.mode == progress_mode::indeterminate
                           ? text("Working...") | dim
                           : text(""),
                       separator(),
                       stop->Render() | center,
                   })) |
               size(WIDTH, EQUAL, width) | border;
    }

    [[nodiscard]] ftxui::Element render_input_viewer(
        const ftxui::Component& menu,
        const ftxui::Component& close) const
    {
        using namespace ftxui;
        Elements body;
        if (tester_.has_input() && !loaded_input_path_.empty())
        {
            body.push_back(paragraph("File: " + format_path(loaded_input_path_)) | dim);
            body.push_back(separator());
        }
        body.push_back(paragraph("Up/Down/PgUp/PgDn scroll  |  Esc/F1 close") | dim);
        body.push_back(separator());
        body.push_back(menu->Render() | vscroll_indicator | frame | flex);
        body.push_back(separator());
        body.push_back(close->Render() | center);
        return window(text(" Input  [F1] "), vbox(std::move(body))) |
               size(WIDTH, EQUAL, detail::terminal_available_width()) |
               size(HEIGHT, EQUAL, detail::terminal_available_height()) |
               border;
    }

    [[nodiscard]] ftxui::Element render_solution_viewer(
        const ftxui::Component& menu,
        const ftxui::Component& close) const
    {
        using namespace ftxui;
        Elements body;
        if (tester_.has_solution())
        {
            body.push_back(text(
                std::string{"Valid: "} + (tester_.is_valid() ? "yes" : "NO")) |
                           bold);
            body.push_back(text("Cost: " + current_cost_text()) | bold);
            if constexpr (requires { tester_.cost_report(); })
            {
                // Each cost component's value, and its describe(solution)
                // text when it has one.
                if (tester_.is_valid())
                {
                    for (const auto& component : tester_.cost_report())
                    {
                        body.push_back(
                            text("  " + component.name + ": " + component.value));
                        if (!component.description.empty())
                        {
                            // The description under its component, indented.
                            for (const auto& line :
                                detail::split_text_lines(component.description))
                                body.push_back(text("    " + line) | dim);
                        }
                    }
                }
            }
            body.push_back(separator());
        }
        body.push_back(paragraph("Up/Down/PgUp/PgDn scroll  |  Esc/F2/S close") | dim);
        body.push_back(separator());
        body.push_back(menu->Render() | vscroll_indicator | frame | flex);
        body.push_back(separator());
        body.push_back(close->Render() | center);
        return window(text(" Solution  [F2/S] "), vbox(std::move(body))) |
               size(WIDTH, EQUAL, detail::terminal_available_width()) |
               size(HEIGHT, EQUAL, detail::terminal_available_height()) |
               border;
    }

    void open_browser(const file_target target)
    {
        browser_target_ = target;
        const std::string& current =
            target == file_target::input ? input_path_ : solution_path_;

        std::error_code error;
        std::filesystem::path start;
        if (!current.empty())
        {
            auto candidate = resolve_path(current);
            if (std::filesystem::is_directory(candidate, error) && !error)
            {
                start = candidate;
            }
            else if (candidate.has_parent_path())
            {
                start = candidate.parent_path();
            }
        }
        if (start.empty())
        {
            if (!options_.path_base.empty())
            {
                start = absolute_path_from({}, options_.path_base);
            }
            else
            {
                start = std::filesystem::current_path(error);
                if (error)
                {
                    set_status(status_kind::error, "Browse: cannot determine current directory");
                    return;
                }
            }
        }

        browser_directory_ = std::filesystem::absolute(start, error);
        if (error)
        {
            browser_directory_ = start;
        }
        refresh_browser();
        browser_visible_ = true;
    }

    void refresh_browser()
    {
        try
        {
            browser_entries_ = directory_entries(browser_directory_);
            browser_labels_.clear();
            browser_labels_.reserve(browser_entries_.size());
            for (const auto& entry : browser_entries_)
            {
                auto label = entry.path.filename().string();
                browser_labels_.push_back(
                    entry.directory ? "[dir] " + label + "/" : "      " + label);
            }
            browser_selected_ = 0;
            browser_error_.clear();
        }
        catch (const std::exception& error)
        {
            browser_entries_.clear();
            browser_labels_.clear();
            browser_selected_ = 0;
            browser_error_ = error.what();
        }
    }

    void browser_up()
    {
        const auto parent = browser_directory_.parent_path();
        if (!parent.empty() && parent != browser_directory_)
        {
            browser_directory_ = parent;
            refresh_browser();
        }
    }

    void accept_browser_selection()
    {
        if (browser_entries_.empty() || browser_selected_ < 0 ||
            static_cast<std::size_t>(browser_selected_) >= browser_entries_.size())
        {
            browser_error_ = "no file selected";
            return;
        }

        const auto& selected = browser_entries_.at(
            static_cast<std::size_t>(browser_selected_));
        if (selected.directory)
        {
            browser_directory_ = selected.path;
            refresh_browser();
            return;
        }

        auto selected_text = editable_path(
            selected.path,
            options_.path_display,
            options_.path_base);
        if (browser_target_ == file_target::input)
        {
            input_path_ = std::move(selected_text);
            add_known_path(file_target::input, input_path_);
        }
        else
        {
            solution_path_ = std::move(selected_text);
            add_known_path(file_target::solution, solution_path_);
        }
        browser_visible_ = false;
        set_status(status_kind::info, "Selected file: " + format_path(selected.path));
    }

    [[nodiscard]] ftxui::Element render_browser(
        const ftxui::Component& browser_menu,
        const ftxui::Component& browser_buttons) const
    {
        using namespace ftxui;
        const auto title = browser_target_ == file_target::input
                               ? " Select input file "
                               : " Select solution file ";
        Elements body{
            text(format_path(browser_directory_)) | bold,
            separator(),
            browser_menu->Render() | vscroll_indicator | frame | flex,
        };
        if (!browser_error_.empty())
        {
            body.push_back(separator());
            body.push_back(text("ERROR: " + browser_error_) | bold);
        }
        body.push_back(separator());
        body.push_back(browser_buttons->Render());
        body.push_back(
            text(
                "Enter/Open: open file or directory  |  Backspace/Up: parent directory  |  "
                "Esc: cancel")
            | dim);
        return window(text(title), vbox(std::move(body))) |
               size(WIDTH, EQUAL, detail::terminal_available_width()) |
               size(HEIGHT, EQUAL, detail::terminal_available_height()) |
               border;
    }

    [[nodiscard]] ftxui::Element render_help(const ftxui::Component& controls) const
    {
        using namespace ftxui;
        Elements lines{
            text("Pages") | bold,
            separator(),
            text("F3  Input / Output page"),
            text("F4  Move page"),
            text("F5  Run page"),
            text("F1  show Input"),
            text("F2  show Solution"),
            text("S   show Solution"),
            separator(),
            text("Input / Output page") | bold,
            text(
                "L load input   I initial   R random   Shift-L load solution   W save   C check"),
            text("Move page") | bold,
            text("B best   I first improving   F first   N next   R random   A apply"),
            text("P list   T stats   C costs   D independence   U distribution"),
            text("Run page") | bold,
            text("G run selected   Enter run selected   X stop running"),
            text("Running asks for the runner's parameters first; P problem parameters"),
            separator(),
            text("Q  " + options_.exit_label),
            text("? / H  help   Esc  close"),
            separator(),
            text("Page shortcuts apply only to the active page.") | dim,
            text("Move and Run unlock after a valid solution is available.") | dim,
            text("Paths are shown relative to the configured base by default.") | dim,
            text("Shortcuts are disabled while a text field is focused.") | dim,
            separator(),
            controls->Render(),
        };
        return window(text(" Keyboard help "), vbox(std::move(lines))) |
               size(WIDTH, EQUAL, detail::terminal_available_width()) |
               size(HEIGHT, EQUAL, detail::terminal_available_height()) |
               border;
    }

    tester_type& tester_;
    tui::options options_;
    frontend_role role_{frontend_role::standalone};
    std::uint64_t seed_{};
    std::string seed_text_;
    std::string target_text_;
    std::string timeout_text_;
    std::string evaluations_text_;
    std::chrono::steady_clock::time_point run_started_{};

    std::string input_path_;
    // The file of the Input loaded, which input_path_ may no longer name.
    std::filesystem::path loaded_input_path_;
    std::string solution_path_;
    std::vector<std::string> known_input_paths_;
    std::vector<std::string> known_input_labels_;
    int known_input_selected_{};
    std::vector<std::string> known_solution_paths_;
    std::vector<std::string> known_solution_labels_;
    int known_solution_selected_{};

    status_kind status_kind_{status_kind::info};
    std::string status_{"Ready"};

    std::vector<std::string> runner_names_;
    int runner_selected_{};
    std::string last_move_result_;
    std::string last_run_result_;

    std::vector<std::string> page_labels_{"Input/Output", "Move", "Run"};
    int page_selected_{};

    bool browser_visible_{};
    file_target browser_target_{file_target::input};
    std::filesystem::path browser_directory_;
    std::vector<file_entry> browser_entries_;
    std::vector<std::string> browser_labels_;
    int browser_selected_{};
    std::string browser_error_;

    bool parameters_visible_{};
    ftxui::Component parameter_inputs_;
    std::string parameter_title_;
    std::string parameter_action_{"Apply"};
    std::string parameter_prefix_;
    std::vector<detail::parameter_field> parameter_fields_;
    std::function<easylocal::config::override_result(
        std::span<const easylocal::config::text_override>)>
        parameter_apply_;
    std::function<void()> parameter_after_;
    std::string parameter_error_;
    std::map<std::string, std::string> original_parameters_;
    std::vector<std::string> changed_parameter_lines_;

    bool help_visible_{};
    bool diagnostic_visible_{};
    std::string diagnostic_title_;
    std::vector<std::string> diagnostic_lines_;
    int diagnostic_selected_{};
    bool progress_visible_{};
    progress_snapshot progress_{};
    ftxui::App* event_app_{};
    // The text fields of the Run page: while one is focused, letters are typed
    // rather than shortcuts.
    ftxui::Component seed_input_;
    ftxui::Component target_input_;
    ftxui::Component timeout_input_;
    ftxui::Component evaluations_input_;
    std::jthread run_worker_{};
    std::future<async_runner_result<typename tester_type::solution_type>> run_future_{};
    std::shared_ptr<easylocal::shared_run_progress> run_progress_state_;
    std::string run_name_;
    std::string run_before_;
    bool input_visible_{};
    detail::text_viewer input_viewer_;
    bool solution_visible_{};
    detail::text_viewer solution_viewer_;
};

} // namespace detail

/// Runs the interactive tester on an app: an interactive session on it, with
/// options.seed for its RNG, and the Input loaded from options.input_path when
/// it is set (through the read_input hook).
template<class App>
void run(App application, tui::options settings = {})
{
    using session_type = easylocal::Session<App>;
    session_type session{std::move(application), settings.seed};
    if constexpr (session_type::supports_input_loading)
    {
        if (!settings.input_path.empty())
            session.load_input(detail::initial_input_file(settings));
    }
    detail::tester_frontend<App>{session, std::move(settings)}.run();
}

} // namespace easylocal::tui
