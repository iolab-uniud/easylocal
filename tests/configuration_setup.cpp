#include <easylocal/config/parameter_set.hpp>
#include <easylocal/config/setup.hpp>

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{

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

// A requirement of the enclosing block over the fields of its group.
struct WindowParameters
{
    double low{2.0};
    double high{1.0};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"low", &WindowParameters::low>(
                "Low",
                easylocal::unlimited),
            easylocal::config::field<"high", &WindowParameters::high>(
                "High",
                easylocal::unlimited));
    }

    [[nodiscard]]
    auto validate() const noexcept -> easylocal::config::validation_result
    {
        return easylocal::config::check_schema(*this);
    }
};

struct OuterParameters
{
    WindowParameters window{};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::group<"window", &OuterParameters::window>("Window"),
            easylocal::config::require(
                easylocal::config::value<"window.low">
                    < easylocal::config::value<"window.high">,
                "window.low must be below window.high"));
    }

    [[nodiscard]]
    auto validate() const noexcept -> easylocal::config::validation_result
    {
        return easylocal::config::check_schema(*this);
    }
};

struct SolverParameters
{
    double cooling_rate{0.75};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<
                "cooling_rate",
                &SolverParameters::cooling_rate>());
    }

    [[nodiscard]]
    auto validate() const noexcept -> easylocal::config::validation_result
    {
        return cooling_rate > 0.0 && cooling_rate < 1.0
            ? easylocal::config::validation_result::success()
            : easylocal::config::validation_result::failure(
                  "cooling_rate must be in (0, 1)");
    }
};

struct temporary_file
{
    std::filesystem::path path;

    explicit temporary_file(const std::string& contents)
        : path{std::filesystem::current_path() / "easylocal-configuration-setup-test.cfg"}
    {
        std::ofstream output{path};
        output << contents;
    }

    ~temporary_file()
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};

void tree_validation_reports_invalid_untouched_blocks()
{
    AppParameters app{.instance_file = {}, .seed = 17U};
    SolverParameters solver{};
    easylocal::config::parameter_set tree;
    tree.add("application", app);
    tree.add("solver", solver);

    const auto validation = easylocal::config::validate(tree);

    assert(!validation);
    assert(validation.diagnostics.size() == 1);
    assert(validation.diagnostics[0].path == "application");
    assert(validation.diagnostics[0].message ==
           "instance_file must not be empty");
}

void cli_overrides_are_applied()
{
    AppParameters app{};
    SolverParameters solver{};
    easylocal::config::parameter_set tree;
    tree.add("application", app);
    tree.add("solver", solver);

    char program[] = "solver";
    char seed[] = "--application.seed=2026";
    char cooling[] = "--solver.cooling_rate=0.8";
    char* argv[]{program, seed, cooling};

    const auto result = easylocal::config::load_and_apply(3, argv, tree);

    assert(result);
    assert(!result.help_requested);
    assert(app.seed == 2026U);
    assert(solver.cooling_rate == 0.8);
}

// The file of --config is read by the reader given to load_and_apply, as a
// TOML adapter is plugged in; its errors are reported with their line and
// column.
void a_reader_reads_the_config_file()
{
    AppParameters app{};
    SolverParameters solver{};
    easylocal::config::parameter_set tree;
    tree.add("application", app);
    tree.add("solver", solver);

    std::filesystem::path read;
    const easylocal::config::config_file_reader reader =
        [&read](const std::filesystem::path& path) {
            read = path;
            easylocal::config::config_file_parse_result result;
            result.overrides.push_back({"application.seed", "7"});
            return result;
        };
    char program[] = "solver";
    char config[] = "--config=solver.custom";
    char* argv[]{program, config};
    assert(easylocal::config::load_and_apply(2, argv, tree, reader));
    assert(read == std::filesystem::path{"solver.custom"});
    assert(app.seed == 7U);

    const easylocal::config::config_file_reader failing =
        [](const std::filesystem::path& path) {
            easylocal::config::config_file_parse_result result;
            result.diagnostics.push_back({
                .error = easylocal::config::config_file_error::parse_error,
                .line = 3,
                .text = path.string(),
                .message = "unexpected character",
                .column = 5,
            });
            return result;
        };
    const auto result = easylocal::config::load_and_apply(2, argv, tree, failing);
    assert(!result);
    std::ostringstream output;
    easylocal::config::print_diagnostics(output, result);
    assert(
        output.str()
        == "error: config line 3, column 5: unexpected character ('solver.custom')\n");
}

