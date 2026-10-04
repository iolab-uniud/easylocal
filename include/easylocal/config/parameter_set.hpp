#pragma once

/// \file
/// parameter_set: the parameters of one or more objects as paths and textual
/// values, the common ground of the command line, configuration files, TOML,
/// the TextUI and REST.
///
/// It lists, validates and changes them transactionally (all overrides or none)
/// on the objects it refers to.

#include <easylocal/config/domain.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameters.hpp>

#include <cassert>
#include <cstddef>
#include <exception>
#include <functional>
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
/// (format_value), whether it can be changed, the kind of its value and its
/// domain.
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
// leaf(path, descriptor, value&) for the fields, group(path, nested&) for each
// nested block, before its own fields.
template<class Block, class Leaf, class Group>
void walk_schema(Block& block, const std::string& prefix, Leaf& leaf, Group& group)
{
    std::apply(
        [&](auto... descriptors) {
            (
                [&] {
                    using descriptor_type = decltype(descriptors);
                    auto& value = block.*descriptor_type::member;
                    const auto path = join_path(prefix, descriptor_type::name());
                    if constexpr (is_parameter_group_v<descriptor_type>)
                    {
                        group(path, value);
                        walk_schema(value, path, leaf, group);
                    }
                    else
                    {
                        leaf(path, descriptors, value);
                    }
                }(),
                ...);
        },
        std::remove_cvref_t<Block>::parameter_schema());
}

// The fields of a block, not of its nested groups, that lie outside the
// domain of their schema, one diagnostic each, with the field's path.
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
                    if constexpr (!is_parameter_group_v<descriptor_type>)
                    {
                        const auto& value = block.*descriptor_type::member;
                        if (!domain_contains(descriptors.domain, value))
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

// The diagnostics of a block: the fields outside their domains, or else its
// validate(), which should check them too; then the same for its nested groups.
template<class Block>
void validate_one_block(
    const Block& block,
    const std::string& prefix,
    std::vector<configuration_validation_diagnostic>& diagnostics)
{
    if (!check_field_domains(block, prefix, diagnostics))
        return;
    if (const auto validation = block.validate(); !validation)
        diagnostics.push_back({prefix, std::string{validation.message}});
}

template<class Block>
void validate_block(
    const Block& block,
    const std::string& prefix,
    std::vector<configuration_validation_diagnostic>& diagnostics)
{
    validate_one_block(block, prefix, diagnostics);
    auto leaf = [](const std::string&, const auto&, const auto&) {};
    auto group = [&diagnostics](const std::string& path, const auto& nested) {
        validate_one_block(nested, path, diagnostics);
    };
    walk_schema(block, prefix, leaf, group);
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
        requires configurable_endpoint<Endpoint>
    parameter_set& add(const std::string_view prefix, Endpoint& endpoint)
    {
        using block_type = configurable_parameters_t<Endpoint>;
        std::function<void(block_type)> commit;
        if constexpr (!std::is_const_v<Endpoint>)
        {
            commit = [&endpoint](block_type staged) {
                // configure() must accept every block whose validate() succeeds:
                // a failure here is a broken invariant of the object.
                const auto committed = endpoint.configure(std::move(staged));
                assert(committed);
                if (!committed)
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
        for (std::size_t first = 0; first < overrides.size(); ++first)
        {
            for (std::size_t second = first + 1; second < overrides.size(); ++second)
            {
                if (overrides[first].path == overrides[second].path)
                {
                    result.diagnostics.push_back({
                        .error = override_error::duplicate_path,
                        .path = std::string{overrides[second].path},
                        .value = std::string{overrides[second].value},
                        .message = "duplicate override path",
                    });
                }
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
            auto leaf =
                [&](const std::string& path, const auto& descriptor, const auto& value) {
                    result.push_back({
                        .path = path,
                        .description = descriptor.description,
                        .value = format_value(value),
                        .read_only = read_only,
                        .kind = kind_of<decltype(value)>(),
                        .domain = describe_domain(descriptor.domain),
                    });
                };
            auto group = [](const std::string&, const auto&) {};
            detail::walk_schema(get(), std::string{at}, leaf, group);
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
            auto leaf = [&](const std::string& path, const auto&, auto& value) {
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

/// Something whose parameters a set can hold: configuration() gives them, with
/// paths relative to it.
template<class T>
concept configuration_provider = requires(T& value) {
    { value.configuration() } -> std::same_as<parameter_set>;
};

/// The parameters of value under prefix, if it has any.
template<class T>
void add_configuration(parameter_set& parameters, const std::string_view prefix, T& value)
{
    if constexpr (configuration_provider<T>)
        parameters.add(prefix, value.configuration());
}

} // namespace easylocal::config
