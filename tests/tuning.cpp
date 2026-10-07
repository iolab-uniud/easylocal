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
#include <clocale>
#include <cstdlib>
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

// A directory of the build's, which another build running this test does not
// share.
std::filesystem::path fresh_directory(const std::string& name)
{
    const auto directory = std::filesystem::current_path() / name;
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

// A program parameter that names a file.
struct Files
{
    std::filesystem::path table{};

    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"table", &Files::table>("A file", easylocal::unlimited));
    }

    config::validation_result validate() const
    {
        return config::validation_result::success();
    }
};

Captured run(
    std::initializer_list<std::string> arguments,
    std::vector<easylocal::tuning_range> ranges = {},
    config::parameter_set own = {})
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
        {.parameters = std::move(own),
            .tuning = std::move(ranges),
            .out = &out,
            .err = &err});
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
    assert(
        invalid.err
        == "error: tuning.print: expected a value in {\"\", cost, cost_time}, got all\n");
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
    // The program as an absolute path: on Windows, with the current drive.
    assert(contains(
        read_file(directory / "target-runner"),
        "exec '" + std::filesystem::weakly_canonical("/opt/bin/tsp").string() + "'"));
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

// fixed.conf holds what every run shares: not the starting solution, which
// belongs to an instance, and paths made absolute, since irace runs the
// program from its own directory.
void fixed_values_are_shared_by_every_run()
{
    const auto directory = fresh_directory("easylocal-tuning-fixed");
    Files files;
    config::parameter_set own;
    own.add("files", files);
    const auto written = run(
        {"--tuning.irace",
            directory.string(),
            "--solution",
            "start.txt",
            "--files.table",
            "data/table.txt"},
        {},
        own);
    assert(written.status == 0);
    const auto fixed = read_file(directory / "fixed.conf");
    assert(!contains(fixed, "solution ="));
    assert(contains(
        fixed,
        "files.table = " + std::filesystem::absolute("data/table.txt").string() + '\n'));
    std::filesystem::remove_all(directory);

    // The weight of the hard cost is the program's: every run uses it.
    const auto weighted =
        run({"--tuning.irace", directory.string(), "--tuning.hard_weight", "1000"});
    assert(weighted.status == 0);
    assert(contains(read_file(directory / "fixed.conf"), "tuning.hard_weight = 1000\n"));
    std::filesystem::remove_all(directory);
}

// A second objective of the tour, for a Pareto cost.
struct NoObjective
{
    static int evaluate(const tutorial::Tour&)
    {
        return 0;
    }
};

// A cost that is not one number: no irace scenario, whose every run would
// fail, and no --tuning.print.
void a_cost_without_a_number_is_not_tuned()
{
    using namespace tutorial;
    const auto directory = fresh_directory("easylocal-tuning-pareto");
    const auto pareto_app = [] {
        return easylocal::app("tsp")
            | (easylocal::solution_manager<TourManager>()
                | cost::objectives(
                    easylocal::component<TourLength>(),
                    easylocal::component<NoObjective>()))
            | easylocal::neighborhood<TwoOptExplorer>()
            | easylocal::runner<easylocal::runners::FirstImprovement>("fi");
    };
    std::string program{"/opt/bin/tsp"};
    std::string irace_switch{"--tuning.irace"};
    std::string irace_directory{directory.string()};
    std::vector<char*> argv{program.data(), irace_switch.data(), irace_directory.data()};
    std::ostringstream out;
    std::ostringstream err;
    const int status = easylocal::cli::run(
        pareto_app(),
        static_cast<int>(argv.size()),
        argv.data(),
        {.out = &out, .err = &err});
    assert(status == 2);
    assert(contains(err.str(), "tuning.irace: this cost is not one number"));
    assert(!std::filesystem::exists(directory));
}

