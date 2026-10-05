#pragma once

/// \file
/// cli::run: an app as a command-line program.
///
/// It reads the Input, the seed, the runner and the app's parameters from the
/// command line and a configuration file, runs the runner by name on a Session,
/// and prints the cost and the solution: the batch counterpart of the TextUI
/// and the REST service. With `--tuning.*` it writes an irace scenario, or
/// prints only the cost, for automatic configurators.

#include <easylocal/app/io.hpp>
#include <easylocal/app/run_parameters.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/app/tuning.hpp>
#include <easylocal/config/cli.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/config/parameters.hpp>
#include <easylocal/config/setup.hpp>
#include <easylocal/cost/text.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace easylocal::cli
{

/// The command line of cli::run: --instance, --seed, --runner, --start,
/// --solution, --output, --target, --timeout, --max_evaluations and --report.
///
/// The app's parameters come next to them: `--runners.<name>.*`, `--cost.*` and
/// `--neighborhood.*`. Every field has an initializer, so that designated
/// initializers, as in options::defaults, may name only some of them.
struct parameters
{
    /// The Input file; it must be set to run, not to write a tuning scenario.
    std::filesystem::path instance{};
    /// The seed of the random generator.
    std::uint64_t seed{0};
    /// The name of the runner; empty: the first registered.
    std::string runner{};
    /// The starting solution, random or initial; empty: random when the problem
    /// has random solutions.
    std::string start{};
    /// A file to read the starting solution from, instead of start.
    std::filesystem::path solution{};
    /// The file to write the solution to; empty: the standard output.
    std::filesystem::path output{};
    /// The cost at which the run stops, such as 0 or [0, 120]; empty: no
    /// target.
    std::string target{};
    /// The seconds the run may last, such as 10 or 2.5; empty: no limit.
    std::string timeout{};
    /// The evaluations the run may make, the initial one included; unlimited:
    /// no budget beyond the runner's own.
    limit max_evaluations{unlimited};
    /// Whether to print the value of each cost component, and its description.
    bool report{false};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"instance", &parameters::instance>(
                "Input file",
                easylocal::unlimited),
            config::field<"seed", &parameters::seed>(
                "Seed of the random generator",
                easylocal::unlimited),
            config::field<"runner", &parameters::runner>(
                "Name of the runner (empty: the first registered)",
                easylocal::unlimited),
            config::field<"start", &parameters::start>(
                "Starting solution: random or initial (empty: random when the "
                "problem has random solutions)",
                config::one_of("", "random", "initial")),
            config::field<"solution", &parameters::solution>(
                "Starting solution read from this file, instead of start",
                easylocal::unlimited),
            config::field<"output", &parameters::output>(
                "Solution file (empty: standard output)",
                easylocal::unlimited),
            config::field<"target", &parameters::target>(
                "Stop when the solution reaches this cost, such as 0 or "
                "[0, 120] (empty: no target)",
                easylocal::unlimited),
            config::field<"timeout", &parameters::timeout>(
                "Stop the run after this many seconds, such as 10 or 2.5 (empty: no "
                "limit)",
                easylocal::unlimited),
            config::field<"max_evaluations", &parameters::max_evaluations>(
                "Stop the run after this many evaluations (unlimited: no budget "
                "beyond the runner's own)",
                config::range(0, easylocal::unlimited)),
            config::field<"report", &parameters::report>(
                "Print the value of each cost component, and its description"));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const
    {
        if (const auto schema = config::check_schema(*this); !schema)
            return schema;
        if (!timeout_seconds())
        {
            return config::validation_result::failure(
                "timeout must be a non-negative number of seconds");
        }
        return config::validation_result::success();
    }

    /// The time limit in seconds: empty without one, std::nullopt inside when
    /// the text is not a non-negative number.
    [[nodiscard]]
    std::optional<std::optional<double>> timeout_seconds() const
    {
        const auto first = timeout.find_first_not_of(" \t");
        if (first == std::string::npos)
            return std::optional<double>{};
        const auto last = timeout.find_last_not_of(" \t");
        double seconds{};
        const auto* const begin = timeout.data() + first;
        const auto* const end = timeout.data() + last + 1;
        const auto [parsed, error] = std::from_chars(begin, end, seconds);
        if (error != std::errc{} || parsed != end || !(seconds >= 0.0)
            || !std::isfinite(seconds))
        {
            return std::nullopt;
        }
        return std::optional<double>{seconds};
    }
};

