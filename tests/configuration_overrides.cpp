#include <easylocal/core/aggregation.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/helpers/neighborhood_union.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <string_view>

namespace
{

using easylocal::NeighborhoodUnionParameters;
using easylocal::config::apply_overrides;
using easylocal::config::override_error;
using easylocal::config::text_override;
using easylocal::runners::SimulatedAnnealing;
using easylocal::runners::temperature::FixedLength;
using easylocal::runners::temperature::FixedLengthParameters;

struct AppParameters
{
    std::filesystem::path instance_file{"initial.dat"};
    std::uint64_t seed{17U};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<
                "instance_file",
                &AppParameters::instance_file>(),
            easylocal::config::field<"seed", &AppParameters::seed>());
    }

    [[nodiscard]]
    auto validate() const noexcept -> easylocal::config::validation_result
    {
        return instance_file.empty()
            ? easylocal::config::validation_result::failure(
                  "instance_file must not be empty")
            : easylocal::config::validation_result::success();
    }
};

void text_overrides_apply_to_multiple_typed_blocks()
{
    AppParameters app{};
    FixedLength temperature{FixedLengthParameters{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    }};
    NeighborhoodUnionParameters<2> neighborhood{
        .random_biases = {1.0, 1.0},
    };

    const auto tree = easylocal::config::root(
        easylocal::config::named<"application">(app),
        easylocal::config::named<"solver">(
            temperature.configuration(),
            easylocal::config::named<"neighborhood">(neighborhood)));

    constexpr std::array overrides{
        text_override{"application.instance_file", "sample.tsp"},
        text_override{"application.seed", "2026"},
        text_override{"solver.temperature.initial_temperature", "4.0"},
        text_override{"solver.temperature.final_temperature", "0.05"},
        text_override{"solver.temperature.cooling_rate", "0.8"},
        text_override{"solver.temperature.max_iterations", "500"},
        text_override{"solver.neighborhood.random_biases", "[3, 1]"},
    };

    const auto result = apply_overrides(tree, overrides);
    assert(result);
    assert(result.applied_parameter_blocks == 3);
    assert(app.instance_file == std::filesystem::path{"sample.tsp"});
    assert(app.seed == 2026U);
    assert(temperature.parameters().initial_temperature == 4.0);
    assert(temperature.parameters().final_temperature == 0.05);
    assert(temperature.parameters().cooling_rate == 0.8);
    assert(temperature.parameters().max_iterations == 500);
    assert((neighborhood.random_biases == std::array{3.0, 1.0}));
}

void cross_field_overrides_are_validated_as_one_block()
{
    FixedLength temperature{FixedLengthParameters{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    }};

    const auto tree = easylocal::config::root(
        easylocal::config::named<"solver">(temperature.configuration()));

    constexpr std::array overrides{
        text_override{"solver.temperature.initial_temperature", "0.10"},
        text_override{"solver.temperature.final_temperature", "0.05"},
    };

    const auto result = apply_overrides(tree, overrides);
    assert(result);
    assert(result.applied_parameter_blocks == 1);
    assert(temperature.parameters().initial_temperature == 0.10);
    assert(temperature.parameters().final_temperature == 0.05);
}

void invalid_batch_is_globally_atomic()
{
    AppParameters app{};
    FixedLength temperature{FixedLengthParameters{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    }};

    const auto tree = easylocal::config::root(
        easylocal::config::named<"application">(app),
        easylocal::config::named<"solver">(temperature.configuration()));

    constexpr std::array overrides{
        text_override{"application.seed", "99"},
        text_override{"solver.temperature.cooling_rate", "1.5"},
    };

    const auto result = apply_overrides(tree, overrides);
    assert(!result);
    assert(result.applied_parameter_blocks == 0);
    assert(result.diagnostics.size() == 1);
    assert(result.diagnostics.front().error == override_error::validation_error);
    assert(result.diagnostics.front().path == "solver.temperature");
    assert(app.seed == 17U);
    assert(temperature.parameters().cooling_rate == 0.75);
}

void parse_unknown_and_duplicate_errors_are_reported_without_commit()
{
    AppParameters app{};
    FixedLength temperature{FixedLengthParameters{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    }};

    const auto tree = easylocal::config::root(
        easylocal::config::named<"application">(app),
        easylocal::config::named<"solver">(temperature.configuration()));

    constexpr std::array overrides{
        text_override{"application.seed", "not-an-integer"},
        text_override{"application.seed", "42"},
        text_override{"solver.temperature.unknown", "10"},
    };

    const auto result = apply_overrides(tree, overrides);
    assert(!result);
    assert(result.applied_parameter_blocks == 0);

    bool saw_duplicate = false;
    bool saw_parse = false;
    bool saw_unknown = false;
    for (const auto& diagnostic : result.diagnostics)
    {
        saw_duplicate |= diagnostic.error == override_error::duplicate_path;
        saw_parse |= diagnostic.error == override_error::parse_error;
        saw_unknown |= diagnostic.error == override_error::unknown_parameter;
    }

    assert(saw_duplicate);
    assert(saw_parse);
    assert(saw_unknown);
    assert(app.seed == 17U);
}

