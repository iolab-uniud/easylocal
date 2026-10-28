#include <easylocal/config/parameter.hpp>
#include <easylocal/utils/limit.hpp>

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

namespace
{

using easylocal::limit;
using easylocal::unlimited;
using easylocal::config::parameter;
using easylocal::config::parameter_not_set;

// A block as a user writes it: a field with a member initializer has that
// default, a field without one is required.
struct Parameters
{
    parameter<double, "cooling_rate"> cooling_rate{0.95};
    parameter<std::size_t, "allowed_iterations"> allowed_iterations;
    parameter<std::string, "report"> report{"none"};
    parameter<limit, "max_evaluations"> max_evaluations{unlimited};
};

static_assert(std::is_aggregate_v<Parameters>);

void a_member_initializer_is_the_default()
{
    const Parameters parameters{};

    assert(parameters.cooling_rate.is_set());
    assert(parameters.cooling_rate.value() == 0.95);
    assert(parameters.report.value() == "none");
    assert(parameters.max_evaluations.value().is_unlimited());
}

void a_field_without_a_member_initializer_is_not_set()
{
    const Parameters parameters{};

    assert(!parameters.allowed_iterations.is_set());
}

void reading_a_parameter_that_is_not_set_throws_and_names_it()
{
    const Parameters parameters{};
    bool thrown = false;
    try
    {
        const std::size_t iterations = parameters.allowed_iterations;
        static_cast<void>(iterations);
    }
    catch (const parameter_not_set& error)
    {
        thrown = true;
        const std::string_view message{error.what()};
        assert(message.find("allowed_iterations") != std::string_view::npos);
    }
    assert(thrown);
}

void a_value_sets_the_parameter()
{
    Parameters parameters{};
    parameters.allowed_iterations = 1000;

    assert(parameters.allowed_iterations.is_set());
    assert(parameters.allowed_iterations.value() == 1000);

    parameters.cooling_rate = 0.5;
    assert(parameters.cooling_rate.value() == 0.5);
}

void a_designated_initializer_leaves_the_other_defaults()
{
    const Parameters parameters{.allowed_iterations = 10};

    assert(parameters.allowed_iterations.value() == 10);
    assert(parameters.cooling_rate.value() == 0.95);
    assert(parameters.report.value() == "none");
}

void a_parameter_reads_like_its_value()
{
    const Parameters parameters{.cooling_rate = 0.5, .allowed_iterations = 4};

    const double temperature = 10.0 * parameters.cooling_rate;
    assert(temperature == 5.0);
    assert(parameters.cooling_rate < 1.0);
    assert(parameters.allowed_iterations == 4U);

    // Where a template deduces its argument, as (std::max)() and the
    // comparison of a std::string with a literal do, the value is spelled out.
    assert((std::max)(0.75, parameters.cooling_rate.value()) == 0.75);
    assert(parameters.report.value() == "none");
}

void a_parameter_is_a_constant_expression()
{
    constexpr parameter<double, "cooling_rate"> rate{0.95};
    static_assert(rate.is_set());
    static_assert(rate.value() == 0.95);
    static_assert(rate.name() == "cooling_rate");

    constexpr parameter<double, "cooling_rate"> unset{};
    static_assert(!unset.is_set());
}

void a_parameter_tells_its_name_and_its_value_type()
{
    using rate_type = decltype(Parameters::cooling_rate);
    static_assert(std::same_as<rate_type::value_type, double>);
    static_assert(rate_type::name() == "cooling_rate");

    namespace detail = easylocal::config::detail;
    static_assert(detail::is_parameter_v<rate_type>);
    static_assert(!detail::is_parameter_v<double>);
    static_assert(std::same_as<detail::parameter_value_t<rate_type>, double>);
    static_assert(std::same_as<detail::parameter_value_t<double>, double>);
}

} // namespace

int main()
{
    a_member_initializer_is_the_default();
    a_field_without_a_member_initializer_is_not_set();
    reading_a_parameter_that_is_not_set_throws_and_names_it();
    a_value_sets_the_parameter();
    a_designated_initializer_leaves_the_other_defaults();
    a_parameter_reads_like_its_value();
    a_parameter_is_a_constant_expression();
    a_parameter_tells_its_name_and_its_value_type();
}
