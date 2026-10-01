#include <easylocal/config/file.hpp>
#include <easylocal/runners/temperature_policy.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>

namespace
{

using easylocal::config::apply_overrides;
using easylocal::config::config_file_error;
using easylocal::config::load_config_file;
using easylocal::config::overlay_overrides;
using easylocal::config::override_views;
using easylocal::config::parse_config_text;
using easylocal::config::text_override;
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
                &AppParameters::instance_file>(
                    "Problem instance file"),
            easylocal::config::field<"seed", &AppParameters::seed>(
                "Pseudo-random generator seed"));
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

void compact_file_syntax_is_parsed()
{
    constexpr auto text = R"(
# EasyLocal example configuration
application.instance_file = sample.tsp
application.seed=2026
solver.temperature.cooling_rate = 0.8
)";

    const auto parsed = parse_config_text(text);

    assert(parsed);
    assert(parsed.overrides.size() == 3);
    assert(parsed.overrides[0].path == "application.instance_file");
    assert(parsed.overrides[0].value == "sample.tsp");
    assert(parsed.overrides[1].path == "application.seed");
    assert(parsed.overrides[1].value == "2026");
    assert(parsed.overrides[2].path == "solver.temperature.cooling_rate");
    assert(parsed.overrides[2].value == "0.8");
}

void file_parser_accumulates_structural_diagnostics()
{
    constexpr auto text = R"(
missing equals
 = empty-path
application.seed = 1
application.seed = 2
)";

    const auto parsed = parse_config_text(text);

    assert(!parsed);
    assert(parsed.diagnostics.size() == 3);
    assert(parsed.diagnostics[0].error == config_file_error::malformed_line);
    assert(parsed.diagnostics[1].error == config_file_error::empty_path);
    assert(parsed.diagnostics[2].error == config_file_error::duplicate_path);
    assert(parsed.diagnostics[0].line == 2);
    assert(parsed.diagnostics[1].line == 3);
    assert(parsed.diagnostics[2].line == 5);
}

void missing_file_is_reported()
{
    const auto parsed = load_config_file(
        std::filesystem::path{"definitely-not-an-easylocal-config-file.cfg"});

    assert(!parsed);
    assert(parsed.diagnostics.size() == 1);
    assert(parsed.diagnostics[0].error == config_file_error::open_error);
    assert(parsed.diagnostics[0].line == 0);
}

void cli_layer_overrides_file_layer()
{
    const auto file = parse_config_text(R"(
application.seed = 41
solver.temperature.max_iterations = 10
solver.temperature.cooling_rate = 0.7
)");
    assert(file);

    constexpr std::array cli{
        text_override{
            .path = "application.seed",
            .value = "42",
        },
        text_override{
            .path = "solver.temperature.max_iterations",
            .value = "20",
        },
    };

    const auto effective = overlay_overrides(
        std::span<const easylocal::config::owned_text_override>{file.overrides},
        std::span<const text_override>{cli});
    const auto views = override_views(
        std::span<const easylocal::config::owned_text_override>{effective});

    assert(effective.size() == 3);

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

    const auto applied = apply_overrides(
        tree,
        std::span<const text_override>{views});

    assert(applied);
    assert(app.seed == 42U);
    assert(temperature.parameters().max_iterations == 20);
    assert(temperature.parameters().cooling_rate == 0.7);
}

void invalid_effective_batch_is_atomic()
{
    const auto file = parse_config_text(R"(
application.seed = 2026
solver.temperature.cooling_rate = 1.5
)");
    assert(file);

    const auto effective = overlay_overrides(
        std::span<const easylocal::config::owned_text_override>{file.overrides},
        std::span<const text_override>{});
    const auto views = override_views(
        std::span<const easylocal::config::owned_text_override>{effective});

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

    const auto applied = apply_overrides(
        tree,
        std::span<const text_override>{views});

    assert(!applied);
    assert(app.seed == 17U);
    assert(temperature.parameters().cooling_rate == 0.75);
}

} // namespace

int main()
{
    compact_file_syntax_is_parsed();
    file_parser_accumulates_structural_diagnostics();
    missing_file_is_reported();
    cli_layer_overrides_file_layer();
    invalid_effective_batch_is_atomic();
    return 0;
}
