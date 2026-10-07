#pragma once

/// \file
/// Automatic configuration of a program's parameters with irace: the program's
/// `tuning.*` switches, its cost as one number (scalar_cost), and the files of
/// an irace scenario written from its parameter set (write_irace_stub).
///
/// cli::run reads the switches: `--tuning.irace=DIR` writes the stub,
/// `--tuning.print=cost` runs once and prints only the cost.

#include <easylocal/config/domain.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/config/parameters.hpp>
#include <easylocal/cost/scalar.hpp>
#include <easylocal/utils/detail/number_text.hpp>
#include <easylocal/utils/detail/text.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <istream>
#include <limits>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace easylocal
{

/// The parameters of a program for automatic configurators, under "tuning":
/// --tuning.irace=DIR, --tuning.print=cost and --tuning.hard_weight.
struct TuningParameters
{
    /// The directory where the files of an irace scenario are written; empty:
    /// none.
    std::filesystem::path irace{};
    /// What a run prints: empty for the usual report, "cost" for the cost as
    /// one number, "cost_time" for the cost and the running time in seconds.
    std::string print{};
    /// The weight of a hard cost over a soft one, and of each lexicographic
    /// value over the next, when a cost is turned into one number; it must
    /// exceed every soft cost, and the number must stay below 2^53, where a
    /// double still holds the lower levels exactly.
    double hard_weight{1e9};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"irace", &TuningParameters::irace>(
                "Write the files of an irace scenario to this directory, and exit",
                easylocal::unlimited),
            config::field<"print", &TuningParameters::print>(
                "Print only the cost as one number (cost), or the cost and the "
                "running time (cost_time); empty: the usual report",
                config::one_of("", "cost", "cost_time")),
            config::field<"hard_weight", &TuningParameters::hard_weight>(
                "Weight of the hard cost over the soft one in the printed number",
                config::range(0.0, easylocal::unlimited).open_low()));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        if (const auto schema = config::check_schema(*this); !schema)
            return schema;
        // Its domain has no upper bound: it lets infinity through.
        if (!std::isfinite(hard_weight))
            return config::validation_result::failure("hard_weight must be finite");
        return config::validation_result::success();
    }
};

/// The values worth trying for a parameter: a domain, narrower than the one its
/// schema declares, given by a program for tuning, e.g.
/// `{"runners.sa.temperature.initial_temperature", config::range(1.0,
/// 1000.0).log()}`.
struct tuning_range
{
    /// The full path of the parameter.
    std::string path;
    /// The values to try.
    config::domain_info domain;

    /// From the path of a parameter and a domain (config::range,
    /// config::one_of).
    template<class Domain>
        requires config::is_domain_v<Domain>
    tuning_range(std::string parameter_path, const Domain& values)
        : path{std::move(parameter_path)}, domain{config::describe_domain(values)}
    {
    }
};

namespace detail::scalar_cost_adl
{

void scalar_cost() = delete;

// A problem's own reduction of its costs to one number, found through its
// Input.
template<class Input, class Cost>
concept has_scalar_cost = requires(const Input& input, const Cost& cost) {
    { scalar_cost(input, cost) } -> std::convertible_to<double>;
};

template<class Input, class Cost>
    requires has_scalar_cost<Input, Cost>
[[nodiscard]]
double call_scalar_cost(const Input& input, const Cost& cost)
{
    return static_cast<double>(scalar_cost(input, cost));
}

} // namespace detail::scalar_cost_adl

/// Costs of a problem that can be turned into one number: by its
/// scalar_cost(input, cost), or because cost::scalar converts them.
template<class Input, class Cost>
concept scalar_cost_available = detail::scalar_cost_adl::has_scalar_cost<Input, Cost>
    || cost::scalar_convertible<Cost>;

/// A cost as one number, as automatic configurators compare runs: by the
/// problem's scalar_cost(input, cost) when it has one, else as cost::scalar
/// with hard_weight.
template<class Input, class Cost>
    requires scalar_cost_available<Input, Cost>
[[nodiscard]]
double scalar_cost(const Input& input, const Cost& value, const double hard_weight)
{
    if constexpr (detail::scalar_cost_adl::has_scalar_cost<Input, Cost>)
        return detail::scalar_cost_adl::call_scalar_cost(input, value);
    else
        return cost::scalar(value, hard_weight);
}

