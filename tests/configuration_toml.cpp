#include <easylocal/adapters/toml.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/config/parameters.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <string>

namespace
{

struct AppParameters
{
    std::filesystem::path instance_file{"default.tsp"};
    std::size_t seed{1};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"instance_file", &AppParameters::instance_file>(),
            easylocal::config::field<"seed", &AppParameters::seed>());
    }

    [[nodiscard]]
    auto validate() const noexcept -> easylocal::config::validation_result
    {
        return easylocal::config::validation_result::success();
    }
};

struct SearchParameters
{
    double cooling_rate{0.9};
    std::array<double, 2> biases{1.0, 1.0};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"cooling_rate", &SearchParameters::cooling_rate>(),
            easylocal::config::field<"biases", &SearchParameters::biases>());
    }

    [[nodiscard]]
    auto validate() const noexcept -> easylocal::config::validation_result
    {
        if (!(cooling_rate > 0.0 && cooling_rate < 1.0))
        {
            return easylocal::config::validation_result::failure(
                "cooling_rate must be in (0, 1)");
        }
        return easylocal::config::validation_result::success();
    }
};

} // namespace

int main()
{
    const auto parsed = easylocal::config::parse_toml_text(R"toml(
[application]
instance_file = "instances/small.tsp"
seed = 2026

[solver.search]
cooling_rate = 0.75
biases = [3.0, 1.0]
)toml");

    assert(parsed);
    assert(parsed.overrides.size() == 4);

    AppParameters app{};
    SearchParameters search{};
    easylocal::config::parameter_set tree;
    tree.add("application", app);
    tree.add("solver.search", search);

    const auto views = easylocal::config::override_views(parsed.overrides);
    const auto applied = easylocal::config::apply_overrides(tree, views);
    assert(applied);
    assert(app.instance_file == std::filesystem::path{"instances/small.tsp"});
    assert(app.seed == 2026);
    assert(search.cooling_rate == 0.75);
    assert((search.biases == std::array<double, 2>{3.0, 1.0}));

    const auto malformed = easylocal::config::parse_toml_text("x = [1,");
    assert(!malformed);
    assert(!malformed.diagnostics.empty());
    assert(malformed.diagnostics.front().error ==
           easylocal::config::toml_config_error::parse_error);

    const auto unsupported = easylocal::config::parse_toml_text(
        "date = 1979-05-27\n");
    assert(!unsupported);
    assert(unsupported.diagnostics.size() == 1);
    assert(unsupported.diagnostics.front().error ==
           easylocal::config::toml_config_error::unsupported_value);
    assert(unsupported.diagnostics.front().path == "date");

    const auto string_array = easylocal::config::parse_toml_text(
        "names = [\"a\", \"b\"]\n");
    assert(!string_array);
    assert(string_array.diagnostics.size() == 1);
    assert(string_array.diagnostics.front().error ==
           easylocal::config::toml_config_error::unsupported_value);
    assert(string_array.diagnostics.front().path == "names");
}
