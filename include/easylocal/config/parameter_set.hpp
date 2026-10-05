#pragma once

/// \file
/// parameter_set: the parameters of one or more objects as paths and textual
/// values, the common ground of the command line, configuration files, TOML,
/// the TextUI and REST.
///
/// It lists, validates and changes them transactionally (all overrides or none)
/// on the objects it refers to.

#include <easylocal/config/condition.hpp>
#include <easylocal/config/domain.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameters.hpp>

#include <cstddef>
#include <exception>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::config
{

/// One parameter of a set: its full path, its description, its value as text
/// (format_value), whether it can be changed, the kind of its value, its
/// domain, and when it matters.
struct parameter_info
{
    /// The full path of the parameter.
    std::string path;
    /// The description of the parameter.
    std::string_view description;
    /// The value, as text.
    std::string value;
    /// Whether the parameter cannot be changed.
    bool read_only{};
    /// The kind of the value.
    parameter_kind kind{parameter_kind::text};
    /// The values the parameter may take, as its schema declares them; empty
    /// when it declares none.
    domain_info domain{};
    /// Whether the parameter matters with the current values, by its condition.
    bool active{true};
    /// When the parameter matters, with the full paths of the parameters it
    /// refers to; empty when it always does.
    std::optional<expression_info> condition{};
};

/// A requirement between the parameters of a block: the path of the block, why
/// the block is not valid when it does not hold, the expression with full
/// paths, and whether it holds with the current values.
struct requirement_info
{
    /// The path of the block; empty for a block at the root of the set.
    std::string path;
    /// The reason given when the requirement does not hold.
    std::string_view message;
    /// The expression that must hold, with the full paths of the parameters.
    expression_info expression;
    /// Whether it holds with the current values.
    bool satisfied{};
};

/// A block whose validate() fails, with the reason.
struct configuration_validation_diagnostic
{
    /// The path of the block; empty for a block at the root of the set.
    std::string path;
    /// The reason, as validate() gives it.
    std::string message;
};

/// The blocks of a set whose validate() fails.
///
/// It converts to true when every block is valid.
struct configuration_validation_result
{
    /// One diagnostic for each invalid block.
    std::vector<configuration_validation_diagnostic> diagnostics;

    /// Whether there are no diagnostics.
    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return diagnostics.empty();
    }
};

