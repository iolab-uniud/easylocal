#include <easylocal/config/tree.hpp>
#include <easylocal/helpers/neighborhood_union.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{

using easylocal::NeighborhoodUnionParameters;
using easylocal::config::for_each_config_parameter;
using easylocal::config::at;
using easylocal::config::named;
using easylocal::config::root;
using easylocal::runners::temperature::FixedLength;
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
                &AppParameters::instance_file>(),
            easylocal::config::field<
                "seed",
                &AppParameters::seed>());
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

template<class Path, std::size_t Size>
[[nodiscard]]
consteval auto path_is(const std::array<std::string_view, Size>& expected)
    -> bool
{
    constexpr auto actual = Path::segments();
    if constexpr (actual.size() != Size)
    {
        return false;
    }
    else
    {
        return actual == expected;
    }
}

template<class Node>
concept ConfigurableNode =
    requires(
        const std::remove_cvref_t<Node>& node,
        typename std::remove_cvref_t<Node>::parameters_type parameters)
    {
        node.configure(std::move(parameters));
    };

void tree_distinguishes_same_typed_blocks_by_path()
{
    AppParameters app{
        .instance_file = "eil51.tsp",
        .seed = 17U,
    };

    FixedLengthParameters fast{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    };

    FixedLengthParameters slow{
        .initial_temperature = 80.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.95,
        .max_iterations = 2000,
    };

    NeighborhoodUnionParameters<2> fast_neighborhood{
        .random_biases = {3.0, 1.0},
    };

    const auto tree = root(
        named<"input">(app),
        named<"fast">(
            named<"temperature">(fast),
            named<"neighborhood">(fast_neighborhood)),
        named<"slow">(
            named<"temperature">(slow)));

    std::size_t visited = 0;
    bool saw_input_file = false;
    bool saw_fast_temperature = false;
    bool saw_slow_temperature = false;
    bool saw_fast_biases = false;

    for_each_config_parameter(
        tree,
        [&](const auto path, const auto, const auto& value) {
            using path_type = std::remove_cvref_t<decltype(path)>;
            static_assert(std::is_const_v<
                std::remove_reference_t<decltype(value)>>);

            ++visited;

            if constexpr (path_is<path_type>(
                              std::array<std::string_view, 2>{
                                  "input",
                                  "instance_file"}))
            {
                static_assert(std::same_as<
                    std::remove_cvref_t<decltype(value)>,
                    std::filesystem::path>);
                saw_input_file = value == std::filesystem::path{"eil51.tsp"};
            }
            else if constexpr (path_is<path_type>(
                                   std::array<std::string_view, 3>{
                                       "fast",
                                       "temperature",
                                       "initial_temperature"}))
            {
                static_assert(std::same_as<
                    std::remove_cvref_t<decltype(value)>,
                    double>);
                saw_fast_temperature = value == 8.0;
            }
            else if constexpr (path_is<path_type>(
                                   std::array<std::string_view, 3>{
                                       "slow",
                                       "temperature",
                                       "initial_temperature"}))
            {
                static_assert(std::same_as<
                    std::remove_cvref_t<decltype(value)>,
                    double>);
                saw_slow_temperature = value == 80.0;
            }
            else if constexpr (path_is<path_type>(
                                   std::array<std::string_view, 3>{
                                       "fast",
                                       "neighborhood",
                                       "random_biases"}))
            {
                static_assert(std::same_as<
                    std::remove_cvref_t<decltype(value)>,
                    std::array<double, 2>>);
                saw_fast_biases = value == std::array{3.0, 1.0};
            }
        });

    assert(visited == 15);
    assert(saw_input_file);
    assert(saw_fast_temperature);
    assert(saw_slow_temperature);
    assert(saw_fast_biases);
}

void tree_is_a_non_owning_view_of_parameter_blocks()
{
    FixedLengthParameters fast{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    };

    const auto tree = root(
        named<"fast">(
            named<"temperature">(fast)));

    fast.initial_temperature = 12.0;

    bool saw_updated_value = false;
    for_each_config_parameter(
        tree,
        [&](const auto path, const auto, const auto& value) {
            using path_type = std::remove_cvref_t<decltype(path)>;
            if constexpr (path_is<path_type>(
                              std::array<std::string_view, 3>{
                                  "fast",
                                  "temperature",
                                  "initial_temperature"}))
            {
                saw_updated_value = value == 12.0;
            }
        });

    assert(saw_updated_value);
}

