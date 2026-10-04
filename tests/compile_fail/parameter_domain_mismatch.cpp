#include <easylocal/config/parameters.hpp>

#include <string>

struct Parameters
{
    std::string policy{"tabu"};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"policy", &Parameters::policy>(
                "A policy",
                easylocal::config::range(0, 1)));
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
    easylocal::config::for_each_parameter(parameters, [](const auto, const auto&) {});
}