/// What write_irace_stub writes: the program, its parameters and the values to
/// try.
struct irace_stub
{
    /// The directory of the scenario; it is created if needed.
    std::filesystem::path directory;
    /// The program that irace runs, preferably as an absolute path.
    std::filesystem::path program;
    /// The parameters that may be tuned; write_irace_stub leaves out those of
    /// the cost (`cost.*`, which would change what is compared) and the
    /// read-only ones.
    std::vector<config::parameter_info> parameters;
    /// The values to try, by path, instead of the domains of the schema.
    std::vector<tuning_range> ranges;
    /// The requirements between the parameters, written as irace's forbidden
    /// combinations when they refer to tuned parameters; those of the cost
    /// are left out.
    std::vector<config::requirement_info> requirements;
    /// The runners irace chooses among, each with its parameters
    /// (`runners.<name>.*`); with one runner there is no choice.
    std::vector<std::string> runners;
    /// The overrides every run starts from, written to fixed.conf: the values
    /// changed on the command line when the stub was written.
    std::vector<config::owned_text_override> fixed;
    /// An instance for instances.txt; empty: none.
    std::filesystem::path instance;
};

/// The outcome of write_irace_stub: the files written and kept, and the errors.
///
/// It converts to true when there is no error.
struct irace_stub_result
{
    /// The files written.
    std::vector<std::filesystem::path> written;
    /// The files that existed and were kept as they are.
    std::vector<std::filesystem::path> kept;
    /// configurations.txt, rewritten each time from parameters.txt as it is;
    /// empty when it could not be.
    std::filesystem::path configurations;
    /// The parameters tuned: those parameters.txt declares.
    std::size_t tuned{};
    /// The parameters left for the user to complete, commented out, when
    /// parameters.txt is written.
    std::size_t to_complete{};
    /// The values of configurations.txt moved into the ranges of
    /// parameters.txt, as `path = value`.
    std::vector<std::string> moved;
    /// The errors: a range for an unknown parameter or outside its domain, a
    /// parameter of parameters.txt that the program does not have, a file that
    /// cannot be written.
    std::vector<std::string> errors;

    /// Whether there are no errors.
    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return errors.empty();
    }
};

