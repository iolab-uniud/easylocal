#pragma once

/// \file
/// run_launcher: a menu of several apps on the same problem, each opened in the
/// interactive tester when selected.
///
/// The apps share the Input and the current solution, which the launcher owns:
/// its first entry, "Input and solution", loads and saves them, and each app
/// opens on them, creates solutions, moves and runs, but loads no files; what
/// it leaves becomes the shared state, so a solution built with one
/// neighborhood can be explored with another.

#include <easylocal/adapters/tui/tester.hpp>
#include <easylocal/app/session.hpp>

#include <concepts>
#include <cstddef>
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
    /// and their exit leads back to the list.
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

template<std::size_t Index = 0, class Tuple, class Function>
bool visit_application_at(
    Tuple& applications,
    const std::size_t selected,
    Function&& function)
{
    if constexpr (Index == std::tuple_size_v<std::remove_reference_t<Tuple>>)
    {
        return false;
    }
    else
    {
        if (selected == Index)
        {
            std::forward<Function>(function)(std::get<Index>(applications));
            return true;
        }
        return visit_application_at<Index + 1>(
            applications,
            selected,
            std::forward<Function>(function));
    }
}

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
        if constexpr (first_session_type::supports_input_loading)
        {
            if (!options_.tester.input_path.empty())
                input_ =
                    std::make_shared<const input_type>(easylocal::load_input<input_type>(
                        detail::initial_input_file(options_.tester)));
        }

        while (const auto selected = choose_application())
        {
            if (*selected == 0)
            {
                open_tester(
                    std::get<0>(applications_),
                    std::string{root_entry},
                    detail::frontend_role::launcher_root);
                continue;
            }
            const bool dispatched = visit_application_at(
                applications_,
                *selected - 1,
                [this](auto& application) {
                    this->open_tester(
                        application,
                        std::string{application.name()},
                        detail::frontend_role::launcher_child);
                });
            (void)dispatched;
        }
    }

private:
    static constexpr std::string_view root_entry{"Input and solution"};

    // A tester on application, from the shared Input and solution, as the
    // root (Input/Output only) or as a child (no file loading); what the
    // tester leaves becomes the shared state for the next one.
    template<class Selected>
    void open_tester(
        const Selected& application,
        const std::string& name,
        const detail::frontend_role role)
    {
        auto settings = options_.tester;
        settings.title = options_.title + " - " + name;
        settings.exit_label = "back to applications";

        easylocal::Session<Selected> session{application, settings.seed};
        if (input_)
            session.set_input(input_);
        if (input_ && solution_)
            session.set_solution(*solution_);

        detail::tester_frontend<Selected>{session, std::move(settings), role}.run();

        input_ = session.has_input() ? session.input_handle() : nullptr;
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

        auto app = App::TerminalOutput();
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
    std::vector<std::string> names_;
    int selected_{0};
    std::shared_ptr<const input_type> input_;
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
