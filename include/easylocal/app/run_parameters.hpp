#pragma once

/// \file
/// The options of a run given as text, on the command line or in a file: a
/// target cost, read by the problem's read_cost(input, text) or else by
/// cost::from_text.

#include <easylocal/config/parameters.hpp>
#include <easylocal/cost/text.hpp>

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

/// The options of a run that a program reads with its configuration, for
/// example under "run": --run.target=0.
///
/// The target stays text until the Input is known, since a problem may read its
/// costs with read_cost(input, text).
struct RunParameters
{
    /// The cost at which a run stops, such as 0 or [0, 120]; empty: no target.
    std::string target;

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"target", &RunParameters::target>(
                "Stop a run when its solution reaches this cost, such as 0 or "
                "[0, 120]; empty: no target",
                easylocal::unlimited));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        if (const auto schema = config::check_schema(*this); !schema)
            return schema;
        return config::validation_result::success();
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