void unbracketed_fixed_arrays_are_supported()
{
    NeighborhoodUnionParameters<2> neighborhood{};
    const auto tree = easylocal::config::root(
        easylocal::config::named<"neighborhood">(neighborhood));

    constexpr std::array overrides{
        text_override{"neighborhood.random_biases", "2.5, 0"},
    };

    const auto result = apply_overrides(tree, overrides);
    assert(result);
    assert((neighborhood.random_biases == std::array{2.5, 0.0}));
}



void parameter_group_local_values_can_be_overridden()
{
    NeighborhoodUnionParameters<2> neighborhood{
        .random_biases = {1.0, 1.0},
    };
    FixedLengthParameters temperature{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    };

    const auto tree = easylocal::config::root(
        easylocal::config::named<"solver">(
            neighborhood,
            easylocal::config::named<"temperature">(temperature)));

    constexpr std::array overrides{
        text_override{"solver.random_biases", "[4, 1]"},
    };

    const auto result = apply_overrides(tree, overrides);
    assert(result);
    assert(result.applied_parameter_blocks == 1);
    assert((neighborhood.random_biases == std::array{4.0, 1.0}));
    assert(temperature.initial_temperature == 8.0);
}

void diagnostics_accumulate_across_independent_failures()
{
    AppParameters app{};
    FixedLength temperature{FixedLengthParameters{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    }};
    NeighborhoodUnionParameters<2> neighborhood{
        .random_biases = {1.0, 1.0},
    };

    const auto tree = easylocal::config::root(
        easylocal::config::named<"application">(app),
        easylocal::config::named<"solver">(
            temperature.configuration(),
            easylocal::config::named<"neighborhood">(neighborhood)));

    constexpr std::array overrides{
        text_override{"application.seed", "not-an-integer"},
        text_override{"solver.temperature.cooling_rate", "1.5"},
        text_override{"solver.neighborhood.random_biases", "[1, -2]"},
        text_override{"solver.unknown", "17"},
    };

    const auto result = apply_overrides(tree, overrides);
    assert(!result);
    assert(result.applied_parameter_blocks == 0);

    bool saw_parse = false;
    bool saw_temperature_validation = false;
    bool saw_neighborhood_validation = false;
    bool saw_unknown = false;

    for (const auto& diagnostic : result.diagnostics)
    {
        if (diagnostic.error == override_error::parse_error &&
            diagnostic.path == "application.seed")
        {
            saw_parse = diagnostic.value == "not-an-integer";
        }
        else if (diagnostic.error == override_error::validation_error &&
                 diagnostic.path == "solver.temperature")
        {
            saw_temperature_validation = true;
            assert(diagnostic.value.empty());
        }
        else if (diagnostic.error == override_error::validation_error &&
                 diagnostic.path == "solver.neighborhood")
        {
            saw_neighborhood_validation = true;
            assert(diagnostic.value.empty());
        }
        else if (diagnostic.error == override_error::unknown_parameter &&
                 diagnostic.path == "solver.unknown")
        {
            saw_unknown = diagnostic.value == "17";
        }
    }

    assert(saw_parse);
    assert(saw_temperature_validation);
    assert(saw_neighborhood_validation);
    assert(saw_unknown);

    assert(app.seed == 17U);
    assert(temperature.parameters().cooling_rate == 0.75);
    assert((neighborhood.random_biases == std::array{1.0, 1.0}));
}


void built_in_aggregator_weights_are_runtime_configurable()
{
    auto aggregate = easylocal::aggregation::weighted_sum{1, 10, 100};
    const auto tree = easylocal::config::root(aggregate.configuration());

    constexpr std::array overrides{
        text_override{"cost.weights", "[2, 20, 200]"},
    };

    const auto result = apply_overrides(tree, overrides);
    assert(result);
    assert(result.applied_parameter_blocks == 1);
    assert((aggregate.parameters().weights == std::array{2, 20, 200}));
    assert(aggregate(1, 1, 1) == 222);
}

void const_parameter_nodes_are_reported_as_read_only()
{
    const AppParameters app{};
    const auto tree = easylocal::config::root(
        easylocal::config::named<"application">(app));

    constexpr std::array overrides{
        text_override{"application.seed", "99"},
    };

    const auto result = apply_overrides(tree, overrides);
    assert(!result);
    assert(result.applied_parameter_blocks == 0);
    assert(result.diagnostics.size() == 1);
    assert(result.diagnostics.front().error ==
           override_error::read_only_parameter);
    assert(result.diagnostics.front().value == "99");
    assert(app.seed == 17U);
}

} // namespace

int main()
{
    text_overrides_apply_to_multiple_typed_blocks();
    cross_field_overrides_are_validated_as_one_block();
    invalid_batch_is_globally_atomic();
    parse_unknown_and_duplicate_errors_are_reported_without_commit();
    unbracketed_fixed_arrays_are_supported();
    parameter_group_local_values_can_be_overridden();
    diagnostics_accumulate_across_independent_failures();
    built_in_aggregator_weights_are_runtime_configurable();
    const_parameter_nodes_are_reported_as_read_only();
}
