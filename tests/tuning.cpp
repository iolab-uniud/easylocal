// Tuning with irace: costs as one number (cost::scalar, scalar_cost), the
// files of an irace scenario (write_irace_stub), and cli::run's --tuning.*
// switches.
#include "../examples/tutorial/tsp.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/app/cli.hpp>
#include <easylocal/app/tuning.hpp>
#include <easylocal/cost/hierarchical.hpp>
#include <easylocal/cost/lexicographic.hpp>
#include <easylocal/cost/scalar.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#ifndef EASYLOCAL_TUTORIAL_INSTANCE
#error "EASYLOCAL_TUTORIAL_INSTANCE must name the tutorial's instance"
#endif

namespace
{

namespace config = easylocal::config;
namespace cost = easylocal::cost;

static_assert(cost::scalar(7, 10.0) == 7.0);
static_assert(cost::scalar(cost::hierarchical<int, double>{2, 3.5}, 100.0) == 203.5);
static_assert(cost::scalar(cost::lexicographic<int, int, int>{1, 2, 3}, 10.0) == 123.0);
static_assert(!cost::scalar_convertible<cost::lexicographic<std::string>>);

} // namespace

namespace problem
{

struct Input
{
};

// A problem's own reduction of its costs, found by ADL.
double scalar_cost(const Input&, const cost::hierarchical<int, int>& value)
{
    return value.hard() * 2.0 + value.soft();
}

} // namespace problem