/// What cli::run adds to the command line and where it writes.
struct options
{
    /// The values of the switches before the command line, such as an
    /// instance or a runner to use when none is given.
    cli::parameters defaults{};

    /// The program's own parameters, parsed with the others; they refer to
    /// blocks that must outlive the call.
    config::parameter_set parameters{};
    /// The values to try for parameters when tuning, by path, instead of the
    /// domains of their schemas: what --tuning.irace writes as their ranges.
    std::vector<tuning_range> tuning{};
    /// Where the help and the results go: the cost, the effort, the report and
    /// the solution, when no output file is given.
    std::ostream* out{&std::cout};
    /// Where the errors go.
    std::ostream* err{&std::cerr};
};

namespace detail
{

// One line per cost component, "component <name> <value>", followed by its
// description, indented, when it has one.
template<class Session>
void write_report(std::ostream& out, const Session& session)
{
    for (const auto& component : session.cost_report())
    {
        out << "component " << component.name << ' ' << component.value << '\n';
        std::string_view description{component.description};
        while (!description.empty())
        {
            const auto end = description.find('\n');
            out << "  " << description.substr(0, end) << '\n';
            if (end == std::string_view::npos)
                break;
            description.remove_prefix(end + 1);
        }
    }
}

template<class Session>
void write_solution(std::ostream& out, const Session& session)
{
    if constexpr (Session::supports_solution_saving)
        session.save_solution(out);
    else
        out << easylocal::describe(session.solution()) << '\n';
}

// --tuning.irace: writes the irace scenario of the program, with the values
// changed on the command line as the values every run starts from.
inline int write_irace(
    std::ostream& out,
    std::ostream& err,
    const std::string_view program,
    const parameters& command_line,
    const TuningParameters& tuning,
    const options& settings,
    const std::vector<config::parameter_info>& defaults,
    const std::vector<config::parameter_info>& values,
    std::vector<config::parameter_info> tunable,
    std::vector<config::requirement_info> requirements,
    const std::vector<std::string_view>& names)
{
    irace_stub stub{
        .directory = tuning.irace,
        .program = std::filesystem::path{program},
        .parameters = std::move(tunable),
        .ranges = settings.tuning,
        .requirements = std::move(requirements),
        .runners = {},
        .fixed = {},
        .instance = command_line.instance,
    };
    std::error_code error;
    if (stub.program.has_parent_path())
        stub.program = std::filesystem::weakly_canonical(stub.program, error);
    // A runner chosen on the command line is the only one tuned.
    if (!command_line.runner.empty())
        stub.runners.push_back(command_line.runner);
    else
        stub.runners.assign(names.begin(), names.end());
    // What every run shares: not what belongs to one run, such as its
    // instance or its starting solution, and paths made absolute, since irace
    // runs the program from its own directory.
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        const auto& path = values[index].path;
        const bool per_run = path == "instance" || path == "seed" || path == "solution"
            || path == "output" || path == "report" || path == "tuning.irace"
            || path == "tuning.print";
        if (per_run || values[index].value == defaults[index].value)
            continue;
        auto value = values[index].value;
        if (values[index].kind == config::parameter_kind::path && !value.empty())
            value = std::filesystem::absolute(value, error).string();
        stub.fixed.push_back({path, std::move(value)});
    }

