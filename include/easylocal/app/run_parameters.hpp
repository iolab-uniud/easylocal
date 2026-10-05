#pragma once

/// \file
/// The limits of a run given as text, on the command line or in a file: a
/// target cost, read by the problem's read_cost(input, text) or else by
/// cost::from_text, a time limit and an evaluation budget.

#include <easylocal/config/domain.hpp>
#include <easylocal/config/parameters.hpp>
#include <easylocal/cost/text.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/trace/tracer.hpp>
#include <easylocal/utils/detail/number_text.hpp>
#include <easylocal/utils/detail/text.hpp>
#include <easylocal/utils/limit.hpp>

#include <cmath>
#include <concepts>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace easylocal
{

namespace detail::cost_text_adl
{

void read_cost() = delete;

// A problem's own textual form of its costs, found through its Input.
template<class Input, class Cost>
concept has_read_cost = requires(const Input& input, std::string_view text) {
    { read_cost(input, text) } -> std::convertible_to<Cost>;
};

template<class Cost, class Input>
    requires has_read_cost<Input, Cost>
[[nodiscard]]
Cost call_read_cost(const Input& input, const std::string_view text)
{
    return read_cost(input, text);
}

} // namespace detail::cost_text_adl

/// Costs of a problem that can be read as text: by its read_cost, or because
/// cost::from_text reads them.
template<class Input, class Cost>
concept readable_cost =
    detail::cost_text_adl::has_read_cost<Input, Cost> || cost::text_readable<Cost>;

/// A cost written as text, such as a target: by the problem's read_cost(input,
/// text) when it has one, else as cost::from_text reads it.
///
/// Throws std::invalid_argument (or what the problem's read_cost throws) when
/// the text is not a cost.
template<class Cost, class Input>
    requires readable_cost<Input, Cost>
[[nodiscard]]
Cost read_cost(const Input& input, const std::string_view text)
{
    if constexpr (detail::cost_text_adl::has_read_cost<Input, Cost>)
        return detail::cost_text_adl::call_read_cost<Cost>(input, text);
    else
        return cost::from_text<Cost>(text);
}

/// The limits of a run that a program reads with its configuration, for
/// example under "run": --run.target=0, --run.timeout=10,
/// --run.max_evaluations=5000.
///
/// cli::run reads the same limits as its own switches (--target, --timeout,
/// --max_evaluations). The target stays text until the Input is known, since a
/// problem may read its costs with read_cost(input, text); options() turns the
/// limits into run options.
struct RunParameters
{
    /// The cost at which a run stops, such as 0 or [0, 120]; empty: no target.
    std::string target{};
    /// The seconds a run may last, such as 10 or 2.5; empty: no time limit.
    std::string timeout{};
    /// The evaluations a run may make, the initial one included; unlimited: no
    /// budget beyond the runner's own.
    limit max_evaluations{unlimited};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"target", &RunParameters::target>(
                "Stop a run when its solution reaches this cost, such as 0 or "
                "[0, 120]; empty: no target",
                easylocal::unlimited),
            config::field<"timeout", &RunParameters::timeout>(
                "Stop a run after this many seconds, such as 10 or 2.5; empty: no "
                "limit",
                easylocal::unlimited),
            config::field<"max_evaluations", &RunParameters::max_evaluations>(
                "Stop a run after this many evaluations; unlimited: no budget "
                "beyond the runner's own",
                config::range(0, easylocal::unlimited)));
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
        if (detail::trim_space(timeout).empty())
            return std::optional<double>{};
        const auto seconds = detail::parse_number<double>(timeout);
        if (!seconds || !(*seconds >= 0.0) || !std::isfinite(*seconds))
            return std::nullopt;
        return std::optional<double>{*seconds};
    }

    /// The run options of these limits, added to base (such as with(control)):
    /// the target read with read_cost, the time limit and the evaluation
    /// budget, each when it is set.
    ///
    /// Throws std::invalid_argument, naming the field, when the target is not
    /// a cost or the timeout not a number of seconds.
    template<
        class Cost,
        class Input,
        class Tracer = trace::null_tracer,
        class Target = no_target>
        requires readable_cost<Input, Cost>
    [[nodiscard]]
    run_options<Tracer, Cost> options(
        const Input& input,
        const run_options<Tracer, Target>& base = {}) const
    {
        const auto seconds = timeout_seconds();
        if (!seconds)
        {
            throw std::invalid_argument{
                "timeout: '" + timeout + "' is not a non-negative number of seconds"};
        }
        auto result = base.without_target();
        if (*seconds)
            result = result.timeout(**seconds);
        if (!max_evaluations.is_unlimited())
            result = result.max_evaluations(max_evaluations);
        run_options<Tracer, Cost> options{
            .control = result.control,
            .tracer = result.tracer,
            .target = target_cost<Cost>(input),
            .time_limit = result.time_limit,
            .evaluation_limit = result.evaluation_limit,
        };
        return options;
    }

    /// The target as a cost of the problem, read with read_cost; empty when no
    /// target is set.
    ///
    /// Throws std::invalid_argument, naming the field, when the text is not a
    /// cost.
    template<class Cost, class Input>
        requires readable_cost<Input, Cost>
    [[nodiscard]]
    std::optional<Cost> target_cost(const Input& input) const
    {
        if (target.find_first_not_of(" \t") == std::string::npos)
            return std::nullopt;
        try
        {
            return read_cost<Cost>(input, target);
        }
        catch (const std::invalid_argument& error)
        {
            throw std::invalid_argument{"target: " + std::string{error.what()}};
        }
    }
};

} // namespace easylocal
