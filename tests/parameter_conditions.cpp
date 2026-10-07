// Conditions and requirements between parameters: expressions over the fields
// of a block (config::value), field(...).only_if(...) and config::require,
// checked by check_schema and by a parameter_set, and listed with full paths.
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/runners/great_deluge.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/utils/limit.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>

namespace
{

namespace config = easylocal::config;

struct Schedule
{
    double start{10.0};
    double end{1.0};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"start", &Schedule::start>("Start"),
            config::field<"end", &Schedule::end>("End"),
            config::require(
                config::value<"end"> < config::value<"start">,
                "end must be below start"));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

struct Search
{
    std::size_t samples{0};
    double acceptance{0.5};
    std::string policy{"fixed"};
    std::size_t tenure{5};
    double reheat{2.0};
    Schedule schedule{};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"samples", &Search::samples>("Samples"),
            config::field<"acceptance", &Search::acceptance>(
                "Acceptance",
                config::range(0.0, 1.0).open())
                .only_if(config::value<"samples"> > 0),
            config::field<"policy", &Search::policy>(
                "Policy",
                config::one_of("fixed", "random")),
            config::field<"tenure", &Search::tenure>("Tenure", config::range(1, 100))
                .only_if(config::value<"policy"> == "fixed"),
            config::field<"reheat", &Search::reheat>("Reheat"),
            config::group<"schedule", &Search::schedule>("Schedule"),
            // A requirement may name the fields of a nested group.
            config::require(
                config::value<"schedule.start">
                        * config::value<"reheat"> > config::value<"schedule.end">,
                "a reheat must stay above the end of the schedule"));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

static_assert(Schedule{}.validate());
static_assert(!Schedule{.start = 1.0, .end = 2.0}.validate());
static_assert(
    config::evaluate(config::value<"start"> - config::value<"end">, Schedule{}) == 9.0);
static_assert(config::evaluate(!(config::value<"end"> >= 5.0), Schedule{}));
static_assert(config::evaluate(
    -config::value<"end"> < 0 && config::value<"start"> / 2 == 5,
    Schedule{}));

// Arithmetic is computed in double, as R computes it: integers divide exactly,
// unlimited is +infinity, and a minus applies after the conversion.
struct Counts
{
    std::size_t count{7};
    easylocal::limit budget{easylocal::unlimited};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"count", &Counts::count>("Count", config::range(0, 100)),
            config::field<"budget", &Counts::budget>(
                "Budget",
                config::range(0, easylocal::unlimited)));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

static_assert(config::evaluate(config::value<"count"> / 2, Counts{}) == 3.5);
static_assert(config::evaluate(-config::value<"count">, Counts{}) == -7.0);
static_assert(config::evaluate(config::value<"budget"> + 1 > 5, Counts{}));
static_assert(config::evaluate(
    config::value<"budget"> - 1 == std::numeric_limits<double>::infinity(),
    Counts{}));
static_assert(config::evaluate(
    config::value<"budget"> > 5,
    Counts{.budget = easylocal::limit{10}}));

void a_field_that_does_not_matter_is_not_checked()
{
    Search search;
    search.acceptance = 2.0; // outside its domain, but samples is 0
    assert(search.validate());
    search.samples = 10;
    assert(search.validate().message == "acceptance: expected a value in (0, 1), got 2");

    search = {};
    search.policy = "random";
    search.tenure = 0;
    assert(search.validate());
}

void requirements_name_their_block_and_reach_into_groups()
{
    Search search;
    search.reheat = 0.05; // 10 * 0.05 <= 1
    assert(
        search.validate().message == "a reheat must stay above the end of the schedule");

    config::parameter_set set;
    set.add("runners.ts", search);
    const auto validation = set.validate();
    assert(validation.diagnostics.size() == 1);
    assert(validation.diagnostics.front().path == "runners.ts");

    search = {};
    const std::array overrides{config::text_override{"runners.ts.schedule.end", "15"}};
    const auto applied = set.apply(overrides);
    assert(!applied);
    assert(applied.diagnostics.front().path == "runners.ts.schedule");
    assert(applied.diagnostics.front().message == "end must be below start");
}

void parameters_list_their_conditions_with_full_paths()
{
    Search search;
    config::parameter_set set;
    set.add("runners.ts", search);
    const auto listed = set.parameters();

    const auto& acceptance = listed[1];
    assert(acceptance.path == "runners.ts.acceptance");
    assert(!acceptance.active);
    assert(acceptance.condition);
    assert(acceptance.condition->to_string() == "(runners.ts.samples > 0)");
    assert(
        acceptance.condition->references()
        == std::vector<std::string>{"runners.ts.samples"});

    const auto& tenure = listed[3];
    assert(tenure.active);
    assert(tenure.condition->to_string() == "(runners.ts.policy == \"fixed\")");
    assert(!listed[0].condition);

    const auto requirements = set.requirements();
    assert(requirements.size() == 2);
    // In the order of the schema: the group comes before the requirement.
    assert(requirements[0].path == "runners.ts.schedule");
    assert(requirements[0].message == "end must be below start");
    assert(requirements[1].path == "runners.ts");
    assert(requirements[1].satisfied);
    assert(
        requirements[1].expression.to_string()
        == "((runners.ts.schedule.start * runners.ts.reheat) > runners.ts.schedule.end)");

    // As irace reads them, with each name written by the caller.
    const auto text = requirements[0].expression.text_with(
        [](const std::string_view path) { return "<" + std::string{path} + ">"; });
    assert(text == "(<runners.ts.schedule.end> < <runners.ts.schedule.start>)");
}

void constants_are_written_as_r_reads_them()
{
    const auto path = [](const std::string_view name) { return std::string{name}; };
    const auto infinite = config::describe_expression(
        config::value<"end"> < std::numeric_limits<double>::infinity(),
        "s");
    assert(infinite.text_with(path) == "(s.end < Inf)");
    const auto undefined = config::describe_expression(
        config::value<"end"> != -std::numeric_limits<double>::infinity()
            || config::value<"end"> == std::numeric_limits<double>::quiet_NaN(),
        "s");
    assert(undefined.text_with(path) == "((s.end != -Inf) || (s.end == NaN))");

    // Quotes and backslashes in a text are escaped.
    const auto quoted = config::describe_expression(
        config::value<"policy"> == std::string_view{"a\"b\\c"},
        "s");
    assert(quoted.text_with(path) == "(s.policy == \"a\\\"b\\\\c\")");
    assert(quoted.to_string() == "(s.policy == \"a\\\"b\\\\c\")");
}

void built_in_schemas_declare_their_relations()
{
    using easylocal::runners::GreatDelugeParameters;
    using easylocal::runners::temperature::ClassicParameters;

    ClassicParameters classic;
    classic.final_temperature = 20.0;
    assert(
        classic.validate().message
        == "final_temperature must be smaller than initial_temperature");
    classic = {};
    classic.initial_acceptance = 1.0; // calibration_samples is 0
    assert(config::check_schema(classic));

    GreatDelugeParameters deluge;
    deluge.min_level = deluge.initial_level;
    assert(deluge.validate().message == "min_level must be smaller than initial_level");
}

// A limit in an expression: unlimited is above every count.
struct Budgeted
{
    easylocal::limit budget{easylocal::unlimited};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"budget", &Budgeted::budget>(
                "Budget",
                config::range(0, easylocal::unlimited)),
            config::require(config::value<"budget"> > 1000, "budget must be above 1000"));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

void unlimited_is_above_every_count()
{
    assert(config::check_schema(Budgeted{}));
    assert(!config::check_schema(Budgeted{.budget = easylocal::limit{10}}));
    assert(config::check_schema(Budgeted{.budget = easylocal::limit{2000}}));
}

} // namespace

int main()
{
    a_field_that_does_not_matter_is_not_checked();
    requirements_name_their_block_and_reach_into_groups();
    parameters_list_their_conditions_with_full_paths();
    constants_are_written_as_r_reads_them();
    built_in_schemas_declare_their_relations();
    unlimited_is_above_every_count();
}