namespace detail
{

[[nodiscard]]
inline std::string join_path(const std::string_view prefix, const std::string_view name)
{
    if (prefix.empty())
        return std::string{name};
    if (name.empty())
        return std::string{prefix};
    std::string path{prefix};
    path += '.';
    path.append(name);
    return path;
}

// Every field of a block, the fields of its nested groups included:
// leaf(path, descriptor, value&, owner) for the fields, with the block they
// belong to, group(path, nested&) for each nested block, before its own
// fields. Requirements are left out.
template<class Block, class Leaf, class Group>
void walk_schema(Block& block, const std::string& prefix, Leaf& leaf, Group& group)
{
    std::apply(
        [&](auto... descriptors) {
            (
                [&] {
                    using descriptor_type = decltype(descriptors);
                    if constexpr (!is_parameter_requirement_v<descriptor_type>)
                    {
                        auto& value = block.*descriptor_type::member;
                        const auto path = join_path(prefix, descriptor_type::name());
                        if constexpr (is_parameter_group_v<descriptor_type>)
                        {
                            group(path, value);
                            walk_schema(value, path, leaf, group);
                        }
                        else
                        {
                            leaf(path, descriptors, value, std::as_const(block));
                        }
                    }
                }(),
                ...);
        },
        std::remove_cvref_t<Block>::parameter_schema());
}

// The requirements of a block and of its nested groups:
// visit(path of the block, requirement, block).
template<class Block, class Visit>
void walk_requirements(const Block& block, const std::string& prefix, Visit& visit)
{
    std::apply(
        [&](const auto&... descriptors) {
            (
                [&] {
                    using descriptor_type = std::remove_cvref_t<decltype(descriptors)>;
                    if constexpr (is_parameter_requirement_v<descriptor_type>)
                        visit(prefix, descriptors, block);
                    else if constexpr (is_parameter_group_v<descriptor_type>)
                    {
                        walk_requirements(
                            block.*descriptor_type::member,
                            join_path(prefix, descriptor_type::name()),
                            visit);
                    }
                }(),
                ...);
        },
        Block::parameter_schema());
}

// The fields of a block that matter, not of its nested groups, that lie
// outside the domain of their schema, one diagnostic each, with the field's
// path; then the requirements that do not hold, with the block's path.
template<class Block>
bool check_field_domains(
    const Block& block,
    const std::string& prefix,
    std::vector<configuration_validation_diagnostic>& diagnostics)
{
    bool valid = true;
    std::apply(
        [&](const auto&... descriptors) {
            (
                [&] {
                    using descriptor_type = std::remove_cvref_t<decltype(descriptors)>;
                    if constexpr (is_parameter_requirement_v<descriptor_type>)
                    {
                        if (!static_cast<bool>(evaluate(descriptors.expression, block)))
                        {
                            valid = false;
                            diagnostics.push_back(
                                {prefix, std::string{descriptors.message}});
                        }
                    }
                    else if constexpr (!is_parameter_group_v<descriptor_type>)
                    {
                        const auto& value = block.*descriptor_type::member;
                        if (is_active(descriptors, block)
                            && !domain_contains(descriptors.domain, value))
                        {
                            valid = false;
                            diagnostics.push_back(
                                {join_path(prefix, descriptor_type::name()),
                                    "expected a value in "
                                        + describe_domain(descriptors.domain).text()
                                        + ", got " + format_value(value)});
                        }
                    }
                }(),
                ...);
        },
        std::remove_cvref_t<Block>::parameter_schema());
    return valid;
}

// The diagnostics of a block and of its nested groups, each under its own
// path: the fields outside their domains and the requirements that do not
// hold; then the groups; then, when all of these pass, the block's validate(),
// which checks them again (check_schema) and would repeat a group's diagnostic
// under the block's path. Whether the block is valid.
template<class Block>
bool validate_block(
    const Block& block,
    const std::string& prefix,
    std::vector<configuration_validation_diagnostic>& diagnostics)
{
    bool valid = check_field_domains(block, prefix, diagnostics);
    std::apply(
        [&](const auto&... descriptors) {
            (
                [&] {
                    using descriptor_type = std::remove_cvref_t<decltype(descriptors)>;
                    if constexpr (is_parameter_group_v<descriptor_type>)
                    {
                        if (!validate_block(
                                block.*descriptor_type::member,
                                join_path(prefix, descriptor_type::name()),
                                diagnostics))
                            valid = false;
                    }
                }(),
                ...);
        },
        std::remove_cvref_t<Block>::parameter_schema());
    if (!valid)
        return false;
    if constexpr (requires { block.validate(); })
    {
        if (const auto validation = block.validate(); !validation)
        {
            diagnostics.push_back({prefix, std::string{validation.message}});
            return false;
        }
    }
    return true;
}

} // namespace detail

/// The parameters of one or more objects, as paths and textual values: what the
/// command line, configuration files, TOML, the TextUI and REST read and
/// change.
///
/// A set refers to the objects it was built from, which must outlive it and
/// stay in place; it holds no values of its own.
///
/// Components give their parameters with paths relative to themselves; who
/// composes them adds prefixes:
///
///     config::parameter_set parameters;
///     parameters.add("application", program_parameters);
///     parameters.add("solver", runner.configuration());
class parameter_set
{
public:
    /// A block of values (a parameter_block): its fields, under prefix.
    ///
    /// A const block is read-only.
    template<parameter_block Block>
    parameter_set& add(const std::string_view prefix, Block& block)
    {
        using block_type = std::remove_const_t<Block>;
        std::function<void(block_type)> commit;
        if constexpr (!std::is_const_v<Block>)
            commit = [&block](block_type staged) { block = std::move(staged); };
        return add_entry(
            make_entry<block_type>(
                std::string{prefix},
                [&block]() -> const block_type& { return block; },
                std::move(commit)));
    }

