// A field of an enum type: its values are named once beside the enum, and the
// names are what a configurator reads and writes, and what its domain holds.
#include <easylocal/config/cli.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/config/parameters.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace
{

namespace config = easylocal::config;

enum class Start
{
    random,
    initial,
    greedy,
};

enum class Report
{
    none,
    brief,
};

} // namespace

template<>
struct easylocal::enumeration<Start>
{
    static constexpr std::array values{
        easylocal::enumerator{"random", Start::random},
        easylocal::enumerator{"initial", Start::initial},
        easylocal::enumerator{"greedy", Start::greedy}};
};

template<>
struct easylocal::enumeration<Report>
{
    static constexpr std::array values{
        easylocal::enumerator{"none", Report::none},
        easylocal::enumerator{"brief", Report::brief}};
};

namespace
{

static_assert(easylocal::enumerated<Start>);
static_assert(!easylocal::enumerated<int>);

struct Parameters
{
    easylocal::parameter<Start, "start"> start{Start::random};
    // Only two of the three values: a field may take fewer than the enum
    // declares.
    easylocal::parameter<Start, "restart"> restart{Start::random};
    easylocal::parameter<Report, "report"> report{};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<&Parameters::start>("Where the search starts"),
            config::field<&Parameters::restart>(
                "Where a restart starts",
                config::one_of(Start::random, Start::initial)),
            config::field<&Parameters::report>("What the run reports"));
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

void the_declared_values_are_the_domain()
{
    Parameters parameters{.report = Report::brief};
    config::parameter_set set;
    set.add(parameters);

    const auto listed = set.parameters();
    const auto& start = listed.front();
    assert(start.path == "start");
    assert(start.kind == config::parameter_kind::text);
    assert(start.domain.kind == config::domain_info::shape::choice);
    const std::vector<std::string> every{"random", "initial", "greedy"};
    assert(start.domain.choices == every);
    assert(start.value == "random");

    // A declared domain keeps only the values it gives.
    const auto& restart = listed[1];
    const std::vector<std::string> some{"random", "initial"};
    assert(restart.domain.choices == some);
}

void a_value_is_read_and_written_by_name()
{
    // report has no default: the block is not valid until it is set, and an
    // override is applied only to a block that stays valid.
    Parameters parameters{.report = Report::brief};
    config::parameter_set set;
    set.add(parameters);

    const std::array overrides{config::text_override{"start", "greedy"}};
    assert(set.apply(overrides));
    assert(parameters.start.value() == Start::greedy);
    assert(config::format_value(parameters.start.value()) == "greedy");

    // A name the enum does not declare, and one the field's domain leaves out.
    const std::array unknown{config::text_override{"start", "nearest"}};
    const auto rejected = set.apply(unknown);
    assert(!rejected);
    assert(rejected.diagnostics.front().message.contains("expected one of"));
    assert(parameters.start.value() == Start::greedy);

    const std::array outside{config::text_override{"restart", "greedy"}};
    const auto refused = set.apply(outside);
    assert(!refused);
    assert(refused.diagnostics.front().message.contains("greedy"));
}

void a_value_that_is_not_declared_is_invalid()
{
    Parameters parameters{.report = Report::brief};
    assert(parameters.validate());

    // A value cast from a number is none of the declared ones.
    parameters.start = static_cast<Start>(7);
    const auto validation = parameters.validate();
    assert(!validation);
    assert(validation.message.starts_with("start: expected a value in"));
}

void a_field_that_is_not_set_is_reported_by_name()
{
    const Parameters parameters;
    assert(!parameters.validate());
    assert(parameters.validate().message == "report: not set, and it has no default");

    config::parameter_set set;
    Parameters mutable_parameters;
    set.add(mutable_parameters);
    const auto help = config::cli_help("program", set);
    assert(help.find("values: {random, initial, greedy}") != std::string::npos);
    assert(help.find("required: it has no default and must be set") != std::string::npos);
}

} // namespace

int main()
{
    the_declared_values_are_the_domain();
    a_value_is_read_and_written_by_name();
    a_value_that_is_not_declared_is_invalid();
    a_field_that_is_not_set_is_reported_by_name();
}
