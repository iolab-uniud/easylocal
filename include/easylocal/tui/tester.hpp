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

        auto solution_controls = Container::Vertical({});
        if constexpr (tester_type::supports_input_loading)
        {
            auto option = InputOption::Default();
            option.multiline = false;
            option.on_enter = [this] { load_input(); };
            input_path_component = Input(&input_path_, "instance file", option);
            solution_controls->Add(input_path_component);
            solution_controls->Add(Container::Horizontal({
                Button("[O] Open", [this] { load_input(); }),
                Button("Browse...", [this] { open_browser(file_target::input); }),
            }));
        }

        if constexpr (tester_type::supports_initial_solution)
        {
            solution_controls->Add(Button("[I] Initial solution", [this] {
                use_initial_solution();
            }));
        }
        if constexpr (tester_type::supports_random_solution)
        {
            solution_controls->Add(Button("[R] Random solution", [this] {
                use_random_solution();
            }));
        }

        if constexpr (tester_type::supports_solution_loading ||
                      tester_type::supports_solution_saving)
        {
            auto option = InputOption::Default();
            option.multiline = false;
            if constexpr (tester_type::supports_solution_loading)
            {
                option.on_enter = [this] { load_solution(); };
            }
            solution_path_component = Input(&solution_path_, "solution file", option);
            solution_controls->Add(solution_path_component);
            solution_controls->Add(Button("Browse...", [this] {
                open_browser(file_target::solution);
            }));
        }
        if constexpr (tester_type::supports_solution_loading)
        {
            solution_controls->Add(Button("[L] Load solution", [this] {
                load_solution();
            }));
        }
        if constexpr (tester_type::supports_solution_saving)
        {
            solution_controls->Add(Button("[S] Save solution", [this] {
                save_solution();
            }));
        }
        solution_controls->Add(Button("[C] Check", [this] { check(); }));

        auto move_controls = Container::Vertical({});
        if constexpr (tester_type::supports_deterministic_moves)
        {
            move_controls->Add(Button("[F] First move", [this] { first_move(); }));
            move_controls->Add(Button("[N] Next move", [this] { next_move(); }));
        }
        if constexpr (tester_type::supports_random_moves)
        {
            move_controls->Add(Button("[M] Random move", [this] { random_move(); }));
        }
        move_controls->Add(Button("[A] Apply move", [this] { apply_move(); }));

        auto run_controls = Container::Vertical({});
        Component runner_menu;
        if (!runner_names_.empty())
        {
            auto menu_option = MenuOption::Vertical();
            menu_option.on_enter = [this] { run_runner(); };
            runner_menu = Menu(&runner_names_, &runner_selected_, menu_option);
            run_controls->Add(runner_menu);
            run_controls->Add(Button("[G] Run selected", [this] { run_runner(); }));
        }
        else
        {
            run_controls->Add(Renderer([] {
                return text("No runner registered") | dim;
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
        auto page_menu_option = MenuOption::HorizontalAnimated();
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
                Button("Open", [this] { accept_browser_selection(); }),
                Button("Up", [this] { browser_up(); }),
                Button("Cancel", [this] { browser_visible_ = false; }),
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
            Button("Close", [this] { help_visible_ = false; }),
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
                    page_selected_ = 0;
                    return true;
                }
                if (event == Event::Character('2'))
                {
                    page_selected_ = 1;
                    return true;
                }
                if (event == Event::Character('3'))
                {
                    page_selected_ = 2;
                    return true;
                }
                if constexpr (tester_type::supports_input_loading)
                {
                    if (event == Event::o || event == Event::O)
                    {
                        load_input();
                        return true;
                    }
                }
                if constexpr (tester_type::supports_initial_solution)
                {
                    if (event == Event::i || event == Event::I)
                    {
                        use_initial_solution();
                        return true;
                    }
                }
                if constexpr (tester_type::supports_random_solution)
                {
                    if (event == Event::r || event == Event::R)
                    {
                        use_random_solution();
                        return true;
                    }
                }
                if constexpr (tester_type::supports_solution_loading)
                {
                    if (event == Event::l || event == Event::L)
                    {
                        load_solution();
                        return true;
                    }
                }
                if constexpr (tester_type::supports_solution_saving)
                {
                    if (event == Event::s || event == Event::S)
                    {
                        save_solution();
                        return true;
                    }
                }
                if (event == Event::c || event == Event::C)
                {
                    check();
                    return true;
                }
                if constexpr (tester_type::supports_deterministic_moves)
                {
                    if (event == Event::f || event == Event::F)
                    {
                        first_move();
                        return true;
                    }
                    if (event == Event::n || event == Event::N)
                    {
                        next_move();
                        return true;
                    }
                }
                if constexpr (tester_type::supports_random_moves)
                {
                    if (event == Event::m || event == Event::M)
                    {
                        random_move();
                        return true;
                    }
                }
                if (event == Event::a || event == Event::A)
                {
                    apply_move();
                    return true;
                }
                if (event == Event::g || event == Event::G)
                {
                    run_runner();
                    return true;
                }
                return false;
            });

        app.Loop(root);
    }

private:
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
            set_status(
                status_kind::success,
                "Loaded instance: " + format_path(path) +
                    " (solution state cleared)");
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
            std::ostringstream out;
            easylocal::print_report(out, report);
            set_status(status_kind::info, out.str());
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
        perform("Next move", [this] {
            if (tester_.use_next_move())
            {
                set_status(status_kind::success, move_status("Selected next move"));
            }
            else
            {
                set_status(status_kind::warning, "Next move: no further move");
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
            tester_.apply_move();
            set_status(status_kind::success, solution_status("Move applied"));
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
            if (!tester_.run_runner(name))
            {
                set_status(
                    status_kind::error,
                    "Run runner: runner not found: " + name);
                return;
            }
            set_status(
                status_kind::success,
                solution_status("Runner completed: " + name));
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
            lines.push_back(text("Instance file: " + format_path(resolve_path(input_path_))));
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
        auto actions = window(text(" Actions "), controls->Render()) |
                       size(WIDTH, LESS_THAN, 34);
        auto details = window(text(" Selected move "), render_move_summary()) | flex;
        return hbox({actions, details}) | flex;
    }

    [[nodiscard]] auto render_run_page(
        const ftxui::Component& controls) const -> ftxui::Element
    {
        using namespace ftxui;
        Elements summary{
            text("Registered runners: " + std::to_string(runner_names_.size())),
            separator(),
        };
        if (tester_.has_solution())
        {
            const bool valid = tester_.is_valid();
            summary.push_back(text(std::string{"Current solution: "} +
                                   (valid ? "valid" : "INVALID")));
            if (valid)
            {
                summary.push_back(text("Cost: " + value_text(tester_.evaluate())));
            }
        }
        else
        {
            summary.push_back(text("Current solution: not selected") | dim);
        }
        auto runners = window(text(" Runners "), controls->Render()) |
                       size(WIDTH, LESS_THAN, 42);
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
                   window(text(" Status "), render_status()),
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
            text("Global shortcuts") | bold,
            separator(),
            text("1  Solution page"),
            text("2  Move page"),
            text("3  Run page"),
            separator(),
            text("O  open instance file"),
            text("I  initial solution"),
            text("R  random solution"),
            text("L  load solution"),
            text("S  save solution"),
            text("C  semantic check"),
            text("F  first move"),
            text("N  next move"),
            text("M  random move"),
            text("A  apply selected move"),
            text("G  run selected runner"),
            text("Q  " + options_.exit_label),
            separator(),
            text("F1 / Esc  close this help"),
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
