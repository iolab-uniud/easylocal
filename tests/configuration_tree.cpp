#include <easylocal/config/tree.hpp>
#include <easylocal/neighborhood_union.hpp>
#include <easylocal/search/temperature_policy.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <type_traits>

namespace
{

using easylocal::NeighborhoodUnionParameters;
using easylocal::config::for_each_config_parameter;
using easylocal::config::named;
using easylocal::config::root;
using easylocal::search::temperature::FixedLengthParameters;

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

    assert(visited == 11);
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

} // namespace

int main()
{
    tree_distinguishes_same_typed_blocks_by_path();
    tree_is_a_non_owning_view_of_parameter_blocks();
}
