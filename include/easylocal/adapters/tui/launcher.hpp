#pragma once

/// \file
/// run_launcher: a menu of several apps on the same problem, each opened in the
/// interactive tester when selected.
///
/// The apps share the Input and the current solution, which the launcher owns:
/// its first entry, "Input and solution", loads and saves them, and each app
/// opens on them in a complete tester, which may load and save files, create
/// solutions, explore moves and run; what it leaves becomes the shared state,
/// so a solution built with one neighborhood can be explored with another.
/// Each app keeps its own session: its runner and problem parameters and its
/// seed stay from one opening to the next.

#include <easylocal/adapters/tui/tester.hpp>
#include <easylocal/app/session.hpp>

#include <concepts>
#include <cstddef>
#include <filesystem>
#include <ftxui/ftxui.hpp>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace easylocal::tui
{

/// The options of the launcher, the TextUI that lists several apps and opens
/// the tester of the one selected.
struct launcher_options
{
    /// The title of the application list, and the first part of each tester's
    /// title.
    std::string title{"EasyLocal Tester"};
    /// The options of the testers it opens; their titles add the app's name,
    /// and their q leads back to the list.
    options tester{};
};

namespace detail
{

template<class... Apps>
[[nodiscard]] std::vector<std::string> application_names(
    const std::tuple<Apps...>& applications)
{
    std::vector<std::string> names;
    names.reserve(sizeof...(Apps));
    std::apply(
        [&](const auto&... application) {
            (names.emplace_back(application.name()), ...);
        },
        applications);
    return names;
}

// The session of an app in the launcher, created when the app is first
// opened, and what its tester remembers from one opening to the next.
template<class App>
struct launched_app
{
    std::optional<easylocal::Session<App>> session;
    std::optional<frontend_memory> memory;
};

template<class FirstApp, class... Apps>
class launcher_frontend
{
    using first_session_type = easylocal::Session<FirstApp>;
    using solution_manager_type = typename first_session_type::solution_manager_type;
    using input_type = typename first_session_type::input_type;
    using solution_type = typename first_session_type::solution_type;

    // The Input and the solution pass from one app to the next, so every app
    // must have the same SolutionManager, cost included: the same recipe.
    static_assert(
        (std::same_as<
             typename easylocal::Session<Apps>::solution_manager_type,
             solution_manager_type>
            && ...),
        "the apps of a launcher share the Input and the current solution: they "
        "must have the same SolutionManager recipe (and differ, for example, in "
        "the neighborhood or the runners)");

public:
    explicit launcher_frontend(
        launcher_options options,
        FirstApp application,
        Apps... applications)
        : options_{std::move(options)},
          applications_{std::move(application), std::move(applications)...},
          names_{application_names(applications_)}
    {
        names_.insert(names_.begin(), std::string{root_entry});
    }

    void run()
    {
        if constexpr (first_session_type::supports_read_input)
        {
            if (!options_.tester.input_path.empty())
            {
                input_file_ = detail::initial_input_file(options_.tester);
                input_ = std::make_shared<const input_type>(
                    easylocal::load_input<input_type>(input_file_));
            }
        }

        while (const auto selected = choose_application())
        {
            if (*selected == 0)
            {
                open_tester(
                    std::get<0>(applications_),
                    root_,
                    std::string{root_entry},
                    detail::frontend_role::launcher_root);
                continue;
            }
            open_application(*selected - 1, std::index_sequence_for<FirstApp, Apps...>{});
        }
    }

private:
    static constexpr std::string_view root_entry{"Input and solution"};

    // The tester of the app at index selected, with its own session.
    template<std::size_t... Index>
    void open_application(const std::size_t selected, std::index_sequence<Index...>)
    {
        (
            [&] {
                if (selected == Index)
                    open_tester(
                        std::get<Index>(applications_),
                        std::get<Index>(launched_),
                        std::string{std::get<Index>(applications_).name()},
                        detail::frontend_role::launcher_child);
            }(),
            ...);
    }

    // A tester on application, from the shared Input and solution, as the
    // root (Input/Output only) or as a child (no file loading), on the
    // session the app kept from its last opening; what the tester leaves
    // becomes the shared state for the next one.
    template<class Selected>
    void open_tester(
        const Selected& application,
        launched_app<Selected>& launched,
        const std::string& name,
        const detail::frontend_role role)
    {
        auto settings = options_.tester;
        settings.title = options_.title + " - " + name;
        // The file of the shared Input, which a tester may have loaded.
        settings.input_path = input_ ? input_file_ : std::filesystem::path{};
        if (launched.memory)
            settings.seed = launched.memory->seed;

        if (!launched.session)
            launched.session.emplace(application, settings.seed);
        auto& session = *launched.session;
        // The shared state: a new Input (or no solution) is bound again, which
        // drops the session's solution.
        if (input_
            && (session.input_handle() != input_
                || (!solution_ && session.has_solution())))
            session.set_input(input_);
        if (input_ && solution_)
            session.set_solution(*solution_);

        detail::tester_frontend<Selected> frontend{
            session,
            std::move(settings),
            role,
            launched.memory ? &*launched.memory : nullptr};
        frontend.run();
        launched.memory = frontend.memory();

        input_ = session.has_input() ? session.input_handle() : nullptr;
        input_file_ = frontend.loaded_input_path();
        if (input_ && session.has_solution())
            solution_ = session.solution();
        else
            solution_.reset();
    }

    [[nodiscard]] std::string shared_state() const
    {
        if (!input_)
            return "No input loaded";
        return solution_
            ? "Input and solution shared by the applications"
            : "Input shared by the applications; no solution yet";
    }

    [[nodiscard]] std::optional<std::size_t> choose_application()
    {
        using namespace ftxui;

        auto app = ftxui::App::TerminalOutput();
        // The entry selected last, so that coming back to the list keeps it.
        int& selected = selected_;
        bool open = false;

        auto menu_option = MenuOption::Vertical();
        menu_option.on_enter = [&] {
            open = true;
            app.Exit();
        };
        auto menu = Menu(&names_, &selected, menu_option);

        auto buttons = Container::Horizontal({
            Button(
                "Open",
                [&] {
                    open = true;
                    app.Exit();
                }),
            Button(
                "Quit",
                [&] {
                    open = false;
                    app.Exit();
                }),
        });
        auto controls = Container::Vertical({menu, buttons});

        auto root = Renderer(controls, [&] {
            return vbox({
                       text(options_.title) | bold | center,
                       text("Load the input and the solution, or open an application")
                           | center | dim,
                       text(shared_state()) | center | dim,
                       separator(),
                       window(text(" Applications "), menu->Render()) | flex,
                       separator(),
                       buttons->Render() | center,
                       text("Enter: open  |  q/Esc: quit") | center | dim,
                   })
                | border;
        });

        root = CatchEvent(root, [&](Event event) {
            if (event == Event::q || event == Event::Q || event == Event::Escape)
            {
                open = false;
                app.Exit();
                return true;
            }
            return false;
        });

        app.Loop(root);
        if (!open || selected < 0 || static_cast<std::size_t>(selected) >= names_.size())
            return std::nullopt;
        return static_cast<std::size_t>(selected);
    }

    launcher_options options_;
    std::tuple<FirstApp, Apps...> applications_;
    std::tuple<launched_app<FirstApp>, launched_app<Apps>...> launched_;
    launched_app<FirstApp> root_;
    std::vector<std::string> names_;
    int selected_{0};
    std::shared_ptr<const input_type> input_;
    // The file input_ was read from; empty when it was not read from a file.
    std::filesystem::path input_file_;
    std::optional<solution_type> solution_;
};

} // namespace detail

/// Opens a menu of the apps, which share the Input (loaded from
/// options.tester.input_path when it is set) and the current solution.
///
/// The apps must have the same SolutionManager recipe: this is checked when the
/// program is compiled.
template<class... Apps>
    requires(sizeof...(Apps) > 0) && (std::copy_constructible<Apps> && ...)
void run_launcher(launcher_options options, Apps... applications)
{
    detail::launcher_frontend<Apps...>{std::move(options), std::move(applications)...}
        .run();
}

} // namespace easylocal::tui
