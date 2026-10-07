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
#include <vector>

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
        .allowed_iterations = 200,
    };

    std::array<std::string_view, 6> names{};
    std::size_t index = 0;

    for_each_parameter(
        parameters,
        [&](const auto descriptor, auto& value) {
            using descriptor_type = decltype(descriptor);
            names[index++] = descriptor_type::name();
            assert(!descriptor.description.empty());

            if constexpr (descriptor_type::name() == "allowed_iterations")
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
            "allowed_iterations",
            "calibration_samples",
            "initial_acceptance"}));
    assert(parameters.allowed_iterations == 250);

    const auto& const_parameters = parameters;
    for_each_parameter(
        const_parameters,
        [](const auto, const auto& value) {
            static_assert(std::is_const_v<
                std::remove_reference_t<decltype(value)>>);
        });

    // A temporary block too.
    std::size_t fields = 0;
    for_each_parameter(decltype(parameters){}, [&](const auto, const auto&) {
        ++fields;
    });
    assert(fields == names.size());
}

void fixed_length_validation_checks_cross_field_invariants()
{
    const FixedLengthParameters valid{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .allowed_iterations = 200,
    };
    assert(valid.validate());

    auto invalid = valid;
    invalid.final_temperature = invalid.initial_temperature;
    assert(!invalid.validate());

    invalid = valid;
    invalid.cooling_rate = 1.0;
    assert(!invalid.validate());

    invalid = valid;
    invalid.allowed_iterations = 0;
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

void a_number_reads_back_from_its_text()
{
    namespace config = easylocal::config;
    const double precise = 0.123456789012345;
    double read = 0.0;
    assert(config::format_value(precise) == "0.123456789012345");
    assert(config::detail::parse_text_value(config::format_value(precise), read).empty());
    assert(read == precise);
    assert(config::format_value(1e-7) == "1e-07");
    assert(config::format_value(std::size_t{1234567}) == "1234567");
}

void values_read_back_from_their_text()
{
    namespace config = easylocal::config;
    const auto round_trip = [](const auto& original) {
        auto read = std::remove_cvref_t<decltype(original)>{};
        return config::detail::parse_text_value(config::format_value(original), read)
                   .empty()
            && read == original;
    };
    assert(round_trip(true));
    assert(round_trip(-42));
    assert(round_trip(std::size_t{7}));
    assert(round_trip(0.1));
    assert(round_trip(std::numeric_limits<double>::infinity()));
    assert(round_trip(easylocal::limit{easylocal::unlimited}));
    assert(round_trip(easylocal::limit{3}));
    assert(round_trip(std::string{"two words"}));
    assert(round_trip(std::filesystem::path{"data/five.tsp"}));
    // A path is UTF-8 text, on Windows too, where the ANSI code page is not.
    const std::filesystem::path accented{u8"dati/citt\u00e0.tsp"};
    assert(round_trip(accented));
    assert(config::format_value(accented) == "dati/citt\xc3\xa0.tsp");
    assert(round_trip(std::vector<std::string>{"a", "b"}));
    // The documented limit: no quoting, so a list element cannot hold a comma.
    assert(!round_trip(std::vector<std::string>{"a, b"}));
}

void a_list_reads_back_from_its_text()
{
    namespace config = easylocal::config;
    std::array<int, 2> pair{};
    assert(!config::detail::parse_text_value("[1, 2,]", pair).empty());
    assert(!config::detail::parse_text_value("[1]", pair).empty());
    assert(config::detail::parse_text_value("[1, 2]", pair).empty());
    assert(
        config::detail::parse_text_value("[1, 2, 3]", pair)
        == "expected 2 elements, got 3");
    assert(config::detail::parse_text_value("[1, x]", pair).starts_with("element 2: "));
    unsigned count{};
    assert(
        config::detail::parse_text_value("-1", count)
            .starts_with("expected a non-negative integer in [0, "));
    assert((pair == std::array{1, 2}));

    std::vector<int> values;
    assert(!config::detail::parse_text_value("[1, 2,]", values).empty());
    assert(config::detail::parse_text_value("[]", values).empty());
    assert(values.empty());

    // Lists of lists, as format_value writes them.
    const std::vector<std::array<int, 2>> nested{{1, 2}, {3, 4}};
    assert(config::format_value(nested) == "[[1, 2], [3, 4]]");
    std::vector<std::array<int, 2>> read;
    assert(config::detail::parse_text_value(config::format_value(nested), read).empty());
    assert(read == nested);
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
        .allowed_iterations = 100,
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
    a_number_reads_back_from_its_text();
    values_read_back_from_their_text();
    a_list_reads_back_from_its_text();
    neighborhood_union_parameter_block_describes_bias_array();
}
