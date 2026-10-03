#include <easylocal/config/setup.hpp>
#include <easylocal/config/tree.hpp>

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
        : path{std::filesystem::temp_directory_path() /
               "easylocal-configuration-setup-test.cfg"}
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
    const auto tree = easylocal::config::root(
        easylocal::config::named<"application">(app),
        easylocal::config::named<"solver">(solver));

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
    const auto tree = easylocal::config::root(
        easylocal::config::named<"application">(app),
        easylocal::config::named<"solver">(solver));

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

void cli_has_precedence_over_file()
{
    temporary_file file{
        "application.seed = 42\n"
        "solver.cooling_rate = 0.6\n"};

    AppParameters app{};
    SolverParameters solver{};
    const auto tree = easylocal::config::root(
        easylocal::config::named<"application">(app),
        easylocal::config::named<"solver">(solver));

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
    const auto tree = easylocal::config::root(
        easylocal::config::named<"application">(app),
        easylocal::config::named<"solver">(solver));

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
    const auto tree = easylocal::config::root(
        easylocal::config::named<"application">(app),
        easylocal::config::named<"solver">(solver));

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
    const auto tree = easylocal::config::root(
        easylocal::config::named<"application">(app),
        easylocal::config::named<"solver">(solver));

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

void help_remains_frontend_policy()
{
    AppParameters app{.instance_file = {}, .seed = 17U};
    SolverParameters solver{};
    const auto tree = easylocal::config::root(
        easylocal::config::named<"application">(app),
        easylocal::config::named<"solver">(solver));

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
        const auto tree = easylocal::config::root(
            easylocal::config::named<"application">(app),
            easylocal::config::named<"solver">(solver));

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
        const auto tree = easylocal::config::root(
            easylocal::config::named<"application">(app),
            easylocal::config::named<"solver">(solver));

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
        assert(output.str().find("expected floating-point value") !=
               std::string::npos);
    }
}

void every_diagnostic_source_is_rendered()
{
    const auto rendered = [](const int argc, char** argv, AppParameters app) {
        SolverParameters solver{};
        const auto tree = easylocal::config::root(
            easylocal::config::named<"application">(app),
            easylocal::config::named<"solver">(solver));
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
    cli_has_precedence_over_file();
    invalid_batch_is_transactional();
    invalid_baseline_can_be_repaired_by_overrides();
    invalid_untouched_baseline_preserves_transactionality();
    help_remains_frontend_policy();
    diagnostics_have_a_uniform_rendering_surface();
    every_diagnostic_source_is_rendered();
    return 0;
}
