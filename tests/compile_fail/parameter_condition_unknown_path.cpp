#include <easylocal/config/parameters.hpp>

#include <cstddef>

struct Parameters
{
    std::size_t samples{0};
    double acceptance{0.5};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"samples", &Parameters::samples>("Samples"),
            easylocal::config::field<"acceptance", &Parameters::acceptance>("Acceptance")
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