namespace detail
{

// irace samples a real range with "digits" digits after the point, 4 by
// default: a scenario uses more when a bound needs them (at most 15), and an
// open bound moves inward by one step of 10^-digits.
inline constexpr int irace_default_digits = 4;
inline constexpr int irace_max_digits = 15;

[[nodiscard]]
inline double irace_step(const int digits)
{
    return std::pow(10.0, -digits);
}

[[nodiscard]]
inline double round_to_irace_step(const double value, const int digits)
{
    const double scale = std::pow(10.0, digits);
    return std::round(value * scale) / scale;
}

// The digits after the point that write value exactly, as the shortest fixed
// notation reads back; at most irace_max_digits.
[[nodiscard]]
inline int decimals_of(const double value)
{
    if (!std::isfinite(value))
        return 0;
    for (int digits = 0; digits < irace_max_digits; ++digits)
    {
        std::array<char, 400> buffer{};
        const auto [end, error] = std::to_chars(
            buffer.data(),
            buffer.data() + buffer.size(),
            value,
            std::chars_format::fixed,
            digits);
        static_cast<void>(error); // 400 characters hold any double
        double back{};
        std::from_chars(buffer.data(), end, back);
        if (back == value)
            return digits;
    }
    return irace_max_digits;
}

// A bound of a real range in fixed notation, 0.0001 rather than 1e-04, for a
// file the user reads and edits.
[[nodiscard]]
inline std::string irace_number(const double value)
{
    std::array<char, 400> buffer{};
    const auto [end, error] = std::to_chars(
        buffer.data(),
        buffer.data() + buffer.size(),
        value,
        std::chars_format::fixed);
    static_cast<void>(error); // 400 characters hold any double in fixed notation
    return std::string{buffer.data(), end};
}

// One line of parameters.txt.
struct irace_parameter
{
    std::string name; // the irace identifier
    std::string path; // the program's parameter; empty for the runner
    char type{};      // c, o, i, r
    bool logarithmic{};
    std::vector<std::string> values; // two bounds, or the choices
    std::string condition;
    std::string default_value;
    bool active{};
    bool off{}; // its condition is false with the values that are not tuned
    std::string note;
    std::string_view description;
};

// The irace identifiers of the parameters, by path: irace names a parameter
// with letters, digits, '.' and '_', so every other character becomes '_',
// and an identifier taken already (by another path, or "runner", the choice
// of the runner) gets a numeric suffix. The switch stays the path.
[[nodiscard]]
inline std::map<std::string, std::string, std::less<>> irace_identifiers(
    const std::vector<config::parameter_info>& infos)
{
    std::map<std::string, std::string, std::less<>> identifiers;
    std::set<std::string, std::less<>> taken{"runner"};
    for (const auto& info : infos)
    {
        std::string base = info.path;
        for (auto& character : base)
        {
            const bool kept = (character >= 'a' && character <= 'z')
                || (character >= 'A' && character <= 'Z')
                || (character >= '0' && character <= '9') || character == '.'
                || character == '_';
            if (!kept)
                character = '_';
        }
        auto identifier = base;
        for (std::size_t suffix = 2; taken.contains(identifier); ++suffix)
            identifier = base + '_' + easylocal::detail::number_text(suffix);
        taken.insert(identifier);
        identifiers.emplace(info.path, std::move(identifier));
    }
    return identifiers;
}

// The identifier of path; the path itself when it is not a parameter.
[[nodiscard]]
inline std::string irace_identifier_of(
    const std::string_view path,
    const std::map<std::string, std::string, std::less<>>& identifiers)
{
    const auto found = identifiers.find(path);
    return found != identifiers.end() ? found->second : std::string{path};
}

[[nodiscard]]
inline std::string irace_domain_text(const irace_parameter& parameter)
{
    std::string result{"("};
    for (std::size_t index = 0; index < parameter.values.size(); ++index)
    {
        if (index != 0)
            result += ", ";
        if (parameter.type == 'c')
            result += '"' + parameter.values[index] + '"';
        else
            result += parameter.values[index];
    }
    return result + ')';
}

[[nodiscard]]
inline std::string irace_line(const irace_parameter& parameter)
{
    std::string type{parameter.type};
    if (parameter.logarithmic)
        type += ",log";
    const auto& path = parameter.path.empty() ? parameter.name : parameter.path;
    std::string line = parameter.name + " \"--" + path + "=\" " + type + ' '
        + irace_domain_text(parameter);
    if (!parameter.condition.empty())
        line += " | " + parameter.condition;
    return line;
}

[[nodiscard]]
inline bool numeric_kind(const config::parameter_kind kind) noexcept
{
    return kind == config::parameter_kind::integer || kind == config::parameter_kind::real
        || kind == config::parameter_kind::limit;
}

// The type and the values of a parameter with a domain, as irace reads them.
inline void set_irace_domain(
    irace_parameter& parameter,
    const config::parameter_kind kind,
    const config::domain_info& domain,
    const int digits)
{
    if (domain.kind == config::domain_info::shape::choice)
    {
        parameter.type = numeric_kind(kind) ? 'o' : 'c';
        parameter.values = domain.choices;
        return;
    }
    if (kind == config::parameter_kind::real)
    {
        parameter.type = 'r';
        const double low = domain.low_open
            ? round_to_irace_step(domain.low + irace_step(digits), digits)
            : domain.low;
        const double high = domain.high_open
            ? round_to_irace_step(domain.high - irace_step(digits), digits)
            : domain.high;
        parameter.values = {irace_number(low), irace_number(high)};
    }
    else
    {
        // The nearest integers inside the range: above an open lower bound,
        // fractional or not, and below an open upper one.
        parameter.type = 'i';
        const auto low = domain.low_open
            ? static_cast<long long>(std::floor(domain.low)) + 1
            : static_cast<long long>(std::ceil(domain.low));
        const auto high = domain.high_open
            ? static_cast<long long>(std::ceil(domain.high)) - 1
            : static_cast<long long>(std::floor(domain.high));
        parameter.values = {
            easylocal::detail::number_text(low),
            easylocal::detail::number_text(high)};
    }
    parameter.logarithmic = domain.logarithmic;
}

// A range to start from for a parameter without a domain: a factor of ten
// around a positive default, on a logarithmic scale.
inline void suggest_irace_domain(
    irace_parameter& parameter,
    const config::parameter_info& info,
    const config::domain_info* bound,
    const int digits)
{
    // Read as the program writes it, whatever the locale.
    const double value =
        easylocal::detail::parse_number<double>(info.value).value_or(0.0);
    if (info.kind == config::parameter_kind::boolean)
    {
        parameter.type = 'c';
        parameter.values = {"true", "false"};
        parameter.note = "a switch: uncomment it to tune it";
        return;
    }
    // The lower bound of a range with no upper one, which a range to start
    // from keeps.
    const bool bounded_below = bound && bound->kind == config::domain_info::shape::range;
    if (!(value > 0.0))
    {
        parameter.type = info.kind == config::parameter_kind::real ? 'r' : 'i';
        parameter.values = {bounded_below ? bound->low_text : "LOW", "HIGH"};
        parameter.note = "no finite domain: give its range";
        return;
    }
    config::domain_info suggested;
    suggested.kind = config::domain_info::shape::range;
    suggested.low = value / 10.0;
    suggested.high = value * 10.0;
    suggested.logarithmic = true;
    if (info.kind != config::parameter_kind::real)
        suggested.low = (std::max)(1.0, suggested.low);
    if (bounded_below && suggested.low <= bound->low)
    {
        suggested.low = bound->low;
        suggested.low_open = bound->low_open;
        suggested.logarithmic = bound->low > 0.0;
    }
    set_irace_domain(parameter, info.kind, suggested, digits);
    parameter.note = "no finite domain: a range around the default to start from";
}

// A parameter as parameters.txt declares it: what configurations.txt must
// agree with.
struct declared_irace_parameter
{
    std::string name;
    std::string path; // what its switch, "--<path>=", sets
    char type{};
    std::vector<std::string> values;
    std::string condition;
};

// The parameters a parameters.txt declares, before its [global] or
// [forbidden] sections; lines that are not `name "switch" type (values)` are
// skipped.
[[nodiscard]]
inline std::vector<declared_irace_parameter> read_irace_parameters(std::istream& in)
{
    std::vector<declared_irace_parameter> result;
    std::string line;
    while (std::getline(in, line))
    {
        auto text = easylocal::detail::trim_space(line);
        if (text.starts_with('['))
            break;
        if (const auto comment = text.find('#'); comment != std::string_view::npos)
            text = easylocal::detail::trim_space(text.substr(0, comment));
        const auto name_end = text.find_first_of(" \t");
        const auto open = text.find('(');
        const auto close = text.find(')', open == std::string_view::npos ? 0 : open);
        if (text.empty() || name_end == std::string_view::npos
            || open == std::string_view::npos || close == std::string_view::npos)
            continue;
        const auto switch_end = text.find('"', text.find('"', name_end) + 1);
        if (switch_end == std::string_view::npos || switch_end > open)
            continue;
        // The type, such as i or c, between the switch and the values.
        const auto type = easylocal::detail::trim_space(
            text.substr(switch_end + 1, open - switch_end - 1));
        if (type.empty())
            continue;
        declared_irace_parameter parameter;
        parameter.name = std::string{text.substr(0, name_end)};
        const auto switch_begin = text.find('"', name_end);
        auto switch_text = text.substr(switch_begin + 1, switch_end - switch_begin - 1);
        if (switch_text.starts_with("--") && switch_text.ends_with('='))
            parameter.path = std::string{switch_text.substr(2, switch_text.size() - 3)};
        parameter.type = type.front();
        auto values = text.substr(open + 1, close - open - 1);
        while (!values.empty())
        {
            const auto comma = values.find(',');
            auto value = easylocal::detail::trim_space(values.substr(0, comma));
            if (value.size() >= 2 && (value.front() == '"' || value.front() == '\''))
                value = value.substr(1, value.size() - 2);
            parameter.values.emplace_back(value);
            if (comma == std::string_view::npos)
                break;
            values.remove_prefix(comma + 1);
        }
        if (const auto bar = text.find('|', close); bar != std::string_view::npos)
            parameter.condition =
                std::string{easylocal::detail::trim_space(text.substr(bar + 1))};
        result.push_back(std::move(parameter));
    }
    return result;
}

// Whether a path is the cost's, `cost` or `cost.*`: it changes what irace
// compares, so it is not tuned.
[[nodiscard]]
inline bool cost_path(const std::string_view path) noexcept
{
    return path == "cost" || path.starts_with("cost.");
}

// A value moved into the values of a parameter, so that a first configuration
// is valid however the user changed the ranges: the nearest bound, or the
// first choice. The second member says whether it moved.
[[nodiscard]]
inline std::pair<std::string, bool> clamp_irace_value(
    const declared_irace_parameter& parameter,
    const std::string& value)
{
    if (parameter.values.empty())
        return {value, false};
    if (parameter.type != 'i' && parameter.type != 'r')
    {
        if (std::ranges::find(parameter.values, value) != parameter.values.end())
            return {value, false};
        return {parameter.values.front(), true};
    }
    // The numbers as the program and irace write them, whatever the locale.
    const auto low = easylocal::detail::parse_number<double>(parameter.values.front());
    const auto high = easylocal::detail::parse_number<double>(parameter.values.back());
    if (!low || !high)
        return {value, false}; // bounds that are not numbers: irace says so
    // An unlimited limit is above every bound: it moves to the upper one.
    const auto parsed = value == "unlimited"
        ? std::optional<double>{std::numeric_limits<double>::infinity()}
        : easylocal::detail::parse_number<double>(value);
    if (parsed && *low <= *parsed && *parsed <= *high)
        return {value, false};
    const double number = std::clamp(parsed.value_or(*low), *low, *high);
    if (parameter.type == 'i')
        return {easylocal::detail::number_text(std::llround(number)), true};
    return {irace_number(number), true};
}

[[nodiscard]]
inline std::string runner_of(const std::string_view path)
{
    constexpr std::string_view prefix{"runners."};
    if (!path.starts_with(prefix))
        return {};
    const auto rest = path.substr(prefix.size());
    return std::string{rest.substr(0, rest.find('.'))};
}

// The digits after the point of the scenario: irace's 4, or more for the
// bounds of a real range (its own or its tuning range) that need them.
[[nodiscard]]
inline int irace_digits(const irace_stub& stub)
{
    int digits = irace_default_digits;
    for (const auto& info : stub.parameters)
    {
        if (info.kind != config::parameter_kind::real)
            continue;
        const auto range = std::ranges::find(stub.ranges, info.path, &tuning_range::path);
        const auto& domain = range != stub.ranges.end() ? range->domain : info.domain;
        if (domain.kind != config::domain_info::shape::range || domain.high_unlimited)
            continue;
        digits = (std::max)({digits, decimals_of(domain.low), decimals_of(domain.high)});
    }
    return (std::min)(digits, irace_max_digits);
}

[[nodiscard]]
inline std::vector<irace_parameter> irace_parameters(
    const irace_stub& stub,
    const int digits,
    std::vector<std::string>& errors)
{
    const auto identifiers = irace_identifiers(stub.parameters);
    std::vector<irace_parameter> result;
    for (const auto& range : stub.ranges)
    {
        const bool known = std::ranges::any_of(stub.parameters, [&](const auto& info) {
            return info.path == range.path;
        });
        if (!known)
            errors.push_back("tuning range for an unknown parameter: " + range.path);
    }

    if (stub.runners.size() > 1)
    {
        irace_parameter runner;
        runner.name = "runner";
        runner.type = 'c';
        runner.values = stub.runners;
        runner.default_value = stub.runners.front();
        runner.active = true;
        runner.description = "The runner";
        result.push_back(std::move(runner));
    }

    for (const auto& info : stub.parameters)
    {
        // The cost's parameters would change what irace compares; a read-only
        // parameter cannot be set.
        if (info.read_only || cost_path(info.path))
            continue;
        const auto owner = runner_of(info.path);
        if (!owner.empty()
            && std::ranges::find(stub.runners, owner) == stub.runners.end())
            continue; // a runner that is not tuned
        irace_parameter parameter;
        parameter.name = irace_identifier_of(info.path, identifiers);
        parameter.path = info.path;
        parameter.default_value = info.value;
        parameter.description = info.description;
        if (!owner.empty() && stub.runners.size() > 1)
            parameter.condition = "runner == \"" + owner + '"';

        const auto range = std::ranges::find(stub.ranges, info.path, &tuning_range::path);
        const config::domain_info* domain = info.domain ? &info.domain : nullptr;
        if (range != stub.ranges.end())
        {
            if (range->domain.kind == config::domain_info::shape::unbounded
                || range->domain.high_unlimited)
            {
                errors.push_back(
                    "tuning range " + range->domain.text() + " of " + info.path
                    + " is not finite");
                continue;
            }
            if (!info.domain.contains(range->domain))
            {
                errors.push_back(
                    "tuning range " + range->domain.text() + " of " + info.path
                    + " is outside its domain " + info.domain.text());
                continue;
            }
            domain = &range->domain;
        }

        const bool tunable_kind = numeric_kind(info.kind)
            || info.kind == config::parameter_kind::boolean
            || (domain && domain->kind == config::domain_info::shape::choice);
        if (!tunable_kind)
        {
            parameter.note = "not tunable by irace (a list, a path or text)";
            result.push_back(std::move(parameter));
            continue;
        }
        // A domain irace can sample: finite, of a kind it types.
        const bool finite = domain
            && domain->kind != config::domain_info::shape::unbounded
            && !domain->high_unlimited
            && !(
                domain->kind == config::domain_info::shape::range
                && info.kind == config::parameter_kind::boolean);
        if (info.kind == config::parameter_kind::limit && info.value == "unlimited"
            && !finite)
        {
            parameter.note = "unlimited: give it a finite range to tune it";
            result.push_back(std::move(parameter));
            continue;
        }
        if (finite)
        {
            set_irace_domain(parameter, info.kind, *domain, digits);
            parameter.active = true;
        }
        else
            suggest_irace_domain(parameter, info, domain, digits);
        result.push_back(std::move(parameter));
    }
    return result;
}

// A parameter in an irace expression: its name when irace tunes it, else its
// value as a constant.
[[nodiscard]]
inline std::string irace_reference(
    const std::string_view path,
    const std::vector<irace_parameter>& parameters,
    const std::vector<config::parameter_info>& infos)
{
    const auto info = std::ranges::find(infos, path, &config::parameter_info::path);
    const bool boolean =
        info != infos.end() && info->kind == config::parameter_kind::boolean;
    const auto tuned = std::ranges::find_if(parameters, [&](const auto& parameter) {
        return parameter.active && parameter.path == path;
    });
    if (tuned != parameters.end())
        return boolean ? "(" + tuned->name + " == \"true\")" : tuned->name;
    if (info == infos.end())
        return "NA";
    if (boolean)
        return info->value == "true" ? "TRUE" : "FALSE";
    if (info->kind == config::parameter_kind::limit && info->value == "unlimited")
        return "Inf";
    if (numeric_kind(info->kind))
        return info->value;
    return '"' + info->value + '"';
}

// The same, as if every parameter it names were tuned: for the lines the user
// may uncomment.
[[nodiscard]]
inline std::string irace_name(
    const std::string_view path,
    const std::vector<config::parameter_info>& infos,
    const std::map<std::string, std::string, std::less<>>& identifiers)
{
    const auto info = std::ranges::find(infos, path, &config::parameter_info::path);
    const auto identifier = irace_identifier_of(path, identifiers);
    if (info != infos.end() && info->kind == config::parameter_kind::boolean)
        return "(" + identifier + " == \"true\")";
    return identifier;
}

[[nodiscard]]
inline bool refers_to_tuned(
    const config::expression_info& expression,
    const std::vector<irace_parameter>& parameters)
{
    return std::ranges::any_of(expression.references(), [&](const std::string& path) {
        return std::ranges::any_of(parameters, [&](const auto& parameter) {
            return parameter.active && parameter.path == path;
        });
    });
}

// The conditions of the parameters, as irace conditions when they refer to
// tuned parameters, else decided now: a parameter whose condition is false is
// left out. A line left commented out keeps its condition with the names of
// the parameters, for when the user uncomments them.
inline void apply_irace_conditions(
    std::vector<irace_parameter>& parameters,
    const std::vector<config::parameter_info>& infos)
{
    const auto identifiers = irace_identifiers(infos);
    for (auto& parameter : parameters)
    {
        if (parameter.path.empty())
            continue; // the runner
        const auto info =
            std::ranges::find(infos, parameter.path, &config::parameter_info::path);
        if (info == infos.end() || !info->condition)
            continue;
        const auto named = info->condition->text_with([&](const std::string_view path) {
            return irace_name(path, infos, identifiers);
        });
        if (!refers_to_tuned(*info->condition, parameters))
        {
            if (!info->active)
            {
                parameter.active = false;
                parameter.off = true;
                parameter.note = "inactive: " + info->condition->to_string()
                    + " is false with the values of the parameters it names";
            }
            if (!parameter.active)
            {
                parameter.condition = parameter.condition.empty()
                    ? named
                    : parameter.condition + " && " + named;
            }
            continue;
        }
        const auto condition =
            info->condition->text_with([&](const std::string_view path) {
                return irace_reference(path, parameters, infos);
            });
        parameter.condition = parameter.condition.empty()
            ? condition
            : parameter.condition + " && " + condition;
    }
}

// The requirements that refer to tuned parameters, as irace's forbidden
// expressions, each with its message and, when it names parameters that are
// not tuned, the expression with every name, for when they are.
struct irace_forbidden_line
{
    std::string_view message;
    std::string expression;
    std::string named;
};

[[nodiscard]]
inline std::vector<irace_forbidden_line> irace_forbidden(
    const std::vector<config::requirement_info>& requirements,
    const std::vector<irace_parameter>& parameters,
    const std::vector<config::parameter_info>& infos)
{
    const auto identifiers = irace_identifiers(infos);
    std::vector<irace_forbidden_line> result;
    for (const auto& requirement : requirements)
    {
        if (cost_path(requirement.path)
            || !refers_to_tuned(requirement.expression, parameters))
            continue;
        irace_forbidden_line line{
            .message = requirement.message,
            .expression =
                "!" + requirement.expression.text_with([&](const std::string_view path) {
                    return irace_reference(path, parameters, infos);
                }),
            .named =
                "!" + requirement.expression.text_with([&](const std::string_view path) {
                    return irace_name(path, infos, identifiers);
                }),
        };
        if (line.named == line.expression)
            line.named.clear();
        result.push_back(std::move(line));
    }
    return result;
}

[[nodiscard]]
inline std::string shell_quote(const std::string_view text)
{
    std::string result{"'"};
    for (const char character : text)
        if (character == '\'')
            result += "'\\''";
        else
            result += character;
    return result + '\'';
}

// Writes a file unless it exists, and records in result whether it was
// written, kept, or could not be written.
template<class Write>
void write_new_file(
    const std::filesystem::path& path,
    irace_stub_result& result,
    Write&& write)
{
    std::error_code error;
    if (std::filesystem::exists(path, error))
    {
        result.kept.push_back(path);
        return;
    }
    std::ofstream out{path};
    write(out);
    out.close();
    if (!out)
    {
        result.errors.push_back("cannot write " + easylocal::detail::utf8_text(path));
        return;
    }
    result.written.push_back(path);
}

} // namespace detail