namespace
{

void scalar_cost_prefers_the_problem_hook()
{
    const cost::hierarchical<int, int> value{3, 4};
    assert(easylocal::scalar_cost(problem::Input{}, value, 1000.0) == 10.0);
    struct OtherInput
    {
    };
    assert(easylocal::scalar_cost(OtherInput{}, value, 1000.0) == 3004.0);
}

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream in{path};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

std::filesystem::path fresh_directory(const std::string& name)
{
    const auto directory = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(directory);
    return directory;
}

config::parameter_info parameter(
    std::string path,
    std::string value,
    config::parameter_kind kind,
    config::domain_info domain = {})
{
    return {
        .path = std::move(path),
        .description = "A parameter",
        .value = std::move(value),
        .read_only = false,
        .kind = kind,
        .domain = std::move(domain),
    };
}

void the_stub_has_the_parameters_with_conditions()
{
    using kind = config::parameter_kind;
    const auto directory = fresh_directory("easylocal-tuning-stub");
    easylocal::irace_stub stub{
        .directory = directory / "nested",
        .program = "/opt/bin/solver",
        .parameters =
            {parameter(
                 "runners.sa.cooling_rate",
                 "0.95",
                 kind::real,
                 config::describe_domain(config::range(0.0, 1.0).open())),
                parameter("runners.sa.initial_temperature", "10", kind::real),
                parameter("runners.sa.samples", "100", kind::integer),
                parameter("runners.fi.max_evaluations", "unlimited", kind::limit),
                parameter("runners.ts.tenure", "5", kind::integer),
                parameter("neighborhood.random_biases", "[1, 1]", kind::list),
                parameter("program.verbose", "false", kind::boolean)},
        .ranges = {{"runners.sa.samples", config::range(10, 1000).log()}},
        .requirements = {},
        .runners = {"sa", "fi"},
        .fixed = {{"start", "initial"}},
        .instance = "five.tsp",
    };
    const auto result = easylocal::write_irace_stub(stub);
    assert(result);
    assert(result.written.size() == 5);
    assert(result.configurations == directory / "nested" / "configurations.txt");
    assert(result.tuned == 3);       // runner, cooling_rate, samples
    assert(result.to_complete == 2); // initial_temperature, verbose

    const auto parameters = read_file(directory / "nested" / "parameters.txt");
    assert(contains(parameters, "runner \"--runner=\" c (\"sa\", \"fi\")\n"));
    assert(contains(
        parameters,
        "runners.sa.cooling_rate \"--runners.sa.cooling_rate=\" r (0.0001, 0.9999) | "
        "runner == \"sa\"\n"));
    assert(contains(
        parameters,
        "runners.sa.samples \"--runners.sa.samples=\" i,log (10, 1000) | runner == \"sa\"\n"));
    assert(contains(
        parameters,
        "# runners.sa.initial_temperature \"--runners.sa.initial_temperature=\" r,log (1, "
        "100) | runner == \"sa\"\n"));
    assert(contains(parameters, "# runners.fi.max_evaluations: unlimited"));
    assert(!contains(parameters, "runners.ts.tenure")); // not among the runners
    assert(contains(parameters, "# neighborhood.random_biases: not tunable"));
    assert(contains(
        parameters,
        "# program.verbose \"--program.verbose=\" c (\"true\", \"false\")"));

    const auto configurations = read_file(directory / "nested" / "configurations.txt");
    assert(
        configurations
        == "runner runners.sa.cooling_rate runners.sa.samples\n\"sa\" 0.95 100\n");
    assert(contains(read_file(directory / "nested" / "fixed.conf"), "start = initial\n"));
    const auto target_runner = read_file(directory / "nested" / "target-runner");
    assert(contains(target_runner, "exec '/opt/bin/solver' --config '"));
    assert(contains(target_runner, "--tuning.print=cost --instance=\"$instance\""));
    const auto permissions =
        std::filesystem::status(directory / "nested" / "target-runner").permissions();
    assert(
        (permissions & std::filesystem::perms::owner_exec)
        != std::filesystem::perms::none);
    assert(contains(read_file(directory / "nested" / "instances.txt"), "five.tsp\n"));
    assert(contains(
        read_file(directory / "nested" / "scenario.txt"),
        "configurationsFile = \"./configurations.txt\"\n"));

    // A stub is written once: the files that exist are kept, and
    // configurations.txt follows parameters.txt as the user edited it, with
    // the values moved into its ranges.
    const std::string edited =
        "runners.sa.initial_temperature \"--runners.sa.initial_temperature=\" r (20, 30)\n"
        "runners.sa.samples \"--x=\" o (10, 1000)  # a comment\n"
        "[forbidden]\n"
        "runners.sa.samples > 5\n";
    std::ofstream{directory / "nested" / "parameters.txt"} << edited;
    const auto again = easylocal::write_irace_stub(stub);
    assert(again);
    assert(again.written.empty());
    assert(again.kept.size() == 5);
    assert(read_file(directory / "nested" / "parameters.txt") == edited);
    assert(
        read_file(directory / "nested" / "configurations.txt")
        == "runners.sa.initial_temperature runners.sa.samples\n20 10\n");
    assert(again.moved.size() == 2);
    assert(again.moved[0] == "runners.sa.initial_temperature = 20");

    std::ofstream{directory / "nested" / "parameters.txt"}
        << "runners.sa.typo \"--runners.sa.typo=\" r (0, 1)\n";
    const auto typo = easylocal::write_irace_stub(stub);
    assert(!typo);
    assert(
        typo.errors.front()
        == "parameters.txt names runners.sa.typo, which is not a parameter to tune");
    std::filesystem::remove_all(directory);
}

void ranges_must_name_parameters_within_their_domains()
{
    using kind = config::parameter_kind;
    const auto directory = fresh_directory("easylocal-tuning-errors");
    easylocal::irace_stub stub{
        .directory = directory,
        .program = "solver",
        .parameters = {parameter(
            "rate",
            "0.5",
            kind::real,
            config::describe_domain(config::range(0.0, 1.0)))},
        .ranges = {{"rate", config::range(0.5, 2.0)}, {"unknown", config::range(1, 2)}},
        .requirements = {},
        .runners = {},
        .fixed = {},
        .instance = {},
    };
    const auto result = easylocal::write_irace_stub(stub);
    assert(!result);
    assert(result.errors.size() == 2);
    assert(result.errors[0] == "tuning range for an unknown parameter: unknown");
    assert(
        result.errors[1] == "tuning range [0.5, 2] of rate is outside its domain [0, 1]");
    assert(!std::filesystem::exists(directory));
}

auto tsp_app()
{
    using namespace tutorial;
    using easylocal::runners::SimulatedAnnealing;
    using easylocal::runners::temperature::Classic;
    auto sm =
        easylocal::solution_manager<TourManager>() | easylocal::component<TourLength>();
    auto nhe = easylocal::neighborhood<TwoOptExplorer>()
        | easylocal::delta<TourLength, TwoOptLengthDelta>();
    return easylocal::app("tsp") | sm | nhe
        | easylocal::runner<easylocal::runners::FirstImprovement>("fi")
        | easylocal::runner<SimulatedAnnealing<Classic>>("sa");
}

struct Captured
{
    int status{};
    std::string out;
    std::string err;
};

Captured run(
    std::initializer_list<std::string> arguments,
    std::vector<easylocal::tuning_range> ranges = {})
{
    std::vector<std::string> storage{"/opt/bin/tsp"};
    storage.insert(storage.end(), arguments);
    std::vector<char*> argv;
    for (auto& argument : storage)
        argv.push_back(argument.data());
    std::ostringstream out;
    std::ostringstream err;
    Captured captured;
    captured.status = easylocal::cli::run(
        tsp_app(),
        static_cast<int>(argv.size()),
        argv.data(),
        {.tuning = std::move(ranges), .out = &out, .err = &err});
    captured.out = out.str();
    captured.err = err.str();
    return captured;
}

void cli_prints_only_the_cost()
{
    const std::string instance = EASYLOCAL_TUTORIAL_INSTANCE;
    const auto cost = run({"--instance", instance, "--tuning.print=cost"});
    assert(cost.status == 0);
    assert(cost.out == "26\n");

    const auto timed = run({"--instance", instance, "--tuning.print=cost_time"});
    assert(timed.status == 0);
    assert(timed.out.starts_with("26 "));
    assert(timed.out.find('\n') == timed.out.size() - 1);

    const auto invalid = run({"--instance", instance, "--tuning.print=all"});
    assert(invalid.status == 2);
    assert(invalid.err == "error: tuning: print must be cost or cost_time\n");
}

void cli_writes_the_stub_without_an_instance()
{
    const auto directory = fresh_directory("easylocal-tuning-cli");
    const auto written = run(
        {"--tuning.irace",
            directory.string(),
            "--runners.sa.temperature.final_temperature=0.1"},
        {{"runners.sa.temperature.cooling_rate", config::range(0.8, 0.99)}});
    assert(written.status == 0);
    // runner and cooling_rate: initial_acceptance matters only with
    // calibration_samples, which is not tuned and is 0.
    assert(contains(written.out, "2 parameters to tune"));
    const auto parameters = read_file(directory / "parameters.txt");
    assert(contains(parameters, "runner \"--runner=\" c (\"fi\", \"sa\")"));
    assert(contains(
        parameters,
        "runners.sa.temperature.cooling_rate \"--runners.sa.temperature.cooling_rate=\" r "
        "(0.8, 0.99) | runner == \"sa\""));
    assert(contains(
        parameters,
        "# inactive: (runners.sa.temperature.calibration_samples > 0) is false"));
    assert(contains(
        parameters,
        "\"--runners.sa.temperature.initial_acceptance=\" r (0.0001, 0.9999) | runner == "
        "\"sa\" && (runners.sa.temperature.calibration_samples > 0)\n"));
    assert(contains(
        read_file(directory / "fixed.conf"),
        "runners.sa.temperature.final_temperature = 0.1\n"));
    assert(contains(read_file(directory / "target-runner"), "exec '/opt/bin/tsp'"));
    std::filesystem::remove_all(directory);

    // A runner chosen on the command line is the only one tuned.
    const auto chosen = run({"--tuning.irace", directory.string(), "--runner", "sa"});
    assert(chosen.status == 0);
    const auto only_sa = read_file(directory / "parameters.txt");
    assert(!contains(only_sa, "\"--runner=\""));
    assert(!contains(only_sa, "runners.fi."));
    assert(contains(read_file(directory / "fixed.conf"), "runner = sa\n"));
    std::filesystem::remove_all(directory);

    const auto outside = run(
        {"--tuning.irace", directory.string()},
        {{"runners.sa.temperature.cooling_rate", config::range(0.5, 1.5)}});
    assert(outside.status == 2);
    assert(contains(outside.err, "is outside its domain (0, 1)"));
}

} // namespace

int main()
{
    scalar_cost_prefers_the_problem_hook();
    the_stub_has_the_parameters_with_conditions();
    ranges_must_name_parameters_within_their_domains();
    cli_prints_only_the_cost();
    cli_writes_the_stub_without_an_instance();
}