// The stub reads numbers as the program writes them, whatever the locale: a
// decimal comma (it_IT) does not turn 0.5 into 0.
void the_stub_reads_numbers_in_any_locale()
{
    using kind = config::parameter_kind;
    const char* const previous = std::setlocale(LC_NUMERIC, nullptr);
    const std::string saved = previous != nullptr ? previous : "C";
    if (std::setlocale(LC_NUMERIC, "it_IT.UTF-8") == nullptr
        && std::setlocale(LC_NUMERIC, "it_IT") == nullptr)
        return; // no comma locale on this system
    const auto directory = fresh_directory("easylocal-tuning-locale");
    easylocal::irace_stub stub{
        .directory = directory,
        .program = "solver",
        .parameters = {parameter("rate", "0.5", kind::real)},
        .ranges = {},
        .requirements = {},
        .runners = {},
        .fixed = {},
        .instance = {},
    };
    const auto suggested = easylocal::write_irace_stub(stub);
    const auto parameters = read_file(directory / "parameters.txt");
    std::ofstream{directory / "parameters.txt"} << "rate \"--rate=\" r (0.25, 0.75)\n";
    stub.parameters = {parameter("rate", "0.9", kind::real)};
    const auto clamped = easylocal::write_irace_stub(stub);
    const auto configurations = read_file(directory / "configurations.txt");
    std::setlocale(LC_NUMERIC, saved.c_str());

    assert(suggested);
    assert(contains(parameters, "# rate \"--rate=\" r,log (0.05, 5)"));
    assert(clamped);
    assert(configurations == "rate\n0.75\n");
    std::filesystem::remove_all(directory);
}

// An unlimited limit given a finite range starts at its upper bound, the
// nearest to no limit, not at the lower one.
void an_unlimited_limit_starts_at_its_upper_bound()
{
    using kind = config::parameter_kind;
    const auto directory = fresh_directory("easylocal-tuning-unlimited");
    easylocal::irace_stub stub{
        .directory = directory,
        .program = "solver",
        .parameters = {parameter(
            "max_evaluations",
            "unlimited",
            kind::limit,
            config::describe_domain(config::range(0, easylocal::unlimited)))},
        .ranges = {{"max_evaluations", config::range(10, 1000)}},
        .requirements = {},
        .runners = {},
        .fixed = {},
        .instance = {},
    };
    const auto result = easylocal::write_irace_stub(stub);
    assert(result);
    assert(read_file(directory / "configurations.txt") == "max_evaluations\n1000\n");
    std::filesystem::remove_all(directory);
}

// Real bounds with more decimals than irace's default 4 set its digits; the
// open bounds of an integer range move to the nearest integer inside it.
void bounds_keep_their_precision()
{
    using kind = config::parameter_kind;
    const auto directory = fresh_directory("easylocal-tuning-digits");
    easylocal::irace_stub stub{
        .directory = directory,
        .program = "solver",
        .parameters =
            {parameter(
                 "rate",
                 "0.001",
                 kind::real,
                 config::describe_domain(config::range(0.00001, 0.5))),
                parameter(
                    "share",
                    "0.5",
                    kind::real,
                    config::describe_domain(config::range(0.0, 1.0).open())),
                parameter(
                    "count",
                    "5",
                    kind::integer,
                    config::describe_domain(config::range(0.5, 10.5).open()))},
        .ranges = {},
        .requirements = {},
        .runners = {},
        .fixed = {},
        .instance = {},
    };
    const auto result = easylocal::write_irace_stub(stub);
    assert(result);
    const auto parameters = read_file(directory / "parameters.txt");
    assert(contains(parameters, "rate \"--rate=\" r (0.00001, 0.5)\n"));
    assert(contains(parameters, "share \"--share=\" r (0.00001, 0.99999)\n"));
    assert(contains(parameters, "count \"--count=\" i (1, 10)\n"));
    assert(contains(parameters, "[global]\ndigits = 5\n"));
    assert(!contains(read_file(directory / "scenario.txt"), "digits"));
    std::filesystem::remove_all(directory);
}

