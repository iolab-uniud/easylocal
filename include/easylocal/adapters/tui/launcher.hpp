#pragma once

#include <easylocal/adapters/tui/tester.hpp>
#include <easylocal/app/session.hpp>

#include <concepts>
#include <cstddef>
#include <ftxui/ftxui.hpp>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace easylocal::tui
{

struct launcher_options
{
    std::string title{"EasyLocal Tester"};
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

template<class... Apps>
class launcher_frontend
{
public:
    explicit launcher_frontend(launcher_options options, Apps... applications)
        : options_{std::move(options)},
          applications_{std::move(applications)...},
          names_{application_names(applications_)}
    {
    }

    void run()
    {
        while (const auto selected = choose_application())
        {
            const bool dispatched =
                visit_application_at(applications_, *selected, [this](auto& application) {
                    auto settings = options_.tester;
                    settings.title =
                        options_.title + " - " + std::string{application.name()};
                    settings.exit_label = "back to applications";
                    easylocal::tui::run(application, std::move(settings));
                });
            (void)dispatched;
        }
    }

private:
    [[nodiscard]] std::optional<std::size_t> choose_application()
    {
        using namespace ftxui;

        auto app = App::TerminalOutput();
        int selected = 0;
        bool open = false;

        auto menu_option = MenuOption::Vertical();
        menu_option.on_enter = [&] {
            open = true;
            app.Exit();
        };
        auto menu = Menu(&names_, &selected, menu_option);

        auto buttons = Container::Horizontal({
            Button("Open", [&] {
                open = true;
                app.Exit();
            }),
            Button("Quit", [&] {
                open = false;
                app.Exit();
            }),
        });
        auto controls = Container::Vertical({menu, buttons});

        auto root = Renderer(controls, [&] {
            return vbox({
                       text(options_.title) | bold | center,
                       text("Select an application") | center | dim,
                       separator(),
                       window(text(" Applications "), menu->Render()) | flex,
                       separator(),
                       buttons->Render() | center,
                       text("Enter: open  |  q/Esc: quit") | center | dim,
                   }) |
                   border;
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
        if (!open || selected < 0 ||
            static_cast<std::size_t>(selected) >= names_.size())
        {
            return std::nullopt;
        }
        return static_cast<std::size_t>(selected);
    }

    launcher_options options_;
    std::tuple<Apps...> applications_;
    std::vector<std::string> names_;
};

} // namespace detail

template<class... Apps>
    requires (sizeof...(Apps) > 0) && (std::copy_constructible<Apps> && ...)
void run_launcher(launcher_options options, Apps... applications)
{
    detail::launcher_frontend<Apps...>{
        std::move(options),
        std::move(applications)...}
        .run();
}

} // namespace easylocal::tui