    /// A configurable object: parameters() gives its block, configure() takes a
    /// valid one, for objects that rebuild something from their parameters.
    template<class Endpoint>
        requires detail::configurable_endpoint<Endpoint>
    parameter_set& add(const std::string_view prefix, Endpoint& endpoint)
    {
        using block_type = detail::configurable_parameters_t<Endpoint>;
        std::function<void(block_type)> commit;
        if constexpr (!std::is_const_v<Endpoint>)
        {
            commit = [&endpoint](block_type staged) {
                // configure() must accept every block whose validate() succeeds:
                // a failure here is a broken invariant of the object.
                if (!endpoint.configure(std::move(staged)))
                    std::terminate();
            };
        }
        return add_entry(
            make_entry<block_type>(
                std::string{prefix},
                [&endpoint]() -> const block_type& { return endpoint.parameters(); },
                std::move(commit)));
    }

    /// The parameters of another set, under prefix.
    parameter_set& add(const std::string_view prefix, const parameter_set& other)
    {
        for (auto entry : other.entries_)
        {
            entry.prefix = detail::join_path(prefix, entry.prefix);
            add_entry(std::move(entry));
        }
        return *this;
    }

    /// The same, at the root of this set.
    template<class Source>
    parameter_set& add(Source& source)
        requires requires(parameter_set& set) { set.add(std::string_view{}, source); }
    {
        return add(std::string_view{}, source);
    }

    /// The parameters of another set, at the root of this set.
    parameter_set& add(const parameter_set& other)
    {
        return add(std::string_view{}, other);
    }

    /// Whether nothing was added to the set.
    [[nodiscard]]
    bool empty() const noexcept
    {
        return entries_.empty();
    }

    /// The parameters, with their paths, descriptions, values as text and
    /// whether they are read-only.
    [[nodiscard]]
    std::vector<parameter_info> parameters() const
    {
        std::vector<parameter_info> result;
        for (const auto& entry : entries_)
            entry.list(entry.prefix, result);
        return result;
    }

    /// The requirements between the parameters of each block, with full paths.
    [[nodiscard]]
    std::vector<requirement_info> requirements() const
    {
        std::vector<requirement_info> result;
        for (const auto& entry : entries_)
            entry.requirements(entry.prefix, result);
        return result;
    }

    /// The diagnostics of every block's validate(), with the block's path.
    [[nodiscard]]
    configuration_validation_result validate() const
    {
        configuration_validation_result result;
        for (const auto& entry : entries_)
            entry.validate(entry.prefix, result.diagnostics);
        return result;
    }

    /// Applies textual overrides: all of them, or none.
    ///
    /// Every block they touch is changed on a copy, parsed and validated first;
    /// only when every override names a parameter and every touched block is
    /// valid are the copies committed. The set itself does not change, only the
    /// objects it refers to.
    override_result apply(const std::span<const text_override> overrides) const
    {
        override_result result;
        // Each repetition of a path once, however many came before it.
        for (std::size_t index = 1; index < overrides.size(); ++index)
        {
            const auto earlier = overrides.first(index);
            if (std::ranges::any_of(earlier, [&](const text_override& other) {
                    return other.path == overrides[index].path;
                }))
            {
                result.diagnostics.push_back({
                    .error = override_error::duplicate_path,
                    .path = std::string{overrides[index].path},
                    .value = std::string{overrides[index].value},
                    .message = "duplicate override path",
                });
            }
        }

        std::vector<std::size_t> matches(overrides.size(), 0);
        std::vector<std::function<void()>> commits;
        for (const auto& entry : entries_)
        {
            if (auto commit =
                    entry.stage(entry.prefix, overrides, matches, result.diagnostics))
            {
                commits.push_back(std::move(commit));
            }
        }
        for (std::size_t index = 0; index < overrides.size(); ++index)
        {
            if (matches[index] == 0)
            {
                result.diagnostics.push_back({
                    .error = override_error::unknown_parameter,
                    .path = std::string{overrides[index].path},
                    .value = std::string{overrides[index].value},
                    .message = "unknown configuration parameter",
                });
            }
        }
        if (!result)
            return result;

        for (auto& commit : commits)
            commit();
        result.applied_parameter_blocks = commits.size();
        return result;
    }

private:
    struct entry_type
    {
        std::string prefix;
        std::function<void(std::string_view, std::vector<parameter_info>&)> list;
        std::function<
            void(std::string_view, std::vector<configuration_validation_diagnostic>&)>
            validate;
        std::function<void(std::string_view, std::vector<requirement_info>&)>
            requirements;
        // Changes a copy of the block with the overrides that name its
        // parameters, counting them in matches; the result commits the copy,
        // and is empty when the block is untouched or the copy is invalid.
        std::function<std::function<void()>(
            std::string_view,
            std::span<const text_override>,
            std::vector<std::size_t>&,
            std::vector<override_diagnostic>&)>
            stage;
    };