void cli_has_precedence_over_file()
{
    temporary_file file{
        "application.seed = 42\n"
        "solver.cooling_rate = 0.6\n"};

    AppParameters app{};
    SolverParameters solver{};
    easylocal::config::parameter_set tree;
    tree.add("application", app);
    tree.add("solver", solver);

    auto config_option = std::string{"--config="} + file.path.string();
    char program[] = "solver";
    char cooling[] = "--solver.cooling_rate=0.9";
    char* argv[]{program, config_option.data(), cooling};

    const auto result = easylocal::config::load_and_apply(3, argv, tree);

    assert(result);
    assert(app.seed == 42U);
    assert(solver.cooling_rate == 0.9);
}

void invalid_batch_is_transactional()
{
    AppParameters app{};
    SolverParameters solver{};
    easylocal::config::parameter_set tree;
    tree.add("application", app);
    tree.add("solver", solver);

    char program[] = "solver";
    char seed[] = "--application.seed=2026";
    char cooling[] = "--solver.cooling_rate=1.5";
    char* argv[]{program, seed, cooling};

    const auto result = easylocal::config::load_and_apply(3, argv, tree);

    assert(!result);
    assert(app.seed == 17U);
    assert(solver.cooling_rate == 0.75);
}

void invalid_baseline_can_be_repaired_by_overrides()
{
    AppParameters app{.instance_file = {}, .seed = 17U};
    SolverParameters solver{};
    easylocal::config::parameter_set tree;
    tree.add("application", app);
    tree.add("solver", solver);

    char program[] = "solver";
    char repair[] = "--application.instance_file=repaired.dat";
    char* argv[]{program, repair};

    const auto result = easylocal::config::load_and_apply(2, argv, tree);

    assert(result);
    assert(app.instance_file == std::filesystem::path{"repaired.dat"});
}

void invalid_untouched_baseline_preserves_transactionality()
{
    AppParameters app{.instance_file = {}, .seed = 17U};
    SolverParameters solver{};
    easylocal::config::parameter_set tree;
    tree.add("application", app);
    tree.add("solver", solver);

    char program[] = "solver";
    char cooling[] = "--solver.cooling_rate=0.8";
    char* argv[]{program, cooling};

    const auto result = easylocal::config::load_and_apply(2, argv, tree);

    assert(!result);
    assert(solver.cooling_rate == 0.75);
    assert(result.diagnostics.size() == 1);
    assert(result.diagnostics[0].source ==
           easylocal::config::setup_diagnostic_source::validation);
    assert(result.diagnostics[0].subject == "application");
}

void a_nested_override_repairs_a_requirement_of_its_enclosing_block()
{
    OuterParameters outer;
    easylocal::config::parameter_set tree;
    tree.add("outer", outer);

    char program[] = "solver";
    char high[] = "--outer.window.high=3";
    char* argv[]{program, high};

    const auto result = easylocal::config::load_and_apply(2, argv, tree);
    assert(result);
    assert(outer.window.high == 3.0);
}

