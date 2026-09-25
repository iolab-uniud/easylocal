#include <easylocal/config/tree.hpp>

#include <cstddef>

namespace
{

struct Parameters
{
    std::size_t value{1};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"value", &Parameters::value>());
    }

    [[nodiscard]]
    constexpr auto validate() const noexcept
        -> easylocal::config::validation_result
    {
        return easylocal::config::validation_result::success();
    }
};

} // namespace

int main()
{
    Parameters first{};
    Parameters second{};

    const auto tree = easylocal::config::root(
        easylocal::config::named<"duplicate">(first),
        easylocal::config::named<"duplicate">(second));

    (void)tree;
}