    template<class Block>
    static entry_type make_entry(
        std::string prefix,
        std::function<const Block&()> get,
        std::function<void(Block)> commit)
    {
        const bool read_only = !commit;
        entry_type entry;
        entry.prefix = std::move(prefix);
        entry.list = [get, read_only](
                         const std::string_view at,
                         std::vector<parameter_info>& result) {
            auto leaf = [&](const std::string& path,
                            const auto& descriptor,
                            const auto& value,
                            const auto& owner) {
                parameter_info info{
                    .path = path,
                    .description = descriptor.description,
                    .value = format_value(value),
                    .read_only = read_only,
                    .kind = kind_of<decltype(value)>(),
                    .domain = describe_domain(descriptor.domain),
                    .active = is_active(descriptor, owner),
                };
                if constexpr (!std::same_as<
                                  std::remove_cvref_t<decltype(descriptor.condition)>,
                                  no_condition>)
                {
                    // The references are relative to the field's block.
                    const auto name = std::remove_cvref_t<decltype(descriptor)>::name();
                    const auto block_path = std::string_view{path}.substr(
                        0,
                        path.size() - name.size() - (path.size() > name.size() ? 1 : 0));
                    info.condition =
                        describe_expression(descriptor.condition, block_path);
                }
                result.push_back(std::move(info));
            };
            auto group = [](const std::string&, const auto&) {};
            detail::walk_schema(get(), std::string{at}, leaf, group);
        };
        entry.requirements =
            [get](const std::string_view at, std::vector<requirement_info>& result) {
                auto visit = [&](const std::string& path,
                                 const auto& requirement,
                                 const auto& block) {
                    result.push_back({
                        .path = path,
                        .message = requirement.message,
                        .expression = describe_expression(requirement.expression, path),
                        .satisfied =
                            static_cast<bool>(evaluate(requirement.expression, block)),
                    });
                };
                detail::walk_requirements(get(), std::string{at}, visit);
            };
        entry.validate =
            [get](
                const std::string_view at,
                std::vector<configuration_validation_diagnostic>& result) {
                detail::validate_block(get(), std::string{at}, result);
            };
        entry.stage =
            [get, commit](
                const std::string_view at,
                const std::span<const text_override> overrides,
                std::vector<std::size_t>& matches,
                std::vector<override_diagnostic>& diagnostics) -> std::function<void()> {
            Block staged = get();
            bool touched = false;
            bool failed = false;
            auto leaf = [&](const std::string& path,
                            const auto&,
                            auto& value,
                            const auto&) {
                for (std::size_t index = 0; index < overrides.size(); ++index)
                {
                    const auto& candidate = overrides[index];
                    if (candidate.path != path)
                        continue;
                    ++matches[index];
                    touched = true;
                    if (!commit)
                    {
                        failed = true;
                        diagnostics.push_back({
                            .error = override_error::read_only_parameter,
                            .path = path,
                            .value = std::string{candidate.value},
                            .message = "parameter is read-only",
                        });
                        continue;
                    }
                    const auto error = detail::parse_text_value(candidate.value, value);
                    if (!error.empty())
                    {
                        failed = true;
                        diagnostics.push_back({
                            .error = override_error::parse_error,
                            .path = path,
                            .value = std::string{candidate.value},
                            .message = std::string{error},
                        });
                    }
                }
            };
            auto group = [](const std::string&, auto&) {};
            detail::walk_schema(staged, std::string{at}, leaf, group);
            if (!touched || failed)
                return {};

            std::vector<configuration_validation_diagnostic> invalid;
            detail::validate_block(staged, std::string{at}, invalid);
            if (!invalid.empty())
            {
                for (auto& diagnostic : invalid)
                {
                    diagnostics.push_back({
                        .error = override_error::validation_error,
                        .path = std::move(diagnostic.path),
                        .value = {},
                        .message = std::move(diagnostic.message),
                    });
                }
                return {};
            }
            return [commit, staged = std::move(staged)]() mutable {
                commit(std::move(staged));
            };
        };
        return entry;
    }