void node_can_expose_local_parameters_and_children()
{
    NeighborhoodUnionParameters<2> neighborhood{
        .random_biases = {3.0, 1.0},
    };
    FixedLengthParameters temperature{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    };

    const auto tree = root(
        named<"solver">(
            neighborhood,
            named<"temperature">(temperature)));

    bool saw_local_parameter = false;
    bool saw_child_parameter = false;

    for_each_config_parameter(
        tree,
        [&](const auto path, const auto, const auto& value) {
            using path_type = std::remove_cvref_t<decltype(path)>;

            if constexpr (path_is<path_type>(
                              std::array<std::string_view, 2>{
                                  "solver",
                                  "random_biases"}))
            {
                saw_local_parameter =
                    value == std::array<double, 2>{3.0, 1.0};
            }
            else if constexpr (path_is<path_type>(
                                   std::array<std::string_view, 3>{
                                       "solver",
                                       "temperature",
                                       "initial_temperature"}))
            {
                saw_child_parameter = value == 8.0;
            }
        });

    assert(saw_local_parameter);
    assert(saw_child_parameter);
}

void typed_lookup_updates_a_parameter_block_after_validation()
{
    AppParameters app{
        .instance_file = "initial.dat",
        .seed = 17U,
    };

    const auto tree = root(named<"application">(app));
    const auto& endpoint = at<"application">(tree);

    auto updated = endpoint.parameters();
    updated.instance_file = "updated.dat";
    updated.seed = 23U;

    assert(endpoint.configure(updated));
    assert(app.instance_file == std::filesystem::path{"updated.dat"});
    assert(app.seed == 23U);

    auto invalid = endpoint.parameters();
    invalid.instance_file.clear();

    const auto validation = endpoint.configure(invalid);
    assert(!validation);
    assert(app.instance_file == std::filesystem::path{"updated.dat"});
    assert(app.seed == 23U);
}

void typed_lookup_distinguishes_same_typed_configurable_endpoints()
{
    FixedLength fast{FixedLengthParameters{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    }};
    FixedLength slow{FixedLengthParameters{
        .initial_temperature = 80.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.95,
        .max_iterations = 2000,
    }};

    const auto tree = root(
        named<"fast">(fast.configuration()),
        named<"slow">(slow.configuration()));

    const auto& fast_endpoint = at<"fast", "temperature">(tree);
    const auto& slow_endpoint = at<"slow", "temperature">(tree);

    const auto slow_before = slow_endpoint.parameters();
    const auto samples_before = fast.samples_per_temperature();

    auto invalid = fast_endpoint.parameters();
    invalid.final_temperature = invalid.initial_temperature;
    assert(!fast_endpoint.configure(invalid));
    assert(fast_endpoint.parameters().max_iterations == 200);
    assert(fast.samples_per_temperature() == samples_before);

    auto updated = fast_endpoint.parameters();
    updated.max_iterations = 20;
    assert(fast_endpoint.configure(updated));

    assert(fast_endpoint.parameters().max_iterations == 20);
    assert(fast.samples_per_temperature() != samples_before);
    assert(slow_endpoint.parameters().initial_temperature ==
           slow_before.initial_temperature);
    assert(slow_endpoint.parameters().max_iterations ==
           slow_before.max_iterations);
}

void const_configuration_exposure_is_read_only()
{
    const FixedLength policy{FixedLengthParameters{
        .initial_temperature = 8.0,
        .final_temperature = 0.25,
        .cooling_rate = 0.75,
        .max_iterations = 200,
    }};

    const auto tree = root(
        named<"solver">(policy.configuration()));
    const auto& endpoint = at<"solver", "temperature">(tree);

    static_assert(!ConfigurableNode<decltype(endpoint)>);
    assert(endpoint.parameters().max_iterations == 200);
}

} // namespace

int main()
{
    tree_distinguishes_same_typed_blocks_by_path();
    tree_is_a_non_owning_view_of_parameter_blocks();
    node_can_expose_local_parameters_and_children();
    typed_lookup_updates_a_parameter_block_after_validation();
    typed_lookup_distinguishes_same_typed_configurable_endpoints();
    const_configuration_exposure_is_read_only();
}
