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
#include <easylocal/trace/binary.hpp>
#include <easylocal/trace/jsonl.hpp>
#include <easylocal/trace/tracer.hpp>
#include <easylocal/utils/detail/number_text.hpp>
#include <easylocal/utils/detail/text.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::cli
{

/// The command line of cli::run: --instance, --seed, --runner, --start,
/// --solution, --output, --target, --timeout, --max_evaluations, --report and
/// --trace.
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
    ///
    /// The points of a front go to numbered files next to it: best.txt gives
    /// best.1.txt, best.2.txt...
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
    /// The file to record the trace of the run to, with timestamps: JSON Lines
    /// when its extension is .jsonl, ELTR otherwise; empty: no trace.
    std::filesystem::path trace{};

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
                "Solution file, with the points of a front in numbered files next "
                "to it (empty: standard output)",
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
                "Print the value of each cost component, and its description"),
            config::field<"trace", &parameters::trace>(
                "Record the trace of the run to this file, JSON Lines for a .jsonl "
                "name, ELTR otherwise (empty: no trace)",
                easylocal::unlimited));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const
    {
        if (const auto schema = config::check_schema(*this); !schema)
            return schema;
        return run_parameters().validate();
    }

    /// The limits of the run, target, timeout and max_evaluations, as the block
    /// a program reads under a prefix of its own.
    [[nodiscard]]
    RunParameters run_parameters() const
    {
        return {
            .target = target,
            .timeout = timeout,
            .max_evaluations = max_evaluations,
        };
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

// Whether Event carries no cost, or a cost of type Cost.
template<class Event, class Cost>
inline constexpr bool event_of_cost = true;

template<template<class> class Event, class EventCost, class Cost>
inline constexpr bool event_of_cost<Event<EventCost>, Cost> =
    std::same_as<EventCost, Cost>;

// The events of a run that a recorder of the app's cost receives: those
// without a cost and those of that cost. A pipeline stage on another cost,
// such as an until_feasible() stage on the hard cost, sends only its
// run_context.
template<class Recorder, class Cost>
class cost_filter
{
public:
    explicit cost_filter(Recorder& recorder) noexcept : recorder_{recorder} {}

    template<class Event>
    static constexpr bool observes =
        trace::observes<Recorder, Event> && event_of_cost<Event, Cost>
        && requires(Recorder& recorder, const Event& value) { recorder.emit(value); };

    template<class Event>
        requires observes<Event>
    void emit(const Event& value)
    {
        recorder_.emit(value);
    }

private:
    Recorder& recorder_;
};

// Whether a cost has the default JSON writer of the JSONL recorder.
template<class Cost>
inline constexpr bool json_cost_writer_available =
    trace::json_cost_writer_for<trace::default_json_cost_writer, Cost>;

// --trace: runs run_with(&tracer) with a recorder of the app's cost writing to
// path, JSON Lines for a .jsonl name and ELTR otherwise, with timestamps and
// metadata; ran is what run_with returns. The exit status: 0 once the trace is
// written, 1 when the file cannot be written, 2 when the cost has no default
// encoding in that format.
template<class Cost, class Run>
int run_traced(
    const std::filesystem::path& path,
    std::vector<std::pair<std::string, std::string>> metadata,
    std::ostream& err,
    const Run& run_with,
    bool& ran)
{
    const bool jsonl = path.extension() == ".jsonl";
    if constexpr (!json_cost_writer_available<Cost>)
    {
        if (jsonl)
        {
            err << "trace: this cost has no JSON encoding: trace to an ELTR file\n";
            return 2;
        }
    }
    if constexpr (!trace::detail::default_binary_cost<Cost>::value)
    {
        if (!jsonl)
        {
            err << "trace: this cost has no ELTR encoding: trace to a .jsonl file\n";
            return 2;
        }
    }
    std::ofstream file{path, std::ios::binary};
    if (!file)
    {
        err << "error: trace: cannot write " << path.string() << '\n';
        return 1;
    }
    const auto record = [&](auto& recorder) {
        cost_filter<std::remove_cvref_t<decltype(recorder)>, Cost> filter{recorder};
        ran = run_with(&filter);
        recorder.flush();
        if (!recorder.good())
        {
            err << "error: trace: cannot write " << path.string() << '\n';
            return 1;
        }
        return 0;
    };
    if (jsonl)
    {
        if constexpr (json_cost_writer_available<Cost>)
        {
            trace::jsonl_recorder<Cost> recorder{
                file,
                {.metadata = std::move(metadata), .timestamps = true}};
            return record(recorder);
        }
    }
    else if constexpr (trace::detail::default_binary_cost<Cost>::value)
    {
        trace::binary_recorder<Cost> recorder{
            file,
            {.metadata = std::move(metadata), .timestamps = true}};
        return record(recorder);
    }
    return 2; // the cost was checked above
}

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
void write_solution(
    std::ostream& out,
    const Session& session,
    const typename Session::solution_type& solution)
{
    if constexpr (Session::supports_solution_saving)
        easylocal::write_solution(session.input(), solution, out);
    else
        out << easylocal::describe(solution) << '\n';
}

// The file of the point of the front numbered index, from 1, next to the
// output file: best.txt gives best.1.txt, best.2.txt...
inline std::filesystem::path front_file(
    const std::filesystem::path& output,
    const std::size_t index)
{
    auto file = output;
    file.replace_filename(
        output.stem().string() + '.' + std::to_string(index)
        + output.extension().string());
    return file;
}

// The front of the last run, when it has one: a line `front <size>`, then for
// each point a line `point <index> cost <cost>` followed by its solution, or,
// with an output file, the solutions saved to the front_file()s.
template<class Session>
void write_front(
    std::ostream& out,
    const Session& session,
    const std::filesystem::path& output)
{
    const auto& front = session.last_run_front();
    if (front.empty())
        return;
    out << "front " << front.size() << '\n';
    std::size_t index = 0;
    for (const auto& point : front)
    {
        ++index;
        out << "point " << index << " cost " << easylocal::detail::report_text(point.cost)
            << '\n';
        if (output.empty())
            write_solution(out, session, point.solution);
        else if constexpr (Session::supports_solution_saving)
            easylocal::save_solution(
                session.input(),
                point.solution,
                front_file(output, index));
    }
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

/// Runs application as a program, from the command line to the solution.
///
/// It parses argc and argv (and a --config file), loads the Input, starts
/// from a random, initial or loaded solution, runs the chosen runner and
/// prints "cost", "time" (seconds), the effort of the run ("iterations",
/// "evaluations", "termination") when the algorithm reports it, with --report
/// the value of each cost component, and the solution, or saves it to
/// --output. After a run with a cost::pareto cost it prints "front" and
/// its size, then each point, "point", its number and "cost", followed by its
/// solution, or saves the solutions to numbered files next to --output.
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

    // Tuning compares runs by their cost as one number: a scenario whose every
    // run would fail is not written.
    if constexpr (!scalar_cost_available<
                      typename session_type::input_type,
                      typename session_type::cost_type>)
    {
        if (!tuning.irace.empty() || !tuning.print.empty())
        {
            err << (tuning.irace.empty() ? "tuning.print" : "tuning.irace")
                << ": this cost is not one number; give the problem a "
                   "scalar_cost(input, cost)\n";
            return 2;
        }
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

    try
    {
        session.load_input(command_line.instance);

        // The run's limits; blank text is no target and no time limit.
        run_options<trace::null_tracer, typename session_type::cost_type> limits{};
        try
        {
            limits =
                command_line.run_parameters()
                    .template options<typename session_type::cost_type>(session.input());
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
        // The run, with the limits, the target and a tracer.
        const auto run_with = [&]<class Tracer>(Tracer* tracer) {
            const run_options<Tracer, typename session_type::cost_type> options{
                .control = limits.control,
                .tracer = tracer,
                .target = limits.target,
                .time_limit = limits.time_limit,
                .evaluation_limit = limits.evaluation_limit,
                .front = limits.front,
            };
            return session.run(runner, options);
        };
        bool ran = false;
        if (command_line.trace.empty())
            ran = run_with(static_cast<trace::null_tracer*>(nullptr));
        else
        {
            // What tells the run apart: the program and its parameters, with
            // the runner that runs.
            std::vector<std::pair<std::string, std::string>> metadata{
                {"program", argc > 0 ? argv[0] : "program"}};
            for (const auto& info : values)
                if (!info.path.starts_with("tuning."))
                    metadata.emplace_back(
                        info.path,
                        info.path == "runner" ? runner : info.value);
            if (const auto status = detail::run_traced<typename session_type::cost_type>(
                    command_line.trace,
                    std::move(metadata),
                    err,
                    run_with,
                    ran);
                status != 0)
            {
                return status;
            }
        }
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
            detail::write_solution(out, session, session.solution());
        else if constexpr (session_type::supports_solution_saving)
            session.save_solution(command_line.output);
        else
        {
            err << "output: this problem cannot write solutions\n";
            return 2;
        }
        detail::write_front(out, session, command_line.output);
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
