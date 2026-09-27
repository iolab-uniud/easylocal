#pragma once

#include <concepts>
#include <memory>
#include <utility>

namespace easylocal
{

template<class App>
    requires std::move_constructible<App> &&
             requires { typename App::input_type; }
class Tester
{
public:
    using app_type = App;
    using input_type = typename App::input_type;
    using instance_type = decltype(
        std::declval<const App&>().for_input(
            std::declval<const input_type&>()));

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

    [[nodiscard]]
    auto has_input() const noexcept -> bool
    {
        return static_cast<bool>(input_);
    }

    void set_input(input_type input)
    {
        auto new_input = std::make_unique<input_type>(std::move(input));
        auto new_instance = std::unique_ptr<instance_type>{
            new instance_type(app_.for_input(*new_input))};

        instance_.reset();
        input_ = std::move(new_input);
        instance_ = std::move(new_instance);
    }

    [[nodiscard]]
    auto input() const noexcept -> const input_type&
    {
        return *input_;
    }

    [[nodiscard]]
    auto instance() noexcept -> instance_type&
    {
        return *instance_;
    }

    [[nodiscard]]
    auto instance() const noexcept -> const instance_type&
    {
        return *instance_;
    }

private:
    App app_;
    std::unique_ptr<input_type> input_;
    std::unique_ptr<instance_type> instance_;
};

} // namespace easylocal
