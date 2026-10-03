#pragma once

// cli::run: an app as a command-line program. It reads the Input, the seed,
// the runner and the app's parameters from the command line and a
// configuration file, runs the runner by name on a Session, and prints the
// cost and the solution: the batch counterpart of the TextUI and the REST
// service.

#include <easylocal/app/io.hpp>
#include <easylocal/app/run_parameters.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/config/cli.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/config/parameters.hpp>
#include <easylocal/config/setup.hpp>
#include <easylocal/cost/text.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace easylocal::cli
{

// The command line of cli::run: --instance, --seed, --runner, --start,
// --solution, --output, --target and --report, next to the app's parameters
// (--runners.<name>.*, --cost.*, --neighborhood.*).
struct parameters
{
    // Every field has an initializer, so that designated initializers, as in
    // options::defaults, may name only some of them.
    std::filesystem::path instance{};
    std::uint64_t seed{0};
    std::string runner{};
    std::string start{};
    std::filesystem::path solution{};
    std::filesystem::path output{};
    std::string target{};
    bool report{false};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"instance", &parameters::instance>("Input file"),
            config::field<"seed", &parameters::seed>("Seed of the random generator"),
            config::field<"runner", &parameters::runner>(
                "Name of the runner (empty: the first registered)"),
            config::field<"start", &parameters::start>(
                "Starting solution: random or initial (empty: random when the "
                "problem has random solutions)"),
            config::field<"solution", &parameters::solution>(
                "Starting solution read from this file, instead of start"),
            config::field<"output", &parameters::output>(
                "Solution file (empty: standard output)"),
            config::field<"target", &parameters::target>(
                "Stop when the solution reaches this cost, such as 0 or "
                "[0, 120] (empty: no target)"),
            config::field<"report", &parameters::report>(
                "Print the value of each cost component, and its description"));
    }

    [[nodiscard]]
    config::validation_result validate() const
    {
        if (instance.empty())
            return config::validation_result::failure("instance must be set");
        if (!start.empty() && start != "random" && start != "initial")
            return config::validation_result::failure("start must be random or initial");
        return config::validation_result::success();
    }
};

// What cli::run adds to the command line and where it writes.
struct options
{
    // The values of the switches before the command line, such as an
    // instance or a runner to use when none is given.
    cli::parameters defaults{};

    // The program's own parameters, parsed with the others; they refer to
    // blocks that must outlive the call.
    config::parameter_set parameters{};
    std::ostream* out{&std::cout};
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

} // namespace detail

// Runs application as a program: parses argc and argv (and a --config file),
// loads the Input, starts from a random, initial or loaded solution, runs the
// chosen runner and prints "cost", "time" (seconds), the effort of the run
// ("iterations", "evaluations", "termination") when the algorithm reports it,
// with --report the value of each cost component, and the solution, or saves
// it to --output. Returns the exit
// status: 0 on success, 1 when the run fails (an unreadable file, for example), 2 for an
// invalid command line.
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
    config::parameter_set configuration;
    configuration.add(command_line);
    configuration.add(application.configuration());
    configuration.add(settings.parameters);

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

    try
    {
        session.load_input(command_line.instance);

        if (!command_line.solution.empty())
        {
            if constexpr (session_type::supports_solution_loading)
                session.load_solution(command_line.solution);
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
        // Blank text is no target, as RunParameters reads it.
        const bool ran = command_line.target.find_first_not_of(" \t") == std::string::npos
            ? session.run(runner)
            : session.run(runner, stop_at(session.read_cost(command_line.target)));
        const std::chrono::duration<double> elapsed =
            std::chrono::steady_clock::now() - begin;
        if (!ran)
            return 1; // the name was checked above

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
    return 0;
}

} // namespace easylocal::cli
