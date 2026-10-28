#include <easylocal/config/parameters.hpp>

#include <cstddef>

struct Parameters
{
    easylocal::config::parameter<std::size_t, "samples"> samples{0};
    easylocal::config::parameter<double, "acceptance"> acceptance{0.5};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<&Parameters::samples>("Samples"),
            easylocal::config::field<&Parameters::acceptance>("Acceptance")
                .only_if(easylocal::config::value<"sample"> > 0));
    }

    [[nodiscard]]
    constexpr easylocal::config::validation_result validate() const noexcept
    {
        return easylocal::config::check_schema(*this);
    }
};

int main()
{
    return Parameters{}.validate() ? 0 : 1;
}
