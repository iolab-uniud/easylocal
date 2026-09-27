#pragma once

#include <concepts>
#include <utility>

namespace easylocal
{

template<class App>
    requires std::move_constructible<App>
class Tester
{
public:
    explicit Tester(App application)
        : app_{std::move(application)}
    {
    }

    [[nodiscard]]
    auto app() noexcept -> App&
    {
        return app_;
    }

    [[nodiscard]]
    auto app() const noexcept -> const App&
    {
        return app_;
    }

private:
    App app_;
};

} // namespace easylocal
