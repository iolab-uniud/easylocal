#include <easylocal/tui/launcher.hpp>

#include <cassert>
#include <cstddef>
#include <string_view>
#include <tuple>

namespace
{

struct named_app
{
    std::string_view label;

    [[nodiscard]] auto name() const noexcept -> std::string_view
    {
        return label;
    }
};

} // namespace

int main()
{
    auto applications = std::tuple{
        named_app{"first"},
        named_app{"second"},
    };

    const auto names = easylocal::tui::detail::application_names(applications);
    assert(names.size() == 2);
    assert(names[0] == "first");
    assert(names[1] == "second");

    std::string_view visited;
    const bool found = easylocal::tui::detail::visit_application_at(
        applications,
        std::size_t{1},
        [&](const auto& application) {
            visited = application.name();
        });
    assert(found);
    assert(visited == "second");

    const bool missing = easylocal::tui::detail::visit_application_at(
        applications,
        std::size_t{2},
        [](const auto&) {});
    assert(!missing);

    return 0;
}
