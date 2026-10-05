#include "../examples/tutorial/tsp.hpp"
#include "support/pareto_grid.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/app/cli.hpp>
#include <easylocal/config/parameters.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/pareto_late_acceptance_hill_climbing.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/solvers/pipeline.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
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

struct Captured
{
    int status{};
    std::string out;
    std::string err;
};

// A program parameter, given to cli::run next to its own.
struct Extra
{
    std::uint64_t level{1};

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"level", &Extra::level>("A program parameter"));
    }

    easylocal::config::validation_result validate() const
    {
        return easylocal::config::validation_result::success();
    }
};

// An algorithm that throws what is not a std::exception.
struct ThrowingParameters
{
};

class ThrowingAlgorithm
{
public:
    using parameters_type = ThrowingParameters;

    explicit ThrowingAlgorithm(ThrowingParameters) {}

    template<class Context>
    easylocal::search_result<typename Context::solution_type, typename Context::cost_type>
    run(const Context&, typename Context::solution_type)
    {
        throw 42;
    }
};

auto tsp_app()
{
    using namespace tutorial;
    namespace solvers = easylocal::solvers;
    auto sm =
        easylocal::solution_manager<TourManager>() | easylocal::component<TourLength>();
    auto nhe = easylocal::neighborhood<TwoOptExplorer>()
        | easylocal::delta<TourLength, TwoOptLengthDelta>();
    auto descent =
        easylocal::make_runner<easylocal::runners::FirstImprovement>({}) | sm | nhe;
    // A pipeline of two descents, run by name like the runner.
    return easylocal::app("tsp") | sm | nhe
        | easylocal::runner<easylocal::runners::FirstImprovement>("fi")
        | easylocal::pipeline(
            "cascade",
            solvers::stage("first", descent),
            solvers::stage("second", descent));
}

// The tutorial's TSP with Simulated Annealing, whose runs depend on the seed.
auto annealing_app()
{
    using namespace tutorial;
    return easylocal::app("tsp")
        | (easylocal::solution_manager<TourManager>()
            | easylocal::component<TourLength>())
        | (easylocal::neighborhood<TwoOptExplorer>()
            | easylocal::delta<TourLength, TwoOptLengthDelta>())
        | easylocal::runner<easylocal::runners::SimulatedAnnealing<
            easylocal::runners::temperature::Classic>>("sa");
}

// A symmetric instance of 30 cities, written where the test runs: five.tsp is
// too small for runs from different seeds to differ.
std::string thirty_cities()
{
    const auto path =
        (std::filesystem::current_path() / "cli-run-thirty-cities.tsp").string();
    std::ofstream file{path};
    const std::size_t n = 30;
    file << n << '\n';
    for (std::size_t i = 0; i < n; ++i)
    {
        for (std::size_t j = 0; j < n; ++j)
            file << (i == j ? 0 : 1 + (i * 7 + j * 7 + i * j * 13) % 97) << ' ';
        file << '\n';
    }
    return path;
}

// The output of a run without its time line, which differs from run to run.
std::string without_time(const std::string& out)
{
    std::istringstream in{out};
    std::string result;
    for (std::string line; std::getline(in, line);)
        if (!line.starts_with("time "))
            result += line + '\n';
    return result;
}

// Points of a grid with two objectives: a run has a front.
auto grid_app()
{
    using namespace pareto_grid;
    return easylocal::app("grid") | grid_solution_manager()
        | easylocal::neighborhood<StepNeighborhood>()
        | easylocal::runner<easylocal::runners::ParetoLateAcceptanceHillClimbing>(
            "plahc");
}

template<class App>
Captured run_app(
    App application,
    std::initializer_list<std::string> arguments,
    easylocal::config::parameter_set own = {},
    easylocal::cli::parameters defaults = {})
{
    std::vector<std::string> storage{"cli_run"};
    storage.insert(storage.end(), arguments);
    std::vector<char*> argv;
    for (auto& argument : storage)
        argv.push_back(argument.data());

    std::ostringstream out;
    std::ostringstream err;
    Captured captured;
    captured.status = easylocal::cli::run(
        std::move(application),
        static_cast<int>(argv.size()),
        argv.data(),
        {.defaults = std::move(defaults),
            .parameters = std::move(own),
            .out = &out,
            .err = &err});
    captured.out = out.str();
    captured.err = err.str();
    return captured;
}

Captured run(
    std::initializer_list<std::string> arguments,
    easylocal::config::parameter_set own = {},
    easylocal::cli::parameters defaults = {})
{
    return run_app(tsp_app(), arguments, std::move(own), std::move(defaults));
}

