#include <easylocal/easylocal.hpp>
#include <easylocal/testing/check.hpp>
#include <easylocal/testing/cost_component.hpp>
#include <easylocal/testing/delta_cost_component.hpp>
#include <easylocal/testing/neighborhood.hpp>
#include <easylocal/testing/solution_manager.hpp>

#if EASYLOCAL_TEST_CONFIG_TOML
#include <easylocal/adapters/toml.hpp>
#endif

#if EASYLOCAL_TEST_TUI
#include <easylocal/adapters/tui/tester.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#endif

#if EASYLOCAL_TEST_REST
#include <easylocal/adapters/rest.hpp>

#include <crow.h>
#endif

#include <string>

#if __cplusplus < 202100L
#error "EasyLocal::Core must propagate a C++23 compile requirement"
#endif

int main()
{
#if EASYLOCAL_TEST_CONFIG_TOML
    const auto parsed = easylocal::config::parse_toml_text(
        "[solver]\niterations = 42\n");
    if (!parsed || parsed.overrides.size() != 1 ||
        parsed.overrides.front().path != "solver.iterations")
    {
        return 1;
    }
#endif
#if EASYLOCAL_TEST_TUI
    // FTXUI's three libraries linked: a component (component) rendered as an
    // element (dom) on a screen (screen).
    auto component = ftxui::Renderer([] { return ftxui::text("ok"); });
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(8), ftxui::Dimension::Fixed(1));
    ftxui::Render(screen, component->Render());
    if (screen.ToString().find("ok") == std::string::npos)
        return 1;
#endif
#if EASYLOCAL_TEST_REST
    // Crow (and Asio) linked: an app with a route, which handles a request.
    crow::SimpleApp server;
    server.loglevel(crow::LogLevel::Warning);
    CROW_ROUTE(server, "/")([] { return "ok"; });
    server.validate();
    crow::request request;
    request.url = "/";
    request.raw_url = "/";
    crow::response response;
    server.handle_full(request, response);
    if (response.code != 200 || response.body != "ok")
        return 1;
#endif
    return 0;
}
