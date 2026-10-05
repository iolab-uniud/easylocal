// config::parameter_set: parameters of several objects, as paths and textual
// values, applied all or none.
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/helpers/neighborhood_union.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{

namespace config = easylocal::config;
using easylocal::NeighborhoodUnionParameters;
using easylocal::runners::temperature::FixedLength;
using easylocal::runners::temperature::FixedLengthParameters;

struct AppParameters
{
    std::filesystem::path instance_file{"instance.dat"};
    std::uint64_t seed{2026U};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"instance_file", &AppParameters::instance_file>("The instance"),
            config::field<"seed", &AppParameters::seed>());
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return instance_file.empty()
            ? config::validation_result::failure("instance_file must not be empty")
            : config::validation_result::success();
    }
};

// A block with a nested group.
struct Schedule
{
    double rate{0.5};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(config::field<"rate", &Schedule::rate>());
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return rate > 0.0 && rate < 1.0
            ? config::validation_result::success()
            : config::validation_result::failure("rate must be in (0, 1)");
    }
};

struct Search
{
    std::size_t budget{100};
    Schedule schedule{};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"budget", &Search::budget>(),
            config::group<"schedule", &Search::schedule>("The schedule"));
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

[[nodiscard]] std::string value_at(
    const config::parameter_set& set,
    std::string_view path)
{
    for (const auto& parameter : set.parameters())
        if (parameter.path == path)
            return parameter.value;
    return "<absent>";
}

void same_typed_blocks_are_told_apart_by_their_prefixes()
{
    AppParameters app{.instance_file = "eil51.tsp", .seed = 17U};
    FixedLengthParameters fast{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .allowed_iterations = 200,
    };
    FixedLengthParameters slow{
        .initial_temperature = 80.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.95,
        .allowed_iterations = 2000,
    };
    NeighborhoodUnionParameters<2> biases{.random_biases = {3.0, 1.0}};

    config::parameter_set fast_set;
    fast_set.add("temperature", fast);
    fast_set.add("neighborhood", biases);

    config::parameter_set set;
    set.add("input", app);
    set.add("fast", fast_set);
    set.add("slow.temperature", slow);

    // Two application fields, the biases, and one set of temperature fields
    // under each prefix.
    std::size_t fast_fields = 0;
    std::size_t slow_fields = 0;
    for (const auto& parameter : set.parameters())
    {
        fast_fields += parameter.path.starts_with("fast.temperature.");
        slow_fields += parameter.path.starts_with("slow.temperature.");
    }
    assert(fast_fields > 0 && fast_fields == slow_fields);
    assert(set.parameters().size() == 2 + 1 + fast_fields + slow_fields);
    assert(value_at(set, "input.instance_file") == "eil51.tsp");
    assert(value_at(set, "fast.temperature.allowed_iterations") == "200");
    assert(value_at(set, "slow.temperature.allowed_iterations") == "2000");
    assert(value_at(set, "fast.neighborhood.random_biases") == "[3, 1]");

    // A view: the set shows what the blocks hold now.
    app.seed = 23U;
    assert(value_at(set, "input.seed") == "23");
}

void nested_groups_have_paths_and_are_validated()
{
    Search search;
    config::parameter_set set;
    set.add("search", search);
    assert(value_at(set, "search.schedule.rate") == "0.5");

    // An invalid nested value rejects every change, valid ones included, and
    // the error names the nested block.
    const std::array invalid{
        config::text_override{"search.budget", "7"},
        config::text_override{"search.schedule.rate", "2"},
    };
    const auto rejected = set.apply(invalid);
    assert(!rejected);
    assert(rejected.diagnostics.size() == 1);
    assert(rejected.diagnostics[0].path == "search.schedule");
    assert(search.budget == 100);

    const std::array valid{
        config::text_override{"search.budget", "7"},
        config::text_override{"search.schedule.rate", "0.25"},
    };
    assert(set.apply(valid));
    assert(search.budget == 7);
    assert(search.schedule.rate == 0.25);
}

void a_block_validates_its_nested_groups()
{
    // The block's own validate() checks its groups, so that a block made in
    // the code is checked whole.
    const Search search{.schedule = {.rate = 2.0}};
    const auto validation = search.validate();
    assert(!validation);
    assert(validation.message == "rate must be in (0, 1)");

    const easylocal::runners::SimulatedAnnealingParameters<FixedLengthParameters>
        annealing{.temperature = {.cooling_rate = 2.0}};
    assert(!annealing.validate());

    // A parameter_set names the group once, not the enclosing block again.
    Search held = search;
    config::parameter_set set;
    set.add("search", held);
    const auto diagnostics = set.validate();
    assert(!diagnostics);
    assert(diagnostics.diagnostics.size() == 1);
    assert(diagnostics.diagnostics[0].path == "search.schedule");
}