void help_remains_frontend_policy()
{
    AppParameters app{.instance_file = {}, .seed = 17U};
    SolverParameters solver{};
    easylocal::config::parameter_set tree;
    tree.add("application", app);
    tree.add("solver", solver);

    char program[] = "solver";
    char help[] = "--help";
    char missing_config[] = "--config=definitely-missing.cfg";
    char* argv[]{program, help, missing_config};

    const auto result = easylocal::config::load_and_apply(3, argv, tree);

    assert(result);
    assert(result.help_requested);
    assert(result.diagnostics.empty());
}

void diagnostics_have_a_uniform_rendering_surface()
{
    {
        AppParameters app{};
        SolverParameters solver{};
        easylocal::config::parameter_set tree;
        tree.add("application", app);
        tree.add("solver", solver);

        char program[] = "solver";
        char invalid[] = "--solver.cooling_rate=2.0";
        char* argv[]{program, invalid};

        const auto result = easylocal::config::load_and_apply(2, argv, tree);
        assert(!result);

        std::ostringstream output;
        easylocal::config::print_diagnostics(output, result);
        assert(output.str().find(
                   "solver: cooling_rate must be in (0, 1)") !=
               std::string::npos);
    }

    {
        AppParameters app{};
        SolverParameters solver{};
        easylocal::config::parameter_set tree;
        tree.add("application", app);
        tree.add("solver", solver);

        char program[] = "solver";
        char invalid[] = "--solver.cooling_rate=not-a-number";
        char* argv[]{program, invalid};

        const auto result = easylocal::config::load_and_apply(2, argv, tree);
        assert(!result);

        std::ostringstream output;
        easylocal::config::print_diagnostics(output, result);
        assert(output.str().find(
                   "solver.cooling_rate = 'not-a-number'") !=
               std::string::npos);
        assert(output.str().find("expected a number") != std::string::npos);
    }
}

void every_diagnostic_source_is_rendered()
{
    const auto rendered = [](const int argc, char** argv, AppParameters app) {
        SolverParameters solver{};
        easylocal::config::parameter_set tree;
        tree.add("application", app);
        tree.add("solver", solver);
        const auto result = easylocal::config::load_and_apply(argc, argv, tree);
        assert(!result);
        std::ostringstream output;
        easylocal::config::print_diagnostics(output, result);
        return output.str();
    };

    {
        char program[] = "solver";
        char cooling[] = "--solver.cooling_rate=0.8";
        char* argv[]{program, cooling};
        assert(
            rendered(2, argv, AppParameters{.instance_file = {}, .seed = 17U})
            == "error: application: instance_file must not be empty\n");
    }

    {
        char program[] = "solver";
        char positional[] = "positional";
        char* argv[]{program, positional};
        assert(
            rendered(2, argv, AppParameters{})
            == "error: positional: expected a long option of the form "
               "--path value or --path=value\n");
    }

    {
        temporary_file file{
            "application.seed = 42\n"
            "not an assignment\n"};
        auto config_option = std::string{"--config="} + file.path.string();
        char program[] = "solver";
        char* argv[]{program, config_option.data()};
        assert(
            rendered(2, argv, AppParameters{})
            == "error: config line 2: expected 'path = value' ('not an assignment')\n");
    }

    {
        char program[] = "solver";
        char missing_config[] = "--config=definitely-missing.cfg";
        char* argv[]{program, missing_config};
        assert(
            rendered(2, argv, AppParameters{})
            == "error: cannot open configuration file ('definitely-missing.cfg')\n");
    }
}

} // namespace

int main()
{
    tree_validation_reports_invalid_untouched_blocks();
    cli_overrides_are_applied();
    a_reader_reads_the_config_file();
    cli_has_precedence_over_file();
    invalid_batch_is_transactional();
    invalid_baseline_can_be_repaired_by_overrides();
    invalid_untouched_baseline_preserves_transactionality();
    a_nested_override_repairs_a_requirement_of_its_enclosing_block();
    help_remains_frontend_policy();
    diagnostics_have_a_uniform_rendering_surface();
    every_diagnostic_source_is_rendered();
    return 0;
}
