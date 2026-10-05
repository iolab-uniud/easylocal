// A cost::apply function with parameters is built from them, with
// cost::apply<F>(parameters, children...), not given as an object.
#include "service_composition_fixture.hpp"

#include <easylocal/config/parameters.hpp>
#include <easylocal/cost.hpp>

#include <string_view>

struct OffsetParameters
{
    int offset{0};

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"offset", &OffsetParameters::offset>(
                "Offset",
                easylocal::config::range(0, 10)));
    }

    [[nodiscard]] easylocal::config::validation_result validate() const
    {
        return easylocal::config::check_schema(*this);
    }
};

class OffsetFunction
{
public:
    using parameters_type = OffsetParameters;

    explicit OffsetFunction(const OffsetParameters& parameters)
        : offset_{parameters.offset}
    {
    }

    [[nodiscard]] static std::string_view name() noexcept
    {
        return "offset";
    }

    [[nodiscard]] int operator()(const int value) const noexcept
    {
        return value + offset_;
    }

private:
    int offset_;
};

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::component;

    [[maybe_unused]] auto expression = easylocal::cost::apply(
        OffsetFunction{OffsetParameters{}},
        component<ComponentA>());
}
