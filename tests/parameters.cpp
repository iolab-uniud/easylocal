#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameters.hpp>
#include <easylocal/helpers/neighborhood_union.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/utils/limit.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace
{

using easylocal::NeighborhoodUnionParameters;
using easylocal::config::for_each_parameter;
using easylocal::config::parameter_block;
using easylocal::runners::FirstImprovementParameters;
using easylocal::runners::temperature::FixedLengthParameters;

struct AppParameters
{
    std::filesystem::path instance_file{"instance.dat"};
    std::uint64_t seed{2026U};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<
                "instance_file",
                &AppParameters::instance_file>("Problem instance file"),
            easylocal::config::field<
                "seed",
                &AppParameters::seed>("Pseudo-random generator seed"));
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

static_assert(parameter_block<AppParameters>);
static_assert(parameter_block<FirstImprovementParameters>);
static_assert(parameter_block<FixedLengthParameters>);
static_assert(parameter_block<NeighborhoodUnionParameters<2>>);

void app_parameter_schema_is_internal_and_typed()
{
    using schema_type = decltype(AppParameters::parameter_schema());
    using instance_descriptor = std::tuple_element_t<0, schema_type>;
    using seed_descriptor = std::tuple_element_t<1, schema_type>;

    static_assert(std::same_as<
        instance_descriptor::value_type,
        std::filesystem::path>);
    static_assert(std::same_as<
        seed_descriptor::value_type,
        std::uint64_t>);
    static_assert(instance_descriptor::name() == "instance_file");
    static_assert(seed_descriptor::name() == "seed");

    AppParameters parameters{};
    assert(parameters.validate());

    parameters.instance_file.clear();
    const auto invalid = parameters.validate();
    assert(!invalid);
    assert(invalid.message == "instance_file must not be empty");
}

void iteration_exposes_names_descriptions_and_typed_references()
{
    FixedLengthParameters parameters{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    };

    std::array<std::string_view, 6> names{};
    std::size_t index = 0;

    for_each_parameter(
        parameters,
        [&](const auto descriptor, auto& value) {
            using descriptor_type = decltype(descriptor);
            names[index++] = descriptor_type::name();
            assert(!descriptor.description.empty());

            if constexpr (descriptor_type::name() == "max_iterations")
            {
                static_assert(std::same_as<
                    std::remove_cvref_t<decltype(value)>,
                    std::size_t>);
                value = 250;
            }
        });

    assert((
        names
        == std::array<std::string_view, 6>{
            "initial_temperature",
            "final_temperature",
            "cooling_rate",
            "max_iterations",
            "calibration_samples",
            "initial_acceptance"}));
    assert(parameters.max_iterations == 250);

    const auto& const_parameters = parameters;
    for_each_parameter(
        const_parameters,
        [](const auto, const auto& value) {
            static_assert(std::is_const_v<
                std::remove_reference_t<decltype(value)>>);
        });
}

void fixed_length_validation_checks_cross_field_invariants()
{
    const FixedLengthParameters valid{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    };
    assert(valid.validate());

    auto invalid = valid;
    invalid.final_temperature = invalid.initial_temperature;
    assert(!invalid.validate());

    invalid = valid;
    invalid.cooling_rate = 1.0;
    assert(!invalid.validate());

    invalid = valid;
    invalid.max_iterations = 0;
    assert(!invalid.validate());

    invalid = valid;
    invalid.initial_temperature =
        std::numeric_limits<double>::infinity();
    assert(!invalid.validate());
}

void first_improvement_parameters_live_with_the_search_method()
{
    using schema_type = decltype(FirstImprovementParameters::parameter_schema());
    using descriptor_type = std::tuple_element_t<0, schema_type>;

    static_assert(descriptor_type::name() == "max_evaluations");
    static_assert(std::same_as<descriptor_type::value_type, easylocal::limit>);

    FirstImprovementParameters parameters{.max_evaluations = 100};
    assert(parameters.validate());
    assert(parameters.max_evaluations == 100);
    // No budget, the default: until a local optimum.
    assert(FirstImprovementParameters{}.max_evaluations.is_unlimited());
    parameters.max_evaluations = easylocal::unlimited;
    assert(parameters.validate());
    // Zero is a budget of zero, not "no budget".
    parameters.max_evaluations = 0;
    assert(!parameters.max_evaluations.is_unlimited());
}

void a_limit_is_a_count_or_unlimited_in_text()
{
    namespace config = easylocal::config;
    easylocal::limit value = 5;
    assert(config::detail::parse_text_value("unlimited", value).empty());
    assert(value.is_unlimited());
    assert(config::format_value(value) == "unlimited");
    assert(config::detail::parse_text_value(" 25 ", value).empty());
    assert(value == 25);
    assert(config::format_value(value) == "25");
    assert(config::detail::parse_text_value("0", value).empty());
    assert(value == 0);
    assert(!config::detail::parse_text_value("infinite", value).empty());
    assert(value == 0); // unchanged by an invalid text
}

void every_temperature_policy_has_a_parameter_block()
{
    namespace temperature = easylocal::runners::temperature;
    static_assert(easylocal::config::parameter_block<temperature::ClassicParameters>);
    static_assert(easylocal::config::parameter_block<temperature::FixedLengthParameters>);
    static_assert(easylocal::config::parameter_block<temperature::CutoffParameters>);
    static_assert(easylocal::config::parameter_block<temperature::HybridParameters>);

    // A policy is rebuilt from new parameters: its derived state follows them.
    temperature::ClassicParameters parameters{
        .initial_temperature = 20.0,
        .final_temperature = 1.0,
        .cooling_rate = 0.5,
        .samples_per_temperature = 3,
    };
    assert(parameters.validate());
    const temperature::Classic classic{parameters};
    assert(classic.parameters().samples_per_temperature == 3);
    assert(classic.temperature() == 20.0);

    parameters.samples_per_temperature = 0;
    assert(!parameters.validate());

    const temperature::CutoffParameters cutoff{
        .initial_temperature = 10.0,
        .final_temperature = 1.0,
        .cooling_rate = 0.5,
        .max_iterations = 100,
        .accepted_ratio = 1.5,
    };
    assert(!cutoff.validate());
}

void neighborhood_union_parameter_block_describes_bias_array()
{
    using parameters_type = NeighborhoodUnionParameters<2>;
    using schema_type = decltype(parameters_type::parameter_schema());
    using descriptor_type = std::tuple_element_t<0, schema_type>;

    static_assert(descriptor_type::name() == "random_biases");
    static_assert(std::same_as<
        descriptor_type::value_type,
        std::array<double, 2>>);

    parameters_type parameters{};
    assert((parameters.random_biases == std::array{1.0, 1.0}));
    assert(parameters.validate());

    parameters.random_biases = {0.0, 0.0};
    assert(parameters.validate());

    parameters.random_biases = {-1.0, 1.0};
    assert(!parameters.validate());

    parameters.random_biases = {
        std::numeric_limits<double>::quiet_NaN(),
        1.0,
    };
    assert(!parameters.validate());
}

} // namespace

int main()
{
    every_temperature_policy_has_a_parameter_block();
    app_parameter_schema_is_internal_and_typed();
    iteration_exposes_names_descriptions_and_typed_references();
    fixed_length_validation_checks_cross_field_invariants();
    first_improvement_parameters_live_with_the_search_method();
    a_limit_is_a_count_or_unlimited_in_text();
    neighborhood_union_parameter_block_describes_bias_array();
}
