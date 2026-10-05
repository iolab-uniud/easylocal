#include <easylocal/adapters/toml.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/config/parameters.hpp>
#include <easylocal/utils/limit.hpp>

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
    easylocal::limit max_evaluations{100};
    easylocal::limit max_iterations{100};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"cooling_rate", &SearchParameters::cooling_rate>(),
            easylocal::config::field<"biases", &SearchParameters::biases>(),
            easylocal::config::field<
                "max_evaluations",
                &SearchParameters::max_evaluations>(),
            easylocal::config::field<
                "max_iterations",
                &SearchParameters::max_iterations>());
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

struct FlagParameters
{
    bool verbose{false};
    std::array<bool, 2> enabled{false, false};
    std::size_t count{0};
    double rate{0.5};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"verbose", &FlagParameters::verbose>(),
            easylocal::config::field<"enabled", &FlagParameters::enabled>(),
            easylocal::config::field<"count", &FlagParameters::count>(),
            easylocal::config::field<"rate", &FlagParameters::rate>());
    }

    [[nodiscard]]
    easylocal::config::validation_result validate() const noexcept
    {
        return easylocal::config::validation_result::success();
    }
};

// Each TOML value is read by its type: a boolean as true or false, a float
// as a real number (not an integer, even 3.0), an integer as one.
void values_are_read_by_their_toml_type()
{
    const auto parsed = easylocal::config::parse_toml_text(R"toml(
[flags]
verbose = true
enabled = [false, true]
count = 3
rate = 2
)toml");
    assert(parsed);

    FlagParameters flags{};
    easylocal::config::parameter_set tree;
    tree.add("flags", flags);
    const auto views = easylocal::config::override_views(parsed.overrides);
    assert(easylocal::config::apply_overrides(tree, views));
    assert(flags.verbose);
    assert((flags.enabled == std::array<bool, 2>{false, true}));
    assert(flags.count == 3);
    assert(flags.rate == 2.0);

    const auto real_count = easylocal::config::parse_toml_text("flags.count = 3.0\n");
    assert(real_count);
    const auto real_views = easylocal::config::override_views(real_count.overrides);
    assert(!easylocal::config::apply_overrides(tree, real_views));
    assert(flags.count == 3);
}

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
max_evaluations = "unlimited"
max_iterations = 500
)toml");

    assert(parsed);
    assert(parsed.overrides.size() == 6);

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
    // A limit is a TOML integer, or the string "unlimited".
    assert(search.max_evaluations.is_unlimited());
    assert(search.max_iterations == 500);

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

    values_are_read_by_their_toml_type();
}