// irace names a parameter with letters, digits, '.' and '_': a path with other
// characters (a runner "slow-fi") gets an identifier of its own, distinct from
// the others, while its switch stays the program's.
void parameters_get_irace_identifiers()
{
    using kind = config::parameter_kind;
    const auto directory = fresh_directory("easylocal-tuning-identifiers");
    const auto unit = config::describe_domain(config::range(0.0, 1.0));
    easylocal::irace_stub stub{
        .directory = directory,
        .program = "solver",
        .parameters =
            {parameter("runners.slow-fi.rate", "0.5", kind::real, unit),
                parameter("runners.slow_fi.rate", "0.25", kind::real, unit),
                parameter("runners.fi.rate", "0.75", kind::real, unit)},
        .ranges = {},
        .requirements = {},
        .runners = {"slow-fi", "slow_fi", "fi"},
        .fixed = {},
        .instance = {},
    };
    const auto result = easylocal::write_irace_stub(stub);
    assert(result);
    const auto parameters = read_file(directory / "parameters.txt");
    assert(contains(
        parameters,
        "runners.slow_fi.rate \"--runners.slow-fi.rate=\" r (0, 1) | runner == "
        "\"slow-fi\"\n"));
    assert(contains(
        parameters,
        "runners.slow_fi.rate_2 \"--runners.slow_fi.rate=\" r (0, 1) | runner == "
        "\"slow_fi\"\n"));
    assert(
        read_file(directory / "configurations.txt")
        == "runner runners.slow_fi.rate runners.slow_fi.rate_2 runners.fi.rate\n"
           "\"slow-fi\" 0.5 NA NA\n");
    std::filesystem::remove_all(directory);
}

// A block whose requirements name tuned parameters (low, high, in [0, 1]) and
// untuned ones of every kind: a number without a finite domain, a boolean, an
// unlimited limit and a text.
struct Related
{
    double low{0.2};
    double high{0.8};
    double scale{5.0};
    bool verbose{false};
    easylocal::limit budget{easylocal::unlimited};
    std::string label{"plain"};

    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"low", &Related::low>("Low", config::range(0.0, 1.0)),
            config::field<"high", &Related::high>("High", config::range(0.0, 1.0)),
            config::field<"scale", &Related::scale>("Scale"),
            config::field<"verbose", &Related::verbose>("Verbose"),
            config::field<"budget", &Related::budget>("Budget"),
            config::field<"label", &Related::label>("Label"),
            config::require(
                config::value<"low"> < config::value<"high">,
                "low must be below high"),
            config::require(
                config::value<"low"> < config::value<"scale">,
                "low must be below scale"),
            config::require(
                config::value<"verbose"> || config::value<"high"> > 0.5,
                "a quiet run needs high above 0.5"),
            config::require(
                config::value<"budget"> > 10 || config::value<"high"> < 1.0,
                "a small budget needs high below 1"),
            config::require(
                config::value<"label"> != "none" || config::value<"low"> > 0.0,
                "an unlabelled run needs low above 0"),
            // Only untuned parameters: no forbidden line.
            config::require(config::value<"scale"> > 0.0, "scale must be positive"));
    }

    config::validation_result validate() const
    {
        return config::check_schema(*this);
    }
};

// The requirements become [forbidden] lines in which a tuned parameter is
// its irace name and an untuned one its value, as R reads it; a line that
// names an untuned parameter also gives the expression with every name.
void requirements_become_forbidden_lines()
{
    const auto directory = fresh_directory("easylocal-tuning-forbidden");
    Related related;
    config::parameter_set set;
    set.add("block", related);
    easylocal::irace_stub stub{
        .directory = directory,
        .program = "solver",
        .parameters = set.parameters(),
        .ranges = {},
        .requirements = set.requirements(),
        .runners = {},
        .fixed = {},
        .instance = {},
    };
    const auto result = easylocal::write_irace_stub(stub);
    assert(result);
    assert(result.tuned == 2);
    const auto parameters = read_file(directory / "parameters.txt");
    const auto forbidden = parameters.substr(parameters.find("[forbidden]\n"));
    assert(
        forbidden
        == "[forbidden]\n"
           "# low must be below high\n"
           "!(block.low < block.high)\n"
           "# low must be below scale\n"
           "# with every parameter it names tuned: !(block.low < block.scale)\n"
           "!(block.low < 5)\n"
           "# a quiet run needs high above 0.5\n"
           "# with every parameter it names tuned: !((block.verbose == \"true\") || "
           "(block.high > 0.5))\n"
           "!(FALSE || (block.high > 0.5))\n"
           "# a small budget needs high below 1\n"
           "# with every parameter it names tuned: !((block.budget > 10) || (block.high "
           "< 1))\n"
           "!((Inf > 10) || (block.high < 1))\n"
           "# an unlabelled run needs low above 0\n"
           "# with every parameter it names tuned: !((block.label != \"none\") || "
           "(block.low > 0))\n"
           "!((\"plain\" != \"none\") || (block.low > 0))\n");
    std::filesystem::remove_all(directory);
}