// The number of times text occurs in out.
std::size_t occurrences(const std::string& out, const std::string& text)
{
    std::size_t count = 0;
    for (auto at = out.find(text); at != std::string::npos; at = out.find(text, at + 1))
        ++count;
    return count;
}

} // namespace

int main()
{
    const std::string instance = EASYLOCAL_TUTORIAL_INSTANCE;

    // A run from a random tour: the descent reaches the optimum of five.tsp.
    const auto solved = run({"--instance", instance, "--seed", "1", "--runner", "fi"});
    assert(solved.status == 0);
    assert(solved.out.starts_with("cost 26\ntime "));
    assert(solved.err.empty());

    // The report: each component's value, then its describe(solution) text,
    // indented; TourLength gives the edges it adds up.
    const auto reported = run(
        {"--instance", instance, "--seed", "1", "--runner", "fi", "--report", "true"});
    assert(reported.status == 0);
    assert(reported.out.find("\ncomponent TourLength 26\n  ") != std::string::npos);

    // The effort of the run, which First Improvement reports.
    assert(solved.out.find("\ntermination local optimum\n") != std::string::npos);
    assert(solved.out.find("\niterations ") != std::string::npos);

    // Defaults stand for switches the command line does not give.
    const auto defaulted = run({}, {}, {.instance = instance, .start = "initial"});
    assert(defaulted.status == 0);
    assert(defaulted.out.starts_with("cost 26\n")); // 0 1 2 3 4 (29) improved

    // The first registered runner when none is named.
    assert(run({"--instance", instance}).status == 0);

    // The initial tour costs 29: a target of 30 stops the run at once.
    const auto stopped =
        run({"--instance", instance, "--start", "initial", "--target", "30"});
    assert(stopped.status == 0);
    assert(stopped.out.starts_with("cost 29\n"));
    assert(stopped.out.ends_with("0 1 2 3 4 \n"));

    // A starting solution from a file, and the solution saved to a file, in
    // the build's directory, which another build running this test does not
    // share.
    const auto directory = std::filesystem::current_path();
    const auto start_file = directory / "easylocal_cli_run_start.txt";
    const auto output_file = directory / "easylocal_cli_run_output.txt";
    std::ofstream{start_file} << "0 1 3 4 2\n";
    const auto saved = run(
        {"--instance",
            instance,
            "--solution",
            start_file.string(),
            "--output",
            output_file.string()});
    assert(saved.status == 0);
    assert(saved.out.starts_with("cost 26\ntime "));
    assert(saved.out.find("0 1 3 4 2") == std::string::npos); // in the file
    assert(std::filesystem::exists(output_file));
    std::filesystem::remove(output_file);

    // A run with a Pareto cost: after the solution, the front, each point with
    // its cost and its solution, here on the row y = 0. A tour has no front.
    assert(solved.out.find("\nfront ") == std::string::npos);
    const auto grid_file = directory / "easylocal_cli_run_grid.txt";
    std::ofstream{grid_file} << "grid\n";
    const auto fronted = run_app(grid_app(), {"--instance", grid_file.string()});
    assert(fronted.status == 0);
    const auto front_at = fronted.out.find("\nfront ");
    assert(front_at != std::string::npos);
    const auto size = std::stoul(fronted.out.substr(front_at + 7));
    assert(size >= 2);
    assert(occurrences(fronted.out, "\npoint ") == size);
    assert(fronted.out.find("\npoint 1 cost [") != std::string::npos);
    assert(occurrences(fronted.out, " 0\n") >= size + 1); // the solution too

    // An option the problem cannot honour is an error of the command line,
    // found before the Input is read: the grid reads no solutions.
    const auto unsupported = run_app(
        grid_app(),
        {"--instance", (directory / "no_such_grid.txt").string(), "--solution", "s.txt"});
    assert(unsupported.status == 2);
    assert(unsupported.err == "error: solution: this problem cannot read solutions\n");

    // With --output the solutions are files next to it, numbered from 1.
    const auto front_output = directory / "easylocal_cli_run_front.txt";
    const auto saved_front = run_app(
        grid_app(),
        {"--instance", grid_file.string(), "--output", front_output.string()});
    assert(saved_front.status == 0);
    assert(std::filesystem::exists(front_output));
    std::filesystem::remove(front_output);
    const auto saved_size =
        std::stoul(saved_front.out.substr(saved_front.out.find("\nfront ") + 7));
    assert(occurrences(saved_front.out, " 0\n") == 0); // in the files
    for (std::size_t index = 1; index <= saved_size; ++index)
    {
        const auto file =
            directory / ("easylocal_cli_run_front." + std::to_string(index) + ".txt");
        assert(std::filesystem::exists(file));
        std::ifstream in{file};
        int x = -1;
        int y = -1;
        in >> x >> y;
        assert(x >= 0 && y == 0);
        in.close();
        std::filesystem::remove(file);
    }
    assert(!std::filesystem::exists(
        directory
        / ("easylocal_cli_run_front." + std::to_string(saved_size + 1) + ".txt")));
    std::filesystem::remove(grid_file);

    // A starting solution that is not valid for the Input does not run.
    std::ofstream{start_file} << "0 1 7 4 2\n";
    const auto invalid_start =
        run({"--instance", instance, "--solution", start_file.string()});
    assert(invalid_start.status == 2);
    assert(invalid_start.err.starts_with("error: solution: "));
    assert(invalid_start.err.find("not valid") != std::string::npos);
    std::filesystem::remove(start_file);

    // Invalid command lines: exit status 2, with a message.
    const auto missing = run({});
    assert(missing.status == 2);
    assert(missing.err == "error: instance must be set\n");

    const auto unknown = run({"--instance", instance, "--runner", "sa"});
    assert(unknown.status == 2);
    assert(unknown.err == "error: unknown runner sa; the runners are: fi cascade\n");

    // A time limit: none left, the run stops at its first check.
    const auto timed_out =
        run({"--instance", instance, "--seed", "1", "--runner", "fi", "--timeout", "0"});
    assert(timed_out.status == 0);
    assert(timed_out.out.find("\ntermination time limit reached\n") != std::string::npos);
    const auto with_target = run(
        {"--instance", instance, "--runner", "fi", "--timeout", "30", "--target", "26"});
    assert(with_target.status == 0);
    assert(with_target.out.find("\ntermination target reached\n") != std::string::npos);
    const auto budgeted =
        run({"--instance", instance, "--runner", "fi", "--max_evaluations", "1"});
    assert(budgeted.status == 0);
    assert(
        budgeted.out.find("\nevaluations 1\ntermination evaluation budget exhausted\n")
        != std::string::npos);
    const auto negative = run({"--instance", instance, "--timeout", "-1"});
    assert(negative.status == 2);
    assert(
        negative.err.find("timeout must be a non-negative number") != std::string::npos);
    const auto bad_target = run({"--instance", instance, "--target", "soon"});
    assert(bad_target.status == 2);
    assert(bad_target.err.starts_with("error: target: "));

    // A pipeline whose stages share a name has no parameters to give: an
    // error, not an exception out of cli::run.
    {
        using namespace tutorial;
        namespace solvers = easylocal::solvers;
        auto sm = easylocal::solution_manager<TourManager>()
            | easylocal::component<TourLength>();
        auto nhe = easylocal::neighborhood<TwoOptExplorer>()
            | easylocal::delta<TourLength, TwoOptLengthDelta>();
        auto descent =
            easylocal::make_runner<easylocal::runners::FirstImprovement>({}) | sm | nhe;
        auto twice = easylocal::app("tsp") | sm | nhe
            | easylocal::pipeline(
                "twice",
                solvers::stage("same", descent),
                solvers::stage("same", descent));
        std::string program{"cli_run"};
        std::vector<char*> argv{program.data()};
        std::ostringstream out;
        std::ostringstream err;
        const int status = easylocal::cli::run(
            std::move(twice),
            1,
            argv.data(),
            {.out = &out, .err = &err});
        assert(status == 2);
        assert(err.str().starts_with("error: "));
    }

    // A pipeline is run by name, and configured under runners.<name>.
    const auto cascaded = run(
        {"--instance",
            instance,
            "--seed",
            "1",
            "--runner",
            "cascade",
            "--runners.cascade.second.attempts",
            "2"});
    assert(cascaded.status == 0);
    assert(cascaded.out.starts_with("cost 26\ntime "));

    // --trace records the run: JSON Lines for a .jsonl name, with the
    // metadata in the header line, a run_context per stage and timestamps;
    // ELTR otherwise.
    {
        const auto jsonl_file = directory / "easylocal_cli_run_trace.jsonl";
        const auto traced = run(
            {"--instance",
                instance,
                "--seed",
                "1",
                "--runner",
                "cascade",
                "--trace",
                jsonl_file.string()});
        assert(traced.status == 0);
        assert(traced.out.starts_with("cost 26\ntime "));
        std::ifstream jsonl{jsonl_file};
        std::string header;
        std::getline(jsonl, header);
        assert(header.starts_with("{\"event\":\"trace\",\"version\":1,\"metadata\":{"));
        assert(header.find("\"runner\":\"cascade\"") != std::string::npos);
        assert(header.find("\"seed\":\"1\"") != std::string::npos);
        const std::string events{
            std::istreambuf_iterator<char>{jsonl},
            std::istreambuf_iterator<char>{}};
        assert(
            events.find(
                "{\"event\":\"run_context\",\"stage\":\"first\",\"stage_index\":0,"
                "\"attempt\":0,\"elapsed_ns\":")
            != std::string::npos);
        assert(events.find("\"stage\":\"second\"") != std::string::npos);
        assert(events.find("\"event\":\"run_finished\"") != std::string::npos);
        jsonl.close();
        std::filesystem::remove(jsonl_file);

        const auto eltr_file = directory / "easylocal_cli_run_trace.eltrace";
        const auto binary = run(
            {"--instance", instance, "--runner", "fi", "--trace", eltr_file.string()});
        assert(binary.status == 0);
        std::ifstream eltr{eltr_file, std::ios::binary};
        std::string magic(4, '\0');
        eltr.read(magic.data(), 4);
        assert(magic == "ELTR");
        eltr.close();
        std::filesystem::remove(eltr_file);

        const auto unwritable = run(
            {"--instance",
                instance,
                "--trace",
                (directory / "no-such-directory" / "trace.jsonl").string()});
        assert(unwritable.status == 1);
        assert(unwritable.err.starts_with("error: trace: cannot write "));
    }

    const auto bad_start = run({"--instance", instance, "--start", "greedy"});
    assert(bad_start.status == 2);
    assert(
        bad_start.err
        == "error: start: expected a value in {\"\", random, initial}, got greedy\n");

    // A run that fails: exit status 1.
    const auto unreadable = run({"--instance", "no-such-instance.tsp"});
    assert(unreadable.status == 1);
    assert(unreadable.err.starts_with("error: failed to open Input file"));

    // A run that throws something else than a std::exception: exit status 1.
    {
        using namespace tutorial;
        auto throwing = easylocal::app("tsp")
            | (easylocal::solution_manager<TourManager>()
                | easylocal::component<TourLength>())
            | easylocal::neighborhood<TwoOptExplorer>()
            | easylocal::runner<ThrowingAlgorithm>("throwing");
        std::string program{"cli_run"};
        std::string instance_switch{"--instance"};
        std::string instance_path{instance};
        std::vector<char*>
            argv{program.data(), instance_switch.data(), instance_path.data()};
        std::ostringstream out;
        std::ostringstream err;
        const int status = easylocal::cli::run(
            std::move(throwing),
            static_cast<int>(argv.size()),
            argv.data(),
            {.out = &out, .err = &err});
        assert(status == 1);
        assert(err.str() == "error: unknown exception\n");
    }

    // The app's and the program's parameters are on the same command line.
    Extra extra;
    easylocal::config::parameter_set own;
    own.add("program", extra);
    const auto configured = run(
        {"--instance",
            instance,
            "--runners.fi.max_evaluations",
            "1",
            "--program.level",
            "3"},
        own);
    assert(configured.status == 0);
    assert(extra.level == 3);

    // A seed reproduces a stochastic run, its random start included, and a
    // Session with the same seed and the same commands reaches the same tour:
    // the command line, the Session, the TextUI and REST draw alike.
    {
        const auto cities = thirty_cities();
        const auto annealed = [&](const std::string& seed) {
            return run_app(
                annealing_app(),
                {"--instance",
                    cities,
                    "--seed",
                    seed,
                    "--runner",
                    "sa",
                    "--max_evaluations",
                    "300"});
        };
        const auto first = annealed("11");
        assert(first.status == 0);
        assert(without_time(annealed("11").out) == without_time(first.out));

        easylocal::Session session{annealing_app(), 11};
        session.load_input(cities);
        session.use_random_solution(session.rng());
        assert(session.run("sa", easylocal::max_evaluations(300)));
        std::ostringstream tour;
        session.save_solution(tour);
        assert(first.out.starts_with(
            "cost " + easylocal::detail::report_text(session.evaluate()) + "\n"));
        assert(first.out.find(tour.str()) != std::string::npos);
        std::filesystem::remove(cities);
    }

    const auto help = run({"--help"});
    assert(help.status == 0);
    assert(help.out.find("--instance") != std::string::npos);
    assert(help.out.find("--runners.fi.max_evaluations") != std::string::npos);
    assert(help.out.find("--runners.cascade.first.attempts") != std::string::npos);

    return 0;
}
