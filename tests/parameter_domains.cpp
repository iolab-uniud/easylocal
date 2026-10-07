// Domains of parameters: config::range and config::one_of in a schema, checked
// by check_schema and by the validation of a parameter_set, and listed with
// the parameters.
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/config/setup.hpp>
#include <easylocal/runners/great_deluge.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/utils/limit.hpp>

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace
{

namespace config = easylocal::config;

struct TunedParameters
{
    double rate{0.5};
    std::size_t samples{10};
    std::string policy{"tabu"};
    std::vector<double> weights{1.0, 2.0};
    easylocal::limit budget{100};
    double free{3.0};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"rate", &TunedParameters::rate>(
                "A rate",
                config::range(0.0, 1.0).open_low()),
            config::field<"samples", &TunedParameters::samples>(
                "A count",
                config::range(1, 1000).log()),
            config::field<"policy", &TunedParameters::policy>(
                "A policy",
                config::one_of("tabu", "random")),
            config::field<"weights", &TunedParameters::weights>(
                "Weights",
                config::range(0.0, 10.0)),
            config::field<"budget", &TunedParameters::budget>(
                "A budget",
                config::range(1, 1000)),
            config::field<"free", &TunedParameters::free>("No domain"));
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        if (const auto schema = config::check_schema(*this); !schema)
            return schema;
        return config::validation_result::success();
    }
};

// Domains are values of a constant expression, and so are their checks.
static_assert(config::domain_contains(config::range(0.0, 1.0).open(), 0.5));
static_assert(!config::domain_contains(config::range(0.0, 1.0).open(), 0.0));
static_assert(!config::domain_contains(config::range(0.0, 1.0).open(), 1.0));
static_assert(config::domain_contains(config::range(0.0, 1.0).open_low(), 1.0));
static_assert(config::domain_contains(config::range(0.0, 1.0), 0.0));
static_assert(config::domain_contains(config::range(1, 10), std::size_t{10}));
static_assert(!config::domain_contains(config::range(1, 10), std::size_t{0}));
static_assert(config::domain_contains(config::one_of(10, 100), 100));
// Numbers of different types share their common type: 1.5 stays 1.5.
static_assert(std::same_as<decltype(config::one_of(1, 1.5, 2))::value_type, double>);
static_assert(config::domain_contains(config::one_of(1, 1.5, 2), 1.5));
static_assert(!config::domain_contains(config::one_of(1, 1.5, 2), 1.0 + 0.25));

void check_domains_names_the_first_field_outside()
{
    TunedParameters parameters;
    parameters.rate = 0.0;
    const auto result = config::check_schema(parameters);
    assert(!result);
    assert(result.message == "rate: expected a value in (0, 1], got 0");

    parameters = {};
    parameters.policy = "greedy";
    assert(
        config::check_schema(parameters).message
        == "policy: expected a value in {tabu, random}, got greedy");

    parameters = {};
    parameters.weights = {1.0, 11.0};
    assert(!config::check_schema(parameters));

    parameters = {};
    parameters.budget = easylocal::unlimited;
    assert(!config::check_schema(parameters));

    parameters = {};
    parameters.rate = std::numeric_limits<double>::quiet_NaN();
    assert(!config::check_schema(parameters));

    // The reason is owned: it may be built at run time.
    std::string reason{"a reason built at run time, longer than a short string"};
    const auto failure = config::validation_result::failure(reason);
    reason.clear();
    assert(failure.message == "a reason built at run time, longer than a short string");
}

void parameter_sets_check_domains_with_the_field_path()
{
    TunedParameters parameters;
    config::parameter_set set;
    set.add("tuned", parameters);

    const std::array overrides{config::text_override{"tuned.rate", "0"}};
    const auto applied = set.apply(overrides);
    assert(!applied);
    assert(applied.diagnostics.size() == 1);
    assert(applied.diagnostics.front().path == "tuned.rate");
    assert(applied.diagnostics.front().message == "expected a value in (0, 1], got 0");
    assert(parameters.rate == 0.5);

    // A block made in the code, not read from text, is checked too, and its
    // validate() is not asked again about the same field.
    parameters.samples = 0;
    const auto validation = set.validate();
    assert(validation.diagnostics.size() == 1);
    assert(validation.diagnostics.front().path == "tuned.samples");
    assert(
        validation.diagnostics.front().message
        == "expected a value in [1, 1000] log, got 0");
}

void parameters_list_their_kind_and_domain()
{
    TunedParameters parameters;
    config::parameter_set set;
    set.add("tuned", parameters);
    const auto listed = set.parameters();
    assert(listed.size() == 6);

    assert(listed[0].kind == config::parameter_kind::real);
    assert(listed[0].domain.text() == "(0, 1]");
    assert(listed[1].kind == config::parameter_kind::integer);
    assert(listed[1].domain.logarithmic);
    assert(listed[2].kind == config::parameter_kind::text);
    assert(listed[2].domain.text() == "{tabu, random}");
    assert(listed[3].kind == config::parameter_kind::list);
    assert(listed[4].kind == config::parameter_kind::limit);
    assert(listed[5].kind == config::parameter_kind::real);
    assert(!listed[5].domain);
    assert(listed[5].domain.text().empty());
}