#ifndef _WIN32
// target-runner, run as irace runs it, passes the instance, the seed and the
// candidate's switches to the program after fixed.conf, with or without the
// bound irace may insert before the switches, and prints what the program
// prints.
void the_target_runner_runs_the_program()
{
    const auto directory = fresh_directory("easylocal-tuning-target-runner");
    std::filesystem::create_directories(directory);
    const auto program = directory / "fake solver";
    std::ofstream{program}
        << "#!/bin/sh\n"
           "printf '%s\\n' \"$@\" > \"$(dirname \"$0\")/arguments.txt\"\n"
           "echo 42\n";
    std::filesystem::permissions(
        program,
        std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::add);
    easylocal::irace_stub stub{
        .directory = directory,
        .program = program,
        .parameters = {parameter(
            "rate",
            "0.5",
            config::parameter_kind::real,
            config::describe_domain(config::range(0.0, 1.0)))},
        .ranges = {},
        .requirements = {},
        .runners = {},
        .fixed = {},
        .instance = {},
    };
    assert(easylocal::write_irace_stub(stub));

    const auto runner = directory / "target-runner";
    const auto output = directory / "output.txt";
    const auto expected = "--config\n" + (directory / "fixed.conf").string()
        + "\n--tuning.print=cost\n--instance=my instance.tsp\n--seed=7\n--rate=0.25\n";
    for (const std::string bound : {"", " 100"})
    {
        const auto command = "'" + runner.string() + "' 3 1 7 'my instance.tsp'" + bound
            + " --rate=0.25 > '" + output.string() + "'";
        assert(std::system(command.c_str()) == 0);
        assert(read_file(output) == "42\n");
        assert(read_file(directory / "arguments.txt") == expected);
    }
    std::filesystem::remove_all(directory);
}
#endif

} // namespace

void the_stub_filters_and_says_when_nothing_is_tuned()
{
    using kind = config::parameter_kind;
    const auto directory = fresh_directory("easylocal-tuning-nothing");
    auto read_only = parameter("runners.fi.max_evaluations", "10", kind::limit);
    read_only.read_only = true;
    easylocal::irace_stub stub{
        .directory = directory,
        .program = "/opt/bin/solver",
        .parameters =
            {parameter("cost.weights", "[1, 1]", kind::list),
                parameter(
                    "cost.excess.bound",
                    "8",
                    kind::real,
                    config::describe_domain(config::range(0.0, 10.0))),
                read_only,
                parameter("program.verbose", "false", kind::boolean)},
        .ranges = {},
        .requirements = {},
        .runners = {"fi"},
        .fixed = {},
        .instance = {},
    };
    const auto result = easylocal::write_irace_stub(stub);
    assert(result);
    assert(result.tuned == 0);
    assert(result.configurations.empty());
    assert(!std::filesystem::exists(directory / "configurations.txt"));
    std::ifstream in{directory / "parameters.txt"};
    const std::string text{std::istreambuf_iterator<char>{in}, {}};
    assert(text.find("cost.weights") == std::string::npos);
    assert(text.find("cost.excess") == std::string::npos);
    assert(text.find("max_evaluations") == std::string::npos);
    assert(text.find("a switch: uncomment it to tune it") != std::string::npos);
}

int main()
{
    scalar_cost_prefers_the_problem_hook();
    the_stub_filters_and_says_when_nothing_is_tuned();
    the_stub_has_the_parameters_with_conditions();
    ranges_must_name_parameters_within_their_domains();
    cli_prints_only_the_cost();
    cli_writes_the_stub_without_an_instance();
    fixed_values_are_shared_by_every_run();
    a_cost_without_a_number_is_not_tuned();
    the_stub_reads_numbers_in_any_locale();
    an_unlimited_limit_starts_at_its_upper_bound();
    bounds_keep_their_precision();
    parameters_get_irace_identifiers();
    requirements_become_forbidden_lines();
#ifndef _WIN32
    the_target_runner_runs_the_program();
#endif
}