    parameter_set& add_entry(entry_type entry)
    {
        std::vector<parameter_info> added;
        entry.list(entry.prefix, added);
        const auto existing = parameters();
        for (const auto& candidate : added)
        {
            for (const auto& present : existing)
            {
                if (candidate.path == present.path)
                {
                    throw std::invalid_argument{
                        "duplicate configuration path: " + candidate.path};
                }
            }
        }
        entries_.push_back(std::move(entry));
        return *this;
    }

    std::vector<entry_type> entries_;
};

/// The free spellings of the set's members.
[[nodiscard]]
inline override_result apply_overrides(
    const parameter_set& parameters,
    const std::span<const text_override> overrides)
{
    return parameters.apply(overrides);
}

/// Whether the parameters are valid, and why not.
[[nodiscard]]
inline configuration_validation_result validate(const parameter_set& parameters)
{
    return parameters.validate();
}

namespace detail
{

// Throws std::invalid_argument with "<path>: <message>" for each diagnostic,
// joined by "; ", unless there are none.
inline void throw_diagnostics(
    const std::vector<configuration_validation_diagnostic>& diagnostics)
{
    if (diagnostics.empty())
        return;
    std::string message;
    for (const auto& diagnostic : diagnostics)
    {
        if (!message.empty())
            message += "; ";
        if (!diagnostic.path.empty())
        {
            message += diagnostic.path;
            message += ": ";
        }
        message += diagnostic.message;
    }
    throw std::invalid_argument{message};
}

} // namespace detail

/// Throws `std::invalid_argument` unless every block of the set is valid; the
/// message gives each invalid block as "<path>: <message>".
inline void require_valid(const parameter_set& parameters)
{
    detail::throw_diagnostics(parameters.validate().diagnostics);
}

/// Returns block when it is valid, with its nested groups; throws
/// `std::invalid_argument` otherwise, with the reason, the path of a field or
/// group first ("temperature.cooling_rate: ...").
///
/// The constructors of the runners and of their policies call it on their
/// parameters.
template<parameter_block Block>
const Block& require_valid(const Block& block)
{
    std::vector<configuration_validation_diagnostic> diagnostics;
    detail::validate_block(block, std::string{}, diagnostics);
    detail::throw_diagnostics(diagnostics);
    return block;
}

/// The paths of the parameters of a set that declare no domain, but the
/// booleans, whose domain is true and false: each should declare a range,
/// one_of, or easylocal::unlimited for any value.
[[nodiscard]]
inline std::vector<std::string> undeclared_domains(const parameter_set& parameters)
{
    std::vector<std::string> result;
    for (const auto& info : parameters.parameters())
        if (!info.domain && info.kind != parameter_kind::boolean)
            result.push_back(info.path);
    return result;
}

namespace detail
{

// Something whose parameters a set can hold: configuration() gives them, with
// paths relative to it.
template<class T>
concept configuration_provider = requires(T& value) {
    { value.configuration() } -> std::same_as<parameter_set>;
};

} // namespace detail

/// The parameters of value under prefix, if it has any: those its
/// configuration() gives, with paths relative to it.
template<class T>
void add_configuration(parameter_set& parameters, const std::string_view prefix, T& value)
{
    if constexpr (detail::configuration_provider<T>)
        parameters.add(prefix, value.configuration());
}

} // namespace easylocal::config
