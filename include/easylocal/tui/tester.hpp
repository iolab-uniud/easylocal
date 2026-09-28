#pragma once

#include <easylocal/check.hpp>
#include <easylocal/tester.hpp>

#include <ftxui/ftxui.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
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

[[nodiscard]] constexpr auto page_index(const tester_page page) noexcept -> int
{
    return static_cast<int>(page);
}

template<class Tester>
[[nodiscard]] auto context_pages_available(const Tester& tester) -> bool
{
    if (!tester.has_input() || !tester.has_solution())
    {
        return false;
    }
    if constexpr (requires { tester.is_valid(); })
    {
        return static_cast<bool>(tester.is_valid());
    }
    return true;
}

template<class Tester>
[[nodiscard]] auto page_available(
    const Tester& tester,
    const tester_page page) -> bool
{
    return page == tester_page::solution || context_pages_available(tester);
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

template<class App>
class tester_frontend
{
public:
    using tester_type = easylocal::Tester<App>;

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
        }
        if (!options_.solution_path.empty())
        {
            solution_path_ = editable_path(
                options_.solution_path,
                options_.path_display,
                options_.path_base);
        }
        for (const auto name : tester_.runner_names())
        {
            runner_names_.emplace_back(name);
        }
    }

    void run()
    {
        using namespace ftxui;

        auto app = ftxui::App::TerminalOutput();

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
            solution_controls->Add(section_label("Instance"));
            auto option = InputOption::Default();
            option.multiline = false;
            option.on_enter = [this] { load_input(); };
            input_path_component = Input(&input_path_, "instance file", option);
            solution_controls->Add(input_path_component);
            solution_controls->Add(Container::Horizontal({
                Button(
                    "O Open",
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
            solution_controls->Add(section_label("Solution file"));
            auto option = InputOption::Default();
            option.multiline = false;
            if constexpr (tester_type::supports_solution_loading)
            {
                option.on_enter = [this] { load_solution(); };
            }
            solution_path_component = Input(&solution_path_, "solution file", option);
            solution_controls->Add(solution_path_component);

            std::vector<Component> file_actions;
            file_actions.push_back(Button(
                "Browse...",
                [this] { open_browser(file_target::solution); },
                ButtonOption::Ascii()));
            if constexpr (tester_type::supports_solution_loading)
            {
                file_actions.push_back(Button(
                    "L Load",
                    [this] { load_solution(); },
                    ButtonOption::Ascii()));
            }
            if constexpr (tester_type::supports_solution_saving)
            {
                file_actions.push_back(Button(
                    "S Save",
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
                "M Random",
                [this] { random_move(); },
                ButtonOption::Ascii()));
        }
        move_controls->Add(section_label("Commit"));
        move_controls->Add(Button(
            "A Apply",
            [this] { apply_move(); },
            ButtonOption::Ascii()));

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
        auto move_page = Renderer(move_controls, [this, move_controls] {
            return render_move_page(move_controls);
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
            if (event == Event::Escape || event == Event::F1)
            {
                help_visible_ = false;
                return true;
            }
            return false;
        });

        Component root = Modal(main_renderer, browser_renderer, &browser_visible_);
        root = Modal(root, help_renderer, &help_visible_);
        root = CatchEvent(
            root,
            [this, &app, input_path_component, solution_path_component](Event event) {
                if (event == Event::F1)
                {
                    help_visible_ = !help_visible_;
                    return true;
                }
                if (browser_visible_ || help_visible_)
                {
                    return false;
                }

                const bool editing_path =
                    (input_path_component && input_path_component->Focused()) ||
                    (solution_path_component && solution_path_component->Focused());
                if (editing_path)
                {
                    return false;
                }

                if (event == Event::q || event == Event::Q)
                {
                    app.Exit();
                    return true;
                }
                if (event == Event::Character('1'))
                {
                    select_page(tester_page::solution);
                    return true;
                }
                if (event == Event::Character('2'))
                {
                    select_page(tester_page::move);
                    return true;
                }
                if (event == Event::Character('3'))
                {
                    select_page(tester_page::run);
                    return true;
                }

                return handle_page_shortcut(event);
            });

        app.Loop(root);
    }

private:
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
                append("O Open");
            if constexpr (tester_type::supports_initial_solution)
                append("I Initial");
            if constexpr (tester_type::supports_random_solution)
                append("R Random");
            if constexpr (tester_type::supports_solution_loading)
                append("L Load");
            if constexpr (tester_type::supports_solution_saving)
                append("S Save");
            append("C Check");
            break;
        case tester_page::move:
            if constexpr (tester_type::supports_deterministic_moves)
            {
                append("F First");
                append("N Next");
            }
            if constexpr (tester_type::supports_random_moves)
                append("M Random");
            append("A Apply");
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
            if (event == ftxui::Event::o || event == ftxui::Event::O)
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
            if (event == ftxui::Event::l || event == ftxui::Event::L)
            {
                load_solution();
                return true;
            }
        }
        if constexpr (tester_type::supports_solution_saving)
        {
            if (event == ftxui::Event::s || event == ftxui::Event::S)
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
            if (event == ftxui::Event::m || event == ftxui::Event::M)
            {
                random_move();
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
            std::string{action} + ": load an instance first");
        return false;
    }

    [[nodiscard]] auto context_pages_available() const noexcept -> bool
    {
        return detail::context_pages_available(tester_);
    }

    void refresh_page_labels()
    {
        const bool enabled = detail::page_available(tester_, tester_page::move);
        page_labels_[0] = "Solution";
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
            set_status(status_kind::warning, "Open instance: enter or browse a file name");
            return;
        }
        perform("Open instance", [this] {
            const auto path = resolve_path(input_path_);
            tester_.load_input(path);
            refresh_page_labels();
            set_status(
                status_kind::success,
                "Loaded instance: " + format_path(path) +
                    "; choose Initial, Random, or Load solution as available");
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
            refresh_page_labels();
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
            refresh_page_labels();
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
            refresh_page_labels();
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
            set_status(
                status_kind::success,
                "Move applied: cost " + value_text(before) + " -> " +
                    value_text(after));
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
        perform("Run runner", [this] {
            const auto& name = runner_names_.at(
                static_cast<std::size_t>(runner_selected_));
            const auto before = tester_.evaluate();
            if (!tester_.run_runner(name))
            {
                set_status(
                    status_kind::error,
                    "Run runner: runner not found: " + name);
                return;
            }
            refresh_page_labels();
            if (!tester_.is_valid())
            {
                set_status(
                    status_kind::error,
                    "Runner completed: " + name +
                        " produced an INVALID solution; Move and Run disabled");
                return;
            }
            const auto after = tester_.evaluate();
            set_status(
                status_kind::success,
                "Runner completed: " + name + " | cost " +
                    value_text(before) + " -> " + value_text(after));
        });
    }

    [[nodiscard]] auto solution_status(std::string prefix) const -> std::string
    {
        if (!tester_.has_solution())
        {
            return prefix;
        }
        const bool valid = tester_.is_valid();
        prefix += valid ? " (valid" : " (INVALID";
        if (valid)
        {
            prefix += ", cost=" + value_text(tester_.evaluate());
        }
        prefix += ')';
        prefix += valid ? "; Move and Run enabled" : "; Move and Run disabled";
        return prefix;
    }

    [[nodiscard]] auto move_status(std::string prefix) const -> std::string
    {
        if (!tester_.has_move())
        {
            return prefix;
        }
        const bool valid = tester_.move_is_valid();
        prefix += valid ? " (valid)" : " (INVALID)";
        if (!valid)
        {
            return prefix;
        }

        prefix += ", incremental=" + value_text(tester_.evaluate_move());
        prefix += ", full=" + value_text(tester_.evaluate_move_fully());
        if constexpr (requires(const tester_type& tester) {
                          { tester.move_evaluation_matches_full() }
                              -> std::same_as<bool>;
                      })
        {
            prefix += tester_.move_evaluation_matches_full()
                          ? ", delta=ok"
                          : ", delta=FAILED";
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

    [[nodiscard]] auto render_move_summary() const -> ftxui::Element
    {
        using namespace ftxui;
        Elements lines;
        if (!tester_.has_move())
        {
            lines.push_back(text("No move selected") | dim);
            return vbox(std::move(lines));
        }

        const bool valid = tester_.move_is_valid();
        lines.push_back(text(std::string{"Valid: "} + (valid ? "yes" : "NO")));
        lines.push_back(separator());
        lines.push_back(text_lines(move_text()));
        if (valid)
        {
            lines.push_back(separator());
            lines.push_back(text("Incremental: " + value_text(tester_.evaluate_move())));
            lines.push_back(text("Full:        " + value_text(tester_.evaluate_move_fully())));
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
        auto actions = window(text(" Actions "), controls->Render()) |
                       size(WIDTH, LESS_THAN, 38);
        auto summary = window(
                           text(" Solution "),
                           vbox({
                               render_solution_summary(),
                               separator(),
                               text_lines(solution_text()) | flex,
                           })) |
                       flex;
        return hbox({actions, summary}) | flex;
    }

    [[nodiscard]] auto render_move_page(
        const ftxui::Component& controls) const -> ftxui::Element
    {
        using namespace ftxui;
        const auto neighborhood = detail::object_name(
            tester_.instance().neighborhood());
        auto actions = window(text(" Actions "), controls->Render()) |
                       size(WIDTH, LESS_THAN, 30);
        auto details = window(
                           text(" Move - " + neighborhood + " "),
                           vbox({
                               text("Current cost: " + value_text(tester_.evaluate())),
                               separator(),
                               render_move_summary(),
                           })) |
                       flex;
        return hbox({actions, details}) | flex;
    }

    [[nodiscard]] auto render_run_page(
        const ftxui::Component& controls) const -> ftxui::Element
    {
        using namespace ftxui;
        Elements summary{
            text("Registered runners: " + std::to_string(runner_names_.size())),
        };
        if (!runner_names_.empty())
        {
            summary.push_back(text(
                "Selected: " + runner_names_.at(
                    static_cast<std::size_t>(runner_selected_))));
        }
        summary.push_back(separator());

        const bool valid = tester_.is_valid();
        summary.push_back(text(std::string{"Current solution: "} +
                               (valid ? "valid" : "INVALID")));
        if (valid)
        {
            summary.push_back(text("Cost: " + value_text(tester_.evaluate())));
        }

        auto runners = window(text(" Runners "), controls->Render()) |
                       size(WIDTH, LESS_THAN, 38);
        auto state = window(text(" Run context "), vbox(std::move(summary))) | flex;
        return hbox({runners, state}) | flex;
    }

    [[nodiscard]] auto render_main(
        const ftxui::Component& page_menu,
        const ftxui::Component& pages) const -> ftxui::Element
    {
        using namespace ftxui;

        return vbox({
                   text(options_.title) | bold | center,
                   text("semantic tester  |  seed=" + std::to_string(options_.seed)) | center | dim,
                   separator(),
                   page_menu->Render() | center,
                   separator(),
                   pages->Render() | flex,
                   window(text(" Status "), render_status()) |
                       size(HEIGHT, LESS_THAN, 4),
                   text(current_page_shortcuts()) | center,
                   text("1 Solution  2 Move  3 Run  |  F1 help  |  q " +
                        options_.exit_label +
                        "  |  Tab/Shift-Tab focus") |
                       center | dim,
               }) |
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
        }
        else
        {
            solution_path_ = std::move(selected_text);
        }
        browser_visible_ = false;
        set_status(status_kind::info, "Selected file: " + format_path(selected.path));
    }

    [[nodiscard]] auto render_browser(const ftxui::Component& browser_menu) const
        -> ftxui::Element
    {
        using namespace ftxui;
        const auto title = browser_target_ == file_target::input
                               ? " Select instance file "
                               : " Select solution file ";
        Elements body{
            text(format_path(browser_directory_)) | bold,
            separator(),
            browser_menu->Render() | vscroll_indicator | frame |
                size(HEIGHT, LESS_THAN, 16),
        };
        if (!browser_error_.empty())
        {
            body.push_back(separator());
            body.push_back(text("ERROR: " + browser_error_) | bold);
        }
        body.push_back(separator());
        body.push_back(text("Enter/Open: open file or directory  |  Esc: cancel") | dim);
        return window(text(title), vbox(std::move(body))) |
               size(WIDTH, GREATER_THAN, 60) |
               border;
    }

    [[nodiscard]] auto render_help(const ftxui::Component& controls) const
        -> ftxui::Element
    {
        using namespace ftxui;
        Elements lines{
            text("Pages") | bold,
            separator(),
            text("1  Solution"),
            text("2  Move"),
            text("3  Run"),
            separator(),
            text("Solution page") | bold,
            text("O open instance   I initial   R random   L load   S save   C check"),
            text("Move page") | bold,
            text("F first   N next   M random   A apply"),
            text("Run page") | bold,
            text("G run selected   Enter run selected"),
            separator(),
            text("Q  " + options_.exit_label),
            text("F1 / Esc  close this help"),
            separator(),
            text("Page shortcuts apply only to the active page.") | dim,
            text("Move and Run unlock after a valid solution is available.") | dim,
            text("Paths are shown relative to the configured base by default.") | dim,
            text("Shortcuts are disabled while a path field is focused.") | dim,
            separator(),
            controls->Render(),
        };
        return window(text(" Keyboard help "), vbox(std::move(lines))) |
               size(WIDTH, GREATER_THAN, 52) |
               border;
    }

    tester_type& tester_;
    tester_options options_;
    typename tester_type::rng_type rng_;

    std::string input_path_;
    std::string solution_path_;

    status_kind status_kind_{status_kind::info};
    std::string status_{"Ready"};

    std::vector<std::string> runner_names_;
    int runner_selected_{};

    std::vector<std::string> page_labels_{"Solution", "Move", "Run"};
    int page_selected_{};

    bool browser_visible_{};
    file_target browser_target_{file_target::input};
    std::filesystem::path browser_directory_;
    std::vector<file_entry> browser_entries_;
    std::vector<std::string> browser_labels_;
    int browser_selected_{};
    std::string browser_error_;

    bool help_visible_{};
};

} // namespace detail

template<class App>
void run(easylocal::Tester<App>& tester, tester_options options = {})
{
    detail::tester_frontend<App>{tester, std::move(options)}.run();
}

} // namespace easylocal::tui
