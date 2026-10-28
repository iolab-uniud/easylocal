#include <easylocal/config/parameters.hpp>

#include <cstddef>

struct Parameters
{
    easylocal::config::parameter<std::size_t, "duplicate"> first{1};
    easylocal::config::parameter<std::size_t, "duplicate"> second{2};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<&Parameters::first>(),
            easylocal::config::field<&Parameters::second>());
    }

    [[nodiscard]]
    constexpr easylocal::config::validation_result validate() const noexcept
    {
        return easylocal::config::validation_result::success();
    }
};

int main()
{
    Parameters parameters{};
    easylocal::config::for_each_parameter(
        parameters,
        [](const auto, const auto&) {});
}
