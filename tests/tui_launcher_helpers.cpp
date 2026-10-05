#include <easylocal/adapters/tui/launcher.hpp>

#include <cassert>
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

    return 0;
}