void domains_contain_narrower_domains()
{
    const auto unit = config::describe_domain(config::range(0.0, 1.0).open());
    assert(unit.contains(config::describe_domain(config::range(0.5, 0.99))));
    assert(!unit.contains(config::describe_domain(config::range(0.0, 0.5))));
    assert(unit.contains(config::describe_domain(config::range(0.0, 0.5).open_low())));
    assert(unit.contains(config::describe_domain(config::one_of(0.25, 0.5))));
    assert(!unit.contains(config::domain_info{}));
    assert(config::domain_info{}.contains(unit));

    const auto policies = config::describe_domain(config::one_of("tabu", "random"));
    assert(policies.contains(config::describe_domain(config::one_of("tabu"))));
    assert(!policies.contains(config::describe_domain(config::one_of("greedy"))));
}

// A default outside its domain may be repaired by the command line, as an
// invalid block may.
void an_invalid_default_can_be_overridden()
{
    TunedParameters parameters;
    parameters.rate = 0.0;
    config::parameter_set set;
    set.add("tuned", parameters);

    char program[] = "program";
    char samples[] = "--tuned.samples=5";
    char rate[] = "--tuned.rate=0.25";
    char* invalid[]{program, samples};
    assert(!config::load_and_apply(2, invalid, set));
    char* repaired[]{program, rate};
    assert(config::load_and_apply(2, repaired, set));
    assert(parameters.rate == 0.25);
}

// A range may have no upper bound, and easylocal::unlimited alone is any value.
struct OpenParameters
{
    double positive{1.0};
    easylocal::limit budget{easylocal::unlimited};
    int offset{-3};
    double timeout{std::numeric_limits<double>::infinity()};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"positive", &OpenParameters::positive>(
                "Positive",
                config::range(0.0, easylocal::unlimited).open_low()),
            config::field<"budget", &OpenParameters::budget>(
                "Budget",
                config::range(1, easylocal::unlimited)),
            config::field<"offset", &OpenParameters::offset>(
                "Offset",
                easylocal::unlimited),
            config::field<"timeout", &OpenParameters::timeout>(
                "Timeout",
                config::range(0.0, easylocal::unlimited)));
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

void ranges_may_be_unlimited()
{
    OpenParameters parameters;
    assert(parameters.validate()); // unlimited budget, infinite timeout
    parameters.positive = 0.0;
    assert(!parameters.validate());
    parameters = {};
    parameters.budget = 0;
    assert(!parameters.validate());

    config::parameter_set set;
    set.add("open", parameters);
    const auto listed = set.parameters();
    assert(listed[0].domain.text() == "(0, unlimited]");
    assert(listed[1].domain.text() == "[1, unlimited]");
    assert(listed[2].domain.text() == "unlimited");
    assert(listed[2].domain.kind == config::domain_info::shape::unbounded);

    // open_high() leaves out the upper end: infinity, or unlimited.
    constexpr auto finite = config::range(0.0, easylocal::unlimited).open_high();
    static_assert(config::domain_contains(finite, 1e300));
    static_assert(
        !config::domain_contains(finite, std::numeric_limits<double>::infinity()));
    static_assert(config::domain_contains(
        config::range(0.0, easylocal::unlimited),
        std::numeric_limits<double>::infinity()));
    constexpr auto counted = config::range(1, easylocal::unlimited).open_high();
    static_assert(config::domain_contains(counted, easylocal::limit{5}));
    static_assert(
        !config::domain_contains(counted, easylocal::limit{easylocal::unlimited}));
    assert(config::describe_domain(counted).text() == "[1, unlimited)");
    assert(!config::describe_domain(counted).contains(
        config::describe_domain(config::range(1, easylocal::unlimited))));
    assert(
        config::describe_domain(config::range(1, easylocal::unlimited))
            .contains(config::describe_domain(counted)));
    assert(config::undeclared_domains(set).empty());

    TunedParameters tuned;
    config::parameter_set with_free;
    with_free.add("tuned", tuned);
    assert(
        config::undeclared_domains(with_free) == std::vector<std::string>{"tuned.free"});

    // Every range lies within unlimited, and only an unlimited range within one.
    const auto any = config::describe_domain(config::unbounded_domain{});
    assert(any.contains(listed[0].domain));
    assert(listed[0].domain.contains(config::describe_domain(config::range(1.0, 2.0))));
    assert(!config::describe_domain(config::range(0.0, 10.0)).contains(listed[0].domain));
}

// The built-in runners declare the domains their validate() checks.
void built_in_validate_checks_the_declared_domains()
{
    using easylocal::runners::GreatDelugeParameters;
    using easylocal::runners::temperature::ClassicParameters;
    using easylocal::runners::temperature::CutoffParameters;

    ClassicParameters classic;
    classic.cooling_rate = 1.0;
    assert(
        classic.validate().message == "cooling_rate: expected a value in (0, 1), got 1");
    classic = {};
    classic.initial_acceptance = 0.0;
    assert(classic.validate()); // without calibration it does not matter
    classic.calibration_samples = 10;
    assert(!classic.validate());

    CutoffParameters cutoff;
    cutoff.accepted_ratio = 1.0;
    assert(cutoff.validate());
    cutoff.accepted_ratio = 0.0;
    assert(!cutoff.validate());

    GreatDelugeParameters deluge;
    deluge.level_rate = 1.0;
    assert(deluge.validate().message == "level_rate: expected a value in (0, 1), got 1");
}

} // namespace

int main()
{
    check_domains_names_the_first_field_outside();
    parameter_sets_check_domains_with_the_field_path();
    parameters_list_their_kind_and_domain();
    domains_contain_narrower_domains();
    an_invalid_default_can_be_overridden();
    ranges_may_be_unlimited();
    built_in_validate_checks_the_declared_domains();
}
