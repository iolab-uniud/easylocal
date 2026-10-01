#pragma once

#include <easylocal/runners/run_control.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/app/check.hpp>
#include <easylocal/app/tester.hpp>

#include <ftxui/ftxui.hpp>
#include <ftxui/screen/string.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::tui
{

enum class path_display_mode
{
    relative,
    absolute,
    both,
};

struct tester_options
{
    std::string title{"EasyLocal++ Tester"};
    std::uint64_t seed{};
    std::string input_path;
    std::string solution_path;
    path_display_mode path_display{path_display_mode::relative};
    std::filesystem::path path_base;
    std::size_t max_render_chars{4096};
    std::size_t random_distribution_rounds{20};
    std::size_t max_diagnostic_entries{256};
    std::string exit_label{"quit"};
};

namespace detail
{

namespace display_adl
{

void describe() = delete;

template<class T>
concept has_describe =
    requires(const T& value) {
        { describe(value) } -> std::convertible_to<std::string>;
    };

template<class T>
    requires has_describe<T>
[[nodiscard]] auto call_describe(const T& value) -> std::string
{
    return describe(value);
}

} // namespace display_adl

template<class T>
concept member_describable =
    requires(const T& value) {
        { value.describe() } -> std::convertible_to<std::string>;
    };

template<class T>
concept ostream_insertable =
    requires(std::ostream& out, const T& value) {
        out << value;
    };

template<class T>
concept tuple_like =
    requires { typename std::tuple_size<std::remove_cvref_t<T>>::type; };

template<class T>
concept named_object =
    requires(const T& value) {
        { value.name() } -> std::convertible_to<std::string_view>;
    };

template<class T>
[[nodiscard]] auto object_name(const T& value) -> std::string
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
    std::optional<std::size_t> total;
    std::string label;
};

[[nodiscard]] inline auto progress_ratio(const progress_snapshot& progress) noexcept
    -> float
{
    if (progress.mode != progress_mode::determinate ||
        !progress.total.has_value() || *progress.total == 0)
    {
        return 0.0F;
    }
    const auto bounded = std::min(progress.current, *progress.total);
    return static_cast<float>(bounded) / static_cast<float>(*progress.total);
}

[[nodiscard]] inline auto path_basename(std::string_view path) -> std::string
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

[[nodiscard]] constexpr auto page_index(const tester_page page) noexcept -> int
{
    return static_cast<int>(page);
}

template<class Tester>
[[nodiscard]] auto solution_stage_of(const Tester& tester) -> solution_stage
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
[[nodiscard]] auto context_pages_available(const Tester& tester) -> bool
{
    return solution_stage_of(tester) == solution_stage::ready;
}

template<class Tester>
[[nodiscard]] auto page_available(
    const Tester& tester,
    const tester_page page) -> bool
{
    return page == tester_page::solution || context_pages_available(tester);
}

template<class Tester>
[[nodiscard]] auto page_after_solution_change(const Tester& tester)
    -> tester_page
{
    return context_pages_available(tester)
               ? tester_page::move
               : tester_page::solution;
}

template<class T>
[[nodiscard]] auto value_text(const T& value) -> std::string
{
    if constexpr (member_describable<T>)
    {
        return value.describe();
    }
    else if constexpr (display_adl::has_describe<T>)
    {
        return display_adl::call_describe(value);
    }
    else if constexpr (easylocal::cost::hierarchical_type<T>)
    {
        return "hard=" + value_text(value.hard()) +
               ", soft=" + value_text(value.soft());
    }
    else if constexpr (easylocal::cost::lexicographic_type<T>)
    {
        std::string result{"["};
        [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            std::size_t emitted = 0;
            ((result += (emitted++ == 0 ? "" : ", ") +
                        value_text(value.template get<Index>())),
             ...);
        }(std::make_index_sequence<
            easylocal::cost::lexicographic_traits<
                std::remove_cvref_t<T>>::size>{});
        result += ']';
        return result;
    }
    else if constexpr (ostream_insertable<T>)
    {
        std::ostringstream out;
        out << value;
        return out.str();
    }
    else if constexpr (requires { value.hard(); value.soft(); })
    {
        return "hard=" + value_text(value.hard()) +
               ", soft=" + value_text(value.soft());
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

[[nodiscard]] inline auto truncate_text(
    std::string value,
    const std::size_t limit) -> std::string
{
    if (limit == 0 || value.size() <= limit)
    {
        return value;
    }
    value.resize(limit);
    value += "\n... <truncated>";
    return value;
}

inline auto text_lines(const std::string& value) -> ftxui::Element
{
    ftxui::Elements lines;
    std::istringstream input{value};
    std::string line;
    while (std::getline(input, line))
    {
        lines.push_back(ftxui::paragraph(line));
    }
    if (lines.empty())
    {
        lines.push_back(ftxui::text(""));
    }
    return ftxui::vbox(std::move(lines));
}

[[nodiscard]] inline auto split_text_lines(std::string_view value)
    -> std::vector<std::string>
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

[[nodiscard]] inline auto wrap_text_lines(
    std::string_view value,
    const std::size_t width) -> std::vector<std::string>
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

[[nodiscard]] inline auto terminal_available_width(const int margin = 4) noexcept
    -> int
{
    const auto dimensions = ftxui::Terminal::Size();
    return std::max(1, dimensions.dimx - margin);
}

[[nodiscard]] inline auto terminal_available_height(const int margin = 2) noexcept
    -> int
{
    const auto dimensions = ftxui::Terminal::Size();
    return std::max(1, dimensions.dimy - margin);
}

[[nodiscard]] constexpr auto page_scroll_selection(
    const int selected,
    const std::size_t count,
    const int delta) noexcept -> int
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

[[nodiscard]] inline auto absolute_path_from(
    const std::filesystem::path& path,
    const std::filesystem::path& base = {}) -> std::filesystem::path
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

[[nodiscard]] inline auto relative_path_from(
    const std::filesystem::path& path,
    const std::filesystem::path& base = {}) -> std::filesystem::path
{
    const auto absolute = absolute_path_from(path, base);
    const auto absolute_base = absolute_path_from({}, base);
    const auto relative = absolute.lexically_relative(absolute_base);
    return relative.empty() ? absolute : relative;
}

[[nodiscard]] inline auto display_path(
    const std::filesystem::path& path,
    const path_display_mode mode,
    const std::filesystem::path& base = {}) -> std::string
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

[[nodiscard]] inline auto editable_path(
    const std::filesystem::path& path,
    const path_display_mode mode,
    const std::filesystem::path& base = {}) -> std::string
{
    if (mode == path_display_mode::absolute)
    {
        return absolute_path_from(path, base).string();
    }
    return relative_path_from(path, base).string();
}

[[nodiscard]] inline auto directory_entries(const std::filesystem::path& directory)
    -> std::vector<file_entry>
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
    bool found{};
    bool cancelled{};
    std::optional<Solution> solution;
    std::string error;
};

struct async_progress_state
{
    std::atomic<std::size_t> evaluations{};
    std::atomic<std::size_t> iterations{};
    std::atomic<std::size_t> evaluation_limit{};
    std::atomic_bool has_evaluation_limit{};
};

template<class App>
class tester_frontend
{
public:
    using tester_type = easylocal::Tester<App>;
    static constexpr bool supports_neighborhood_diagnostics =
        std::equality_comparable<typename tester_type::solution_type>;

    tester_frontend(tester_type& tester, tester_options options)
        : tester_{tester},
          options_{std::move(options)},
          rng_{options_.seed}
    {
        if (!options_.input_path.empty())
        {
            input_path_ = editable_path(
                options_.input_path,
                options_.path_display,
                options_.path_base);
            add_known_path(file_target::input, input_path_);
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
        page_selected_ = page_index(detail::page_after_solution_change(tester_));
    }

    void run()
    {
        using namespace ftxui;

        auto app = ftxui::App::Fullscreen();

        Component input_path_component;
        Component solution_path_component;

        const auto section_label = [](std::string label) {
            return Renderer([label = std::move(label)] {
                return text(label) | bold;
            });
        };

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
        if constexpr (supports_neighborhood_diagnostics)
        {
            if constexpr (tester_type::supports_deterministic_moves)
            {
                move_diagnostics->Add(Button(
                    "P List",
                    [this] { preview_neighbors(); },
                    ButtonOption::Ascii()));
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
        }
        else
        {
            move_diagnostics->Add(Renderer([] {
                return text("Unavailable: Solution has no operator==") | dim;
            }));
        }

        auto run_controls = Container::Vertical({});
        Component runner_menu;
        if (!runner_names_.empty())
        {
            run_controls->Add(section_label("Runner"));
            auto menu_option = MenuOption::Vertical();
            menu_option.on_enter = [this] { run_runner(); };
            runner_menu = Menu(&runner_names_, &runner_selected_, menu_option);
            run_controls->Add(runner_menu);
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

        auto solution_page = Renderer(solution_controls, [this, solution_controls] {
            return render_solution_page(solution_controls);
        });
        auto move_page_controls = Container::Vertical({move_controls, move_diagnostics});
        auto move_page = Renderer(
            move_page_controls,
            [this, move_controls, move_diagnostics] {
                return render_move_page(move_controls, move_diagnostics);
            });
        auto run_page = Renderer(run_controls, [this, run_controls] {
            return render_run_page(run_controls);
        });

        auto pages = Container::Tab(
            {solution_page, move_page, run_page},
            &page_selected_);
        refresh_page_labels();
        auto page_menu_option = MenuOption::Horizontal();
        page_menu_option.on_change = [this] {
            if (page_selected_ != 0 && !context_pages_available())
            {
                page_selected_ = 0;
                set_status(
                    status_kind::warning,
                    "Move and Run require a loaded instance and a valid solution");
            }
        };
        auto page_menu = Menu(&page_labels_, &page_selected_, page_menu_option);
        auto main_controls = Container::Vertical({page_menu, pages});

        auto main_renderer = Renderer(main_controls, [this, page_menu, pages] {
            return render_main(page_menu, pages);
        });

        auto browser_menu_option = MenuOption::Vertical();
        browser_menu_option.on_enter = [this] { accept_browser_selection(); };
        auto browser_menu = Menu(
            &browser_labels_,
            &browser_selected_,
            browser_menu_option);
        auto browser_controls = Container::Vertical({
            browser_menu,
            Container::Horizontal({
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
            }),
        });
        auto browser_renderer = Renderer(browser_controls, [this, browser_menu] {
            return render_browser(browser_menu);
        });
        browser_renderer = CatchEvent(browser_renderer, [this](Event event) {
            if (event == Event::Escape)
            {
                browser_visible_ = false;
                return true;
            }
            return false;
        });

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

        auto input_viewer_menu = Menu(
            &input_viewer_lines_,
            &input_viewer_selected_,
            MenuOption::Vertical());
        auto input_viewer_close = Button(
            "Close",
            [this] { input_visible_ = false; },
            ButtonOption::Ascii());
        auto input_viewer_controls = Container::Vertical({
            input_viewer_menu,
            input_viewer_close,
        });
        auto input_viewer = Renderer(
            input_viewer_controls,
            [this, input_viewer_menu, input_viewer_close] {
                refresh_input_viewer_layout();
                return render_input_viewer(input_viewer_menu, input_viewer_close);
            });
        input_viewer = CatchEvent(input_viewer, [this](Event event) {
            if (event == Event::PageUp || event == Event::PageDown)
            {
                input_viewer_selected_ = detail::page_scroll_selection(
                    input_viewer_selected_,
                    input_viewer_lines_.size(),
                    event == Event::PageUp ? -10 : 10);
                return true;
            }
            if (event == Event::Escape || event == Event::F1)
            {
                input_visible_ = false;
                return true;
            }
            return false;
        });

        auto solution_viewer_menu = Menu(
            &solution_viewer_lines_,
            &solution_viewer_selected_,
            MenuOption::Vertical());
        auto solution_viewer_close = Button(
            "Close",
            [this] { solution_visible_ = false; },
            ButtonOption::Ascii());
        auto solution_viewer_controls = Container::Vertical({
            solution_viewer_menu,
            solution_viewer_close,
        });
        auto solution_viewer = Renderer(
            solution_viewer_controls,
            [this, solution_viewer_menu, solution_viewer_close] {
                refresh_solution_viewer_layout();
                return render_solution_viewer(solution_viewer_menu, solution_viewer_close);
            });
        solution_viewer = CatchEvent(solution_viewer, [this](Event event) {
            if (event == Event::PageUp || event == Event::PageDown)
            {
                solution_viewer_selected_ = detail::page_scroll_selection(
                    solution_viewer_selected_,
                    solution_viewer_lines_.size(),
                    event == Event::PageUp ? -10 : 10);
                return true;
            }
            if (event == Event::Escape || event == Event::F2 ||
                event == Event::s || event == Event::S)
            {
                solution_visible_ = false;
                return true;
            }
            return false;
        });

        Component root = Modal(main_renderer, browser_renderer, &browser_visible_);
        root = Modal(root, help_renderer, &help_visible_);
        root = Modal(root, diagnostic_renderer, &diagnostic_visible_);
        root = Modal(root, progress_renderer, &progress_visible_);
        root = Modal(root, input_viewer, &input_visible_);
        root = Modal(root, solution_viewer, &solution_visible_);
        root = CatchEvent(
            root,
            [this, &app, input_path_component, solution_path_component](Event event) {
                if (event == Event::Custom && run_future_.valid())
                {
                    refresh_runner_progress();
                    if (run_future_.wait_for(std::chrono::seconds{0}) ==
                        std::future_status::ready)
                    {
                        finish_runner_run();
                    }
                    return true;
                }
                if (event == Event::Character('?') || event == Event::h || event == Event::H)
                {
                    help_visible_ = !help_visible_;
                    return true;
                }
                if (browser_visible_ || help_visible_ || diagnostic_visible_ ||
                    progress_visible_)
                {
                    return false;
                }

                if (input_visible_ || solution_visible_)
                {
                    return false;
                }

                const bool editing_path =
                    (input_path_component && input_path_component->Focused()) ||
                    (solution_path_component && solution_path_component->Focused());

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

                if (editing_path)
                {
                    return false;
                }

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
            });

        event_app_ = &app;
        app.Loop(root);
        event_app_ = nullptr;

        // The progress modal normally keeps the frontend alive until the run
        // completes.  Joining here also makes exceptional/event-loop exits
        // deterministic and prevents a worker from outliving the frontend.
        if (run_worker_.joinable())
        {
            run_worker_.join();
        }
    }

private:
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

    [[nodiscard]] auto current_page() const noexcept -> tester_page
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

    [[nodiscard]] auto current_page_shortcuts() const -> std::string
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
            break;
        }
        return result;
    }

    [[nodiscard]] auto handle_page_shortcut(const ftxui::Event& event) -> bool
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

    [[nodiscard]] auto handle_solution_shortcut(const ftxui::Event& event) -> bool
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

    [[nodiscard]] auto handle_move_shortcut(const ftxui::Event& event) -> bool
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
        if constexpr (supports_neighborhood_diagnostics)
        {
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
        }
        if (event == ftxui::Event::a || event == ftxui::Event::A)
        {
            apply_move();
            return true;
        }
        return false;
    }

    [[nodiscard]] auto handle_run_shortcut(const ftxui::Event& event) -> bool
    {
        if (event == ftxui::Event::g || event == ftxui::Event::G)
        {
            run_runner();
            return true;
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

    [[nodiscard]] auto require_input(std::string_view action) -> bool
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

    [[nodiscard]] auto context_pages_available() const noexcept -> bool
    {
        return detail::context_pages_available(tester_);
    }

    void refresh_page_labels()
    {
        const bool enabled = detail::page_available(tester_, tester_page::move);
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
        if (!detail::page_available(tester_, page))
        {
            page_selected_ = page_index(tester_page::solution);
            set_status(
                status_kind::warning,
                "Move and Run require a loaded instance and a valid solution");
            return;
        }
        page_selected_ = page_index(page);
    }

    [[nodiscard]] auto require_solution(std::string_view action) -> bool
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

    [[nodiscard]] auto require_move(std::string_view action) -> bool
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
            tester_.use_random_solution(rng_);
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

    [[nodiscard]] auto resolve_path(std::string_view value) const
        -> std::filesystem::path
    {
        return absolute_path_from(std::filesystem::path{value}, options_.path_base);
    }

    [[nodiscard]] auto format_path(const std::filesystem::path& path) const
        -> std::string
    {
        return display_path(path, options_.path_display, options_.path_base);
    }

    void after_solution_change()
    {
        last_move_result_.clear();
        last_run_result_.clear();
        refresh_page_labels();
        page_selected_ = page_index(detail::page_after_solution_change(tester_));
    }

    void show_input()
    {
        input_viewer_text_ = input_text();
        input_viewer_wrap_width_ = 0;
        refresh_input_viewer_layout();
        input_viewer_selected_ = 0;
        solution_visible_ = false;
        input_visible_ = true;
    }

    void show_solution()
    {
        solution_viewer_text_ = solution_text();
        solution_viewer_wrap_width_ = 0;
        refresh_solution_viewer_layout();
        solution_viewer_selected_ = 0;
        input_visible_ = false;
        solution_visible_ = true;
    }

    [[nodiscard]] auto viewer_wrap_width() const noexcept -> std::size_t
    {
        // Reserve room for the modal borders, menu selection marker, and the
        // vertical scroll indicator.  The value is recomputed on every redraw
        // after a terminal resize.
        const auto available = detail::terminal_available_width();
        return static_cast<std::size_t>(std::max(1, available - 8));
    }

    void refresh_input_viewer_layout()
    {
        const auto width = viewer_wrap_width();
        if (input_viewer_wrap_width_ == width)
        {
            return;
        }
        input_viewer_wrap_width_ = width;
        input_viewer_lines_ = wrap_text_lines(input_viewer_text_, width);
        input_viewer_selected_ = page_scroll_selection(
            input_viewer_selected_,
            input_viewer_lines_.size(),
            0);
    }

    void refresh_solution_viewer_layout()
    {
        const auto width = viewer_wrap_width();
        if (solution_viewer_wrap_width_ == width)
        {
            return;
        }
        solution_viewer_wrap_width_ = width;
        solution_viewer_lines_ = wrap_text_lines(solution_viewer_text_, width);
        solution_viewer_selected_ = page_scroll_selection(
            solution_viewer_selected_,
            solution_viewer_lines_.size(),
            0);
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
            if (tester_.use_random_move(rng_))
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
            for (const auto& entry : result.entries)
            {
                out << '\n' << value_text(entry.move) << " => " << value_text(entry.cost);
            }
            if (result.entries.size() < result.moves)
            {
                out << "\n... " << (result.moves - result.entries.size()) << " more";
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
                rng_, options_.random_distribution_rounds);
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
            const auto after = tester_.evaluate();
            last_move_result_ =
                "Applied: " + value_text(before) + " -> " + value_text(after);
            set_status(status_kind::success, "Move applied");
        });
    }

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

        try
        {
            const auto& selected_name = runner_names_.at(
                static_cast<std::size_t>(runner_selected_));
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
            run_before_ = value_text(tester_.evaluate());
            progress_ = progress_snapshot{
                .mode = progress_mode::indeterminate,
                .current = 0,
                .total = std::nullopt,
                .label = "Running " + run_name_,
            };
            progress_visible_ = true;
            set_status(status_kind::info, "Runner executing: " + run_name_);

            run_progress_state_ = std::make_shared<async_progress_state>();

            std::promise<async_runner_result<typename tester_type::solution_type>> promise;
            run_future_ = promise.get_future();
            auto* event_app = event_app_;
            const auto name = run_name_;

            const auto progress_state = run_progress_state_;
            // Each background run gets its own generator, seeded from the
            // frontend RNG (itself seeded by options.seed), so runs are
            // reproducible and never share state with the UI thread.
            typename tester_type::rng_type run_rng{rng_()};
            run_worker_ = std::jthread(
                [application = std::move(application),
                 input = std::move(input),
                 solution = std::move(solution),
                 run_rng,
                 name,
                 promise = std::move(promise),
                 event_app,
                 progress_state](std::stop_token stop_token) mutable {
                    async_runner_result<typename tester_type::solution_type> completion;
                    std::size_t reports = 0;
                    auto observer = [&](const easylocal::run_progress& progress) {
                        progress_state->evaluations.store(
                            progress.evaluations,
                            std::memory_order_relaxed);
                        progress_state->iterations.store(
                            progress.iterations,
                            std::memory_order_relaxed);
                        progress_state->has_evaluation_limit.store(
                            progress.evaluation_limit.has_value(),
                            std::memory_order_relaxed);
                        progress_state->evaluation_limit.store(
                            progress.evaluation_limit.value_or(0),
                            std::memory_order_relaxed);

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
                        application.for_each_runner_registration_indexed(
                            [&]<class Algorithm, std::size_t Index>(
                                const std::string_view registered_name,
                                const typename Algorithm::parameters_type&) {
                                if (completion.found || registered_name != name)
                                {
                                    return;
                                }

                                auto consume_result = [&](auto result) {
                                    static_assert(
                                        easylocal::search_result_for<
                                            decltype(result),
                                            typename tester_type::solution_type,
                                            typename tester_type::cost_type>,
                                        "TextUI requires runner results to provide the "
                                        "solution and its cost (see easylocal::search_result_for)");
                                    completion.solution.emplace(
                                        std::move(result.solution));
                                };

                                consume_result(application.template run_at_with_rng<Index>(
                                    *input,
                                    std::move(solution),
                                    run_rng,
                                    easylocal::with(control)));
                                completion.cancelled = stop_token.stop_requested();
                                completion.found = true;
                            });
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
            progress_visible_ = false;
            progress_ = {};
            run_progress_state_.reset();
            set_status(status_kind::error, "Run runner: " + std::string{error.what()});
        }
        catch (...)
        {
            progress_visible_ = false;
            progress_ = {};
            run_progress_state_.reset();
            set_status(status_kind::error, "Run runner: unknown error");
        }
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

        const auto evaluations = run_progress_state_->evaluations.load(
            std::memory_order_relaxed);
        const auto iterations = run_progress_state_->iterations.load(
            std::memory_order_relaxed);
        const bool has_limit = run_progress_state_->has_evaluation_limit.load(
            std::memory_order_relaxed);
        const auto limit = run_progress_state_->evaluation_limit.load(
            std::memory_order_relaxed);

        progress_.current = evaluations;
        progress_.total = has_limit ? std::optional<std::size_t>{limit} : std::nullopt;
        progress_.mode = has_limit
            ? progress_mode::determinate
            : progress_mode::indeterminate;
        if (!run_worker_.get_stop_token().stop_requested())
        {
            progress_.label = "Running " + run_name_ +
                              " [eval=" + std::to_string(evaluations) +
                              ", iter=" + std::to_string(iterations) + "]";
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

        if (!completion.error.empty())
        {
            set_status(
                status_kind::error,
                "Runner failed: " + run_name_ + ": " + completion.error);
            return;
        }
        if (!completion.found || !completion.solution)
        {
            set_status(
                status_kind::error,
                "Run runner: runner not found: " + run_name_);
            return;
        }

        tester_.set_solution(std::move(*completion.solution));
        refresh_page_labels();
        if (!tester_.is_valid())
        {
            set_status(
                status_kind::error,
                "Runner completed: " + run_name_ +
                    " produced an INVALID solution; Move and Run disabled");
            return;
        }
        if (completion.cancelled)
        {
            const auto after = tester_.evaluate();
            last_run_result_ =
                run_name_ + ": " + run_before_ + " -> " + value_text(after) +
                " (stopped)";
            set_status(status_kind::warning, "Runner stopped: " + run_name_);
            return;
        }

        const auto after = tester_.evaluate();
        last_run_result_ =
            run_name_ + ": " + run_before_ + " -> " + value_text(after);
        set_status(status_kind::success, "Runner completed: " + run_name_);
    }

    [[nodiscard]] auto solution_status(std::string prefix) const -> std::string
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

    [[nodiscard]] auto move_status(std::string prefix) const -> std::string
    {
        if (tester_.has_move() && !tester_.move_is_valid())
        {
            return prefix + ": INVALID";
        }
        return prefix;
    }

    [[nodiscard]] auto solution_text() const -> std::string
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

    [[nodiscard]] auto input_text() const -> std::string
    {
        if (!tester_.has_input())
        {
            return "<no input loaded>";
        }
        return truncate_text(
            value_text(tester_.input()),
            options_.max_render_chars);
    }

    [[nodiscard]] auto current_cost_text() const -> std::string
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

    [[nodiscard]] auto move_text() const -> std::string
    {
        if (!tester_.has_move())
        {
            return "<no move selected>";
        }
        return truncate_text(value_text(tester_.move()), options_.max_render_chars);
    }

    [[nodiscard]] auto render_solution_summary() const -> ftxui::Element
    {
        using namespace ftxui;
        Elements lines;
        lines.push_back(text(
            std::string{"Instance: "} +
            (tester_.has_input() ? "loaded" : "not loaded")));
        if (tester_.has_input() && !input_path_.empty())
        {
            lines.push_back(paragraph(
                "Instance file: " + format_path(resolve_path(input_path_))));
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

    [[nodiscard]] auto render_progress(const progress_snapshot& progress) const
        -> ftxui::Element
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

    [[nodiscard]] auto render_move_summary() const -> ftxui::Element
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

    [[nodiscard]] auto status_prefix() const -> std::string_view
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

    [[nodiscard]] auto render_status() const -> ftxui::Element
    {
        using namespace ftxui;
        return hbox({
            text(std::string{status_prefix()} + " ") | bold,
            text_lines(status_) | flex,
        });
    }

    [[nodiscard]] auto render_solution_page(
        const ftxui::Component& controls) const -> ftxui::Element
    {
        using namespace ftxui;

        Elements summary;
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
            summary.push_back(text("Setup complete - use F4 Move or F5 Run") | dim);
            break;
        }

        summary.push_back(separator());
        summary.push_back(controls->Render() | flex);
        return window(text(" Input / Output "), vbox(std::move(summary))) | flex;
    }

    [[nodiscard]] auto render_move_page(
        const ftxui::Component& controls,
        const ftxui::Component& diagnostics) const -> ftxui::Element
    {
        using namespace ftxui;
        const auto neighborhood = detail::object_name(
            tester_.runtime().neighborhood());
        auto actions = window(text(" Actions "), controls->Render()) |
                       size(WIDTH, LESS_THAN, 30);
        auto details = window(
                           text(" Move - " + neighborhood + " "),
                           render_move_summary()) |
                       flex;
        auto diagnostic_actions = window(
                                      text(" Diagnostics "),
                                      diagnostics->Render()) |
                                  size(HEIGHT, EQUAL, 3);
        if (ftxui::Terminal::Size().dimx < 90)
        {
            return vbox({
                actions,
                details | flex,
                diagnostic_actions,
            }) | flex;
        }
        return vbox({
            hbox({actions, details}) | flex,
            diagnostic_actions,
        }) | flex;
    }

    [[nodiscard]] auto render_run_page(
        const ftxui::Component& controls) const -> ftxui::Element
    {
        using namespace ftxui;
        Elements body{controls->Render()};
        if (!last_run_result_.empty())
        {
            body.push_back(separator());
            body.push_back(text("Last run") | bold);
            body.push_back(paragraph(last_run_result_));
        }
        return window(text(" Runners "), vbox(std::move(body))) | flex;
    }

    [[nodiscard]] auto render_main(
        const ftxui::Component& page_menu,
        const ftxui::Component& pages) const -> ftxui::Element
    {
        using namespace ftxui;

        const auto title = options_.title + "  |  " + current_instance_name() +
                           "  [seed=" + std::to_string(options_.seed) + "]";
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

    [[nodiscard]] auto current_instance_name() const -> std::string
    {
        const auto basename = path_basename(input_path_);
        if (!basename.empty())
        {
            return basename;
        }
        return tester_.has_input() ? "<memory>" : "<no instance>";
    }

    void show_diagnostic(std::string title, std::string body)
    {
        diagnostic_title_ = std::move(title);
        diagnostic_lines_.clear();
        std::istringstream input{body};
        for (std::string line; std::getline(input, line);)
        {
            diagnostic_lines_.push_back(std::move(line));
        }
        if (diagnostic_lines_.empty())
        {
            diagnostic_lines_.push_back("<no output>");
        }
        diagnostic_selected_ = 0;
        diagnostic_visible_ = true;
    }

    [[nodiscard]] auto render_diagnostic_viewer(
        const ftxui::Component& menu,
        const ftxui::Component& close) const -> ftxui::Element
    {
        using namespace ftxui;
        const auto width = std::min(76, detail::terminal_available_width());
        const auto height = std::min(20, detail::terminal_available_height());
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

    [[nodiscard]] auto render_progress_modal(
        const ftxui::Component& stop) const -> ftxui::Element
    {
        using namespace ftxui;
        const auto width = std::min(60, detail::terminal_available_width());
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

    [[nodiscard]] auto render_input_viewer(
        const ftxui::Component& menu,
        const ftxui::Component& close) const -> ftxui::Element
    {
        using namespace ftxui;
        Elements body;
        if (tester_.has_input() && !input_path_.empty())
        {
            body.push_back(paragraph(
                "File: " + format_path(resolve_path(input_path_))) | dim);
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

    [[nodiscard]] auto render_solution_viewer(
        const ftxui::Component& menu,
        const ftxui::Component& close) const -> ftxui::Element
    {
        using namespace ftxui;
        Elements body;
        if (tester_.has_solution())
        {
            body.push_back(text(
                std::string{"Valid: "} + (tester_.is_valid() ? "yes" : "NO")) |
                           bold);
            body.push_back(text("Cost: " + current_cost_text()) | bold);
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

    [[nodiscard]] auto render_browser(const ftxui::Component& browser_menu) const
        -> ftxui::Element
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
        body.push_back(text("Enter/Open: open file or directory  |  Esc: cancel") | dim);
        return window(text(title), vbox(std::move(body))) |
               size(WIDTH, EQUAL, detail::terminal_available_width()) |
               size(HEIGHT, EQUAL, detail::terminal_available_height()) |
               border;
    }

    [[nodiscard]] auto render_help(const ftxui::Component& controls) const
        -> ftxui::Element
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
            text("L load input   I initial   R random   Shift-L load solution   W save   C check"),
            text("Move page") | bold,
            text("B best   I first improving   F first   N next   R random   A apply"),
            text("P list   T stats   C costs   D independence   U distribution"),
            text("Run page") | bold,
            text("G run selected   Enter run selected   X stop running"),
            separator(),
            text("Q  " + options_.exit_label),
            text("? / H  help   Esc  close"),
            separator(),
            text("Page shortcuts apply only to the active page.") | dim,
            text("Move and Run unlock after a valid solution is available.") | dim,
            text("Paths are shown relative to the configured base by default.") | dim,
            text("Shortcuts are disabled while a path field is focused.") | dim,
            separator(),
            controls->Render(),
        };
        return window(text(" Keyboard help "), vbox(std::move(lines))) |
               size(WIDTH, EQUAL, detail::terminal_available_width()) |
               size(HEIGHT, EQUAL, detail::terminal_available_height()) |
               border;
    }

    tester_type& tester_;
    tester_options options_;
    typename tester_type::rng_type rng_;

    std::string input_path_;
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

    bool help_visible_{};
    bool diagnostic_visible_{};
    std::string diagnostic_title_;
    std::vector<std::string> diagnostic_lines_;
    int diagnostic_selected_{};
    bool progress_visible_{};
    progress_snapshot progress_{};
    ftxui::App* event_app_{};
    std::jthread run_worker_{};
    std::future<async_runner_result<typename tester_type::solution_type>> run_future_{};
    std::shared_ptr<async_progress_state> run_progress_state_;
    std::string run_name_;
    std::string run_before_;
    bool input_visible_{};
    std::string input_viewer_text_;
    std::vector<std::string> input_viewer_lines_{""};
    std::size_t input_viewer_wrap_width_{};
    int input_viewer_selected_{};
    bool solution_visible_{};
    std::string solution_viewer_text_;
    std::vector<std::string> solution_viewer_lines_{""};
    std::size_t solution_viewer_wrap_width_{};
    int solution_viewer_selected_{};
};

} // namespace detail

template<class App>
void run(easylocal::Tester<App>& tester, tester_options options = {})
{
    detail::tester_frontend<App>{tester, std::move(options)}.run();
}

} // namespace easylocal::tui