/// Writes the files of an irace scenario for a program to stub.directory,
/// which it creates: parameters.txt (the parameters to tune and, commented
/// out, the others with a range to start from), fixed.conf (the values every
/// run starts from), target-runner (the script irace calls), scenario.txt,
/// instances.txt and configurations.txt.
///
/// The files are a stub to edit: one that exists is kept as it is, except
/// configurations.txt (the current values, as a first configuration), which is
/// rewritten each time to agree with parameters.txt as it is on disk, its
/// values moved into the ranges. Returns the files written and kept, and the
/// errors; with an invalid range, nothing is written, and with no parameter
/// to tune yet, no configurations.txt.
[[nodiscard]]
inline irace_stub_result write_irace_stub(const irace_stub& stub)
{
    irace_stub_result result;
    const int digits = detail::irace_digits(stub);
    auto parameters = detail::irace_parameters(stub, digits, result.errors);
    if (!result)
        return result;
    detail::apply_irace_conditions(parameters, stub.parameters);
    const auto forbidden =
        detail::irace_forbidden(stub.requirements, parameters, stub.parameters);

    std::error_code error;
    std::filesystem::create_directories(stub.directory, error);
    if (error)
    {
        result.errors.push_back(
            "cannot create " + easylocal::detail::utf8_text(stub.directory) + ": "
            + error.message());
        return result;
    }
    const auto directory = std::filesystem::absolute(stub.directory);
    const auto program = easylocal::detail::utf8_text(stub.program.filename());

    const bool new_parameters =
        !std::filesystem::exists(directory / "parameters.txt", error);
    for (const auto& parameter : parameters)
        if (new_parameters && !parameter.active && !parameter.off && parameter.type != 0)
            ++result.to_complete;

    detail::write_new_file(directory / "parameters.txt", result, [&](std::ostream& out) {
        out << "## The parameters of " << program
            << " for irace, written by --tuning.irace.\n"
            << "## name  switch  type  (domain)  | condition\n"
            << "## A commented line is a parameter left at its value: uncomment it, and\n"
            << "## check its range, to tune it. The cost.* parameters are left out: they\n"
            << "## change the cost that irace compares. After a change, run\n"
            << "## --tuning.irace again: it rewrites configurations.txt to agree.\n";
        for (const auto& parameter : parameters)
        {
            out << '\n';
            if (!parameter.description.empty())
                out << "# " << parameter.description << " (default "
                    << parameter.default_value << ")\n";
            if (parameter.type == 0)
                out << "# " << parameter.name << ": " << parameter.note << '\n';
            else
            {
                if (!parameter.note.empty())
                    out << "# " << parameter.note << '\n';
                out << (parameter.active ? "" : "# ") << detail::irace_line(parameter)
                    << '\n';
            }
        }
        if (digits != detail::irace_default_digits)
        {
            out << "\n## The digits after the point of the real parameters, which their\n"
                << "## bounds need.\n"
                << "[global]\n"
                << "digits = " << digits << '\n';
        }
        out << "\n## Combinations that are not valid, as R expressions, after a line\n"
            << "## [forbidden]: the requirements of the program between tuned parameters.\n";
        if (!forbidden.empty())
        {
            out << "[forbidden]\n";
            for (const auto& line : forbidden)
            {
                out << "# " << line.message << '\n';
                if (!line.named.empty())
                    out << "# with every parameter it names tuned: " << line.named
                        << '\n';
                out << line.expression << '\n';
            }
        }
    });

    detail::write_new_file(directory / "fixed.conf", result, [&](std::ostream& out) {
        out << "# The values every run of irace starts from: those given on the command\n"
            << "# line when the stub was written. The tuned parameters override them.\n";
        for (const auto& fixed : stub.fixed)
            out << fixed.path << " = " << fixed.value << '\n';
    });

    const auto runner_path = directory / "target-runner";
    detail::write_new_file(runner_path, result, [&](std::ostream& out) {
        out << "#!/bin/sh\n"
            << "# The target runner of irace for " << program
            << ", written by --tuning.irace.\n"
            << "# irace calls: target-runner <configuration id> <instance id> <seed>\n"
            << "#   <instance> [<bound>] <switches of the parameters>...\n"
            << "# and reads the cost, one number, from the last line of the output.\n"
            << "instance=$4\n"
            << "seed=$3\n"
            << "shift 4\n"
            << "# The bound, when irace passes one, is not used.\n"
            << "case \"${1-}\" in --*) ;; *) [ $# -gt 0 ] && shift ;; esac\n"
            << "exec " << detail::shell_quote(easylocal::detail::utf8_text(stub.program))
            << " --config "
            << detail::shell_quote(easylocal::detail::utf8_text(directory / "fixed.conf"))
            << " --tuning.print=cost --instance=\"$instance\" --seed=\"$seed\" \"$@\"\n";
    });
    if (std::ranges::find(result.written, runner_path) != result.written.end())
    {
        std::filesystem::permissions(
            runner_path,
            std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec
                | std::filesystem::perms::others_exec,
            std::filesystem::perm_options::add,
            error);
    }

    detail::write_new_file(directory / "instances.txt", result, [&](std::ostream& out) {
        out << "# The instances irace tunes on: one file per line, absolute or relative\n"
            << "# to this directory.\n";
        if (!stub.instance.empty())
            out << easylocal::detail::utf8_text(std::filesystem::absolute(stub.instance))
                << '\n';
    });

    detail::write_new_file(directory / "scenario.txt", result, [&](std::ostream& out) {
        out << "## The irace scenario of " << program << ", written by --tuning.irace:\n"
            << "## run irace in this directory.\n"
            << "targetRunner = \"./target-runner\"\n"
            << "parameterFile = \"./parameters.txt\"\n";
        out << "configurationsFile = \"./configurations.txt\"\n"
            << "trainInstancesDir = \"\"\n"
            << "trainInstancesFile = \"./instances.txt\"\n"
            << "## The budget: the number of runs of the program.\n"
            << "maxExperiments = 1000\n"
            << "## Runs in parallel.\n"
            << "# parallel = 4\n";
    });
    // configurations.txt follows parameters.txt as it is now, edited or not:
    // the current values, with the fixed ones, moved into the ranges.
    std::ifstream declared_in{directory / "parameters.txt"};
    const auto declared = detail::read_irace_parameters(declared_in);
    const auto default_runner =
        stub.runners.empty() ? std::string{} : stub.runners.front();
    // Each declared parameter sets the program's parameter its switch names,
    // or else the one of its identifier (or path).
    std::map<std::string, std::string, std::less<>> paths;
    for (const auto& [path, identifier] : detail::irace_identifiers(stub.parameters))
        paths.emplace(identifier, path);
    const auto known = [&](const std::string_view path) {
        return std::ranges::find(stub.parameters, path, &config::parameter_info::path)
            != stub.parameters.end()
            || std::ranges::find(stub.fixed, path, &config::owned_text_override::path)
            != stub.fixed.end();
    };
    std::vector<std::string> values;
    for (const auto& parameter : declared)
    {
        std::string path;
        if (parameter.name != "runner")
        {
            if (!parameter.path.empty() && known(parameter.path))
                path = parameter.path;
            else if (const auto found = paths.find(parameter.name); found != paths.end())
                path = found->second;
            else if (known(parameter.name))
                path = parameter.name;
        }
        std::string value;
        if (parameter.name == "runner")
            value = default_runner;
        else if (const auto fixed = std::ranges::find(
                     stub.fixed,
                     path,
                     &config::owned_text_override::path);
            !path.empty() && fixed != stub.fixed.end())
            value = fixed->value;
        else if (const auto info = std::ranges::find(
                     stub.parameters,
                     path,
                     &config::parameter_info::path);
            info != stub.parameters.end())
            value = info->value;
        else
        {
            result.errors.push_back(
                "parameters.txt names " + parameter.name
                + ", which is not a parameter to tune");
            continue;
        }
        const auto owner = detail::runner_of(path);
        const auto info =
            std::ranges::find(stub.parameters, path, &config::parameter_info::path);
        const bool runner_off = parameter.condition.starts_with("runner == ")
            && !owner.empty() && owner != default_runner;
        const bool condition_off =
            info != stub.parameters.end() && info->condition && !info->active;
        if (runner_off || condition_off)
        {
            values.emplace_back("NA");
            continue;
        }
        auto [clamped, moved] = detail::clamp_irace_value(parameter, value);
        if (moved)
            result.moved.push_back(path + " = " + clamped);
        values.push_back(parameter.type == 'c' ? '"' + clamped + '"' : clamped);
    }
    result.tuned = declared.size();
    // Nothing to tune: no configuration to write, until a parameter is.
    if (!result || declared.empty())
        return result;
    const auto configurations = directory / "configurations.txt";
    std::ofstream out{configurations};
    for (std::size_t index = 0; index < declared.size(); ++index)
        out << (index == 0 ? "" : " ") << declared[index].name;
    out << '\n';
    for (std::size_t index = 0; index < values.size(); ++index)
        out << (index == 0 ? "" : " ") << values[index];
    out << '\n';
    out.close();
    if (!out)
        result.errors.push_back(
            "cannot write " + easylocal::detail::utf8_text(configurations));
    else
        result.configurations = configurations;

    // The files as the caller named the directory.
    for (auto* paths : {&result.written, &result.kept})
        for (auto& path : *paths)
            path = stub.directory / path.filename();
    if (!result.configurations.empty())
        result.configurations = stub.directory / result.configurations.filename();
    return result;
}

} // namespace easylocal
