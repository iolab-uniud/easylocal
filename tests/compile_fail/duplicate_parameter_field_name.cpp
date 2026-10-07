#include <easylocal/config/parameters.hpp>

#include <cstddef>

struct Parameters
{
    std::size_t first{1};
    std::size_t second{2};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"duplicate", &Parameters::first>(),
            easylocal::config::field<"duplicate", &Parameters::second>());
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