void changes_to_several_blocks_are_all_or_none()
{
    AppParameters app;
    Search search;
    config::parameter_set set;
    set.add("input", app);
    set.add("search", search);

    const std::array mixed{
        config::text_override{"input.seed", "5"},
        config::text_override{"search.schedule.rate", "abc"},
    };
    const auto result = set.apply(mixed);
    assert(!result);
    assert(result.diagnostics[0].error == config::override_error::parse_error);
    assert(app.seed == 2026U);
}

// An object that derives something from its parameters, rebuilt by configure().
class Scheduler
{
public:
    [[nodiscard]]
    const Schedule& parameters() const noexcept
    {
        return schedule_;
    }

    [[nodiscard]]
    config::validation_result configure(Schedule schedule)
    {
        if (const auto validation = schedule.validate(); !validation)
            return validation;
        schedule_ = schedule;
        doubled_ = 2.0 * schedule_.rate;
        return config::validation_result::success();
    }

    [[nodiscard]]
    double doubled() const noexcept
    {
        return doubled_;
    }

private:
    Schedule schedule_{};
    double doubled_{1.0};
};

void configurable_objects_rebuild_through_configure()
{
    Scheduler fast;
    Scheduler slow;
    config::parameter_set set;
    set.add("fast", fast);
    set.add("slow", slow);

    const std::array invalid{config::text_override{"fast.rate", "2"}};
    assert(!set.apply(invalid));
    assert(fast.parameters().rate == 0.5);

    const std::array updated{config::text_override{"fast.rate", "0.25"}};
    assert(set.apply(updated));
    assert(fast.parameters().rate == 0.25);
    assert(fast.doubled() == 0.5); // rebuilt
    assert(slow.parameters().rate == 0.5);
}

void const_objects_are_read_only()
{
    const AppParameters app;
    config::parameter_set set;
    set.add("input", app);
    assert(set.parameters()[0].read_only);

    const std::array change{config::text_override{"input.seed", "1"}};
    const auto result = set.apply(change);
    assert(!result);
    assert(result.diagnostics[0].error == config::override_error::read_only_parameter);
}

void unknown_and_repeated_paths_are_errors()
{
    AppParameters app;
    config::parameter_set set;
    set.add("input", app);

    const std::array unknown{config::text_override{"input.sead", "1"}};
    const auto misspelt = set.apply(unknown);
    assert(!misspelt);
    assert(misspelt.diagnostics[0].error == config::override_error::unknown_parameter);

    const std::array twice{
        config::text_override{"input.seed", "1"},
        config::text_override{"input.seed", "2"},
    };
    const auto repeated = set.apply(twice);
    assert(!repeated);
    assert(repeated.diagnostics[0].error == config::override_error::duplicate_path);
    assert(app.seed == 2026U);

    // Each repetition once: three equal paths are two repetitions.
    const std::array thrice{
        config::text_override{"input.seed", "1"},
        config::text_override{"input.seed", "2"},
        config::text_override{"input.seed", "3"},
    };
    const auto repetitions = set.apply(thrice);
    assert(repetitions.diagnostics.size() == 2);
    assert(repetitions.diagnostics[0].value == "2");
    assert(repetitions.diagnostics[1].value == "3");
}

void the_same_path_cannot_be_added_twice()
{
    AppParameters first;
    AppParameters second;
    config::parameter_set set;
    set.add("input", first);
    bool rejected = false;
    try
    {
        set.add("input", second);
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    assert(rejected);
    set.add("other", second);
    assert(set.parameters().size() == 4);
}

void validate_reports_invalid_blocks_by_path()
{
    AppParameters app{.instance_file = {}};
    Search search;
    search.schedule.rate = 3.0;
    config::parameter_set set;
    set.add("input", app);
    set.add("search", search);

    const auto validation = set.validate();
    assert(!validation);
    assert(validation.diagnostics.size() == 2);
    assert(validation.diagnostics[0].path == "input");
    assert(validation.diagnostics[1].path == "search.schedule");
}

} // namespace

int main()
{
    same_typed_blocks_are_told_apart_by_their_prefixes();
    nested_groups_have_paths_and_are_validated();
    a_block_validates_its_nested_groups();
    changes_to_several_blocks_are_all_or_none();
    configurable_objects_rebuild_through_configure();
    const_objects_are_read_only();
    unknown_and_repeated_paths_are_errors();
    the_same_path_cannot_be_added_twice();
    validate_reports_invalid_blocks_by_path();
    return 0;
}