    const auto result = write_irace_stub(stub);
    for (const auto& message : result.errors)
        err << "tuning.irace: " << message << '\n';
    if (!result)
        return 2;
    for (const auto& path : result.written)
        out << "wrote " << path.string() << '\n';
    for (const auto& path : result.kept)
        out << "kept " << path.string() << " (it exists)\n";
    out << "updated " << result.configurations.string() << '\n';
    for (const auto& moved : result.moved)
        out << "  first configuration moved into its range: " << moved << '\n';
    out << result.tuned << " parameters to tune";
    if (result.to_complete != 0)
        out << ", " << result.to_complete << " more to complete in parameters.txt";
    out << "; then run irace in " << tuning.irace.string() << '\n';
    return 0;
}

} // namespace detail

/// Runs application as a program: parses argc and argv (and a --config file),
/// loads the Input, starts from a random, initial or loaded solution, runs the
/// chosen runner and prints "cost", "time" (seconds), the effort of the run
/// ("iterations", "evaluations", "termination") when the algorithm reports it,
/// with --report the value of each cost component, and the solution, or saves
/// it to --output.
///
/// With --tuning.print=cost it prints only the cost as one number
/// (scalar_cost), and the running time after it with cost_time; with
/// --tuning.irace=DIR it writes an irace scenario to DIR (write_irace_stub),
/// with the ranges of settings.tuning, and exits without loading the Input.
///
/// Returns the exit status: 0 on success, 1 when the run fails (an unreadable
/// file or any other exception), 2 for an invalid command line, a --solution
/// that is not valid for the Input included.
template<class App>
[[nodiscard]]
int run(App application, const int argc, char* argv[], options settings = {})
{
    using session_type = Session<App>;
    static_assert(
        session_type::supports_input_loading,
        "cli::run reads the Input from a file: give the Input a read_input hook");

    auto& out = *settings.out;
    auto& err = *settings.err;

    parameters command_line = settings.defaults;
    TuningParameters tuning;
    config::parameter_set configuration;
    configuration.add(command_line);
    try
    {
        // A pipeline with two stages of the same name, or one without a name,
        // has no parameters to give.
        configuration.add(application.configuration());
    }
    catch (const std::invalid_argument& error)
    {
        err << "error: " << error.what() << '\n';
        return 2;
    }
    configuration.add(settings.parameters);
    configuration.add("tuning", tuning);
    const auto defaults = configuration.parameters();

    const auto configured = config::load_and_apply(argc, argv, configuration);
    if (configured.help_requested)
    {
        out << config::cli_help(argc > 0 ? argv[0] : "program", configuration);
        return 0;
    }
    if (!configured)
    {
        config::print_diagnostics(err, configured);
        return 2;
    }

    // The parameters irace may tune: the app's, but those of the cost, and
    // the program's own.
    std::vector<config::parameter_info> tunable;
    std::vector<config::requirement_info> requirements;
    if (!tuning.irace.empty())
    {
        for (auto& info : application.configuration().parameters())
            if (!info.read_only && !info.path.starts_with("cost."))
                tunable.push_back(std::move(info));
        for (auto& info : settings.parameters.parameters())
            if (!info.read_only)
                tunable.push_back(std::move(info));
        for (auto& requirement : application.configuration().requirements())
            if (requirement.path != "cost" && !requirement.path.starts_with("cost."))
                requirements.push_back(std::move(requirement));
        for (auto& requirement : settings.parameters.requirements())
            requirements.push_back(std::move(requirement));
    }
    // The set refers to the app, which moves into the session below.
    auto values = configuration.parameters();

    session_type session{std::move(application), command_line.seed};
    const auto names = session.runner_names();
    const std::string runner =
        command_line.runner.empty() ? std::string{names.front()} : command_line.runner;
    if (std::find(names.begin(), names.end(), runner) == names.end())
    {
        err << "unknown runner " << runner << "; the runners are:";
        for (const auto name : names)
            err << ' ' << name;
        err << '\n';
        return 2;
    }

    if (!tuning.irace.empty())
        return detail::write_irace(
            out,
            err,
            argc > 0 ? argv[0] : "program",
            command_line,
            tuning,
            settings,
            defaults,
            std::move(values),
            std::move(tunable),
            std::move(requirements),
            names);

    if (command_line.instance.empty())
    {
        err << "error: instance must be set\n";
        return 2;
    }

    if constexpr (!scalar_cost_available<
                      typename session_type::input_type,
                      typename session_type::cost_type>)
    {
        if (!tuning.print.empty())
        {
            err << "tuning.print: this cost is not one number; give the problem a "
                   "scalar_cost(input, cost)\n";
            return 2;
        }
    }

    try
    {
        session.load_input(command_line.instance);

        std::optional<typename session_type::cost_type> target;
        try
        {
            target =
                RunParameters{command_line.target}
                    .template target_cost<typename session_type::cost_type>(
                        session.input());
        }
        catch (const std::invalid_argument& error)
        {
            err << "error: " << error.what() << '\n';
            return 2;
        }

        if (!command_line.solution.empty())
        {
            if constexpr (session_type::supports_solution_loading)
            {
                session.load_solution(command_line.solution);
                if (!session.is_valid())
                {
                    err << "error: solution: " << command_line.solution.string()
                        << " is not valid for the Input\n";
                    return 2;
                }
            }
            else
            {
                err << "solution: this problem cannot read solutions\n";
                return 2;
            }
        }
        else if (command_line.start == "initial"
            || (command_line.start.empty() && !session_type::supports_random_solution))
        {
            if constexpr (session_type::supports_initial_solution)
                session.use_initial_solution();
            else
            {
                err << "start: this problem has no initial solution\n";
                return 2;
            }
        }
        else
        {
            if constexpr (session_type::supports_random_solution)
                session.use_random_solution(session.rng());
            else
            {
                err << "start: this problem has no random solutions\n";
                return 2;
            }
        }

        const auto begin = std::chrono::steady_clock::now();
        // The run's limits; blank text is no time limit.
        run_options<trace::null_tracer> limits{};
        if (const auto seconds = *command_line.timeout_seconds())
            limits = limits.timeout(*seconds);
        if (!command_line.max_evaluations.is_unlimited())
            limits = limits.max_evaluations(command_line.max_evaluations);
        const bool ran = target
            ? session.run(runner, limits.stop_at(std::move(*target)))
            : session.run(runner, limits);
        const std::chrono::duration<double> elapsed =
            std::chrono::steady_clock::now() - begin;
        if (!ran)
            return 1; // the name was checked above

        if constexpr (scalar_cost_available<
                          typename session_type::input_type,
                          typename session_type::cost_type>)
        {
            if (!tuning.print.empty())
            {
                out << easylocal::detail::number_text(
                    scalar_cost(session.input(), session.evaluate(), tuning.hard_weight));
                if (tuning.print == "cost_time")
                    out << ' ' << elapsed.count();
                out << '\n';
                return 0;
            }
        }

        out << "cost " << easylocal::detail::report_text(session.evaluate()) << "\ntime "
            << elapsed.count() << '\n';
        if (const auto& effort = session.last_run_effort())
        {
            out << "iterations " << effort->iterations << "\nevaluations "
                << effort->evaluations << "\ntermination "
                << to_string(effort->termination) << '\n';
        }
        if (command_line.report)
            detail::write_report(out, session);
        if (command_line.output.empty())
            detail::write_solution(out, session);
        else if constexpr (session_type::supports_solution_saving)
            session.save_solution(command_line.output);
        else
        {
            err << "output: this problem cannot write solutions\n";
            return 2;
        }
    }
    catch (const std::exception& error)
    {
        err << "error: " << error.what() << '\n';
        return 1;
    }
    catch (...)
    {
        err << "error: unknown exception\n";
        return 1;
    }
    return 0;
}

} // namespace easylocal::cli
