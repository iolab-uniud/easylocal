#pragma once

/// \file
/// Parameter blocks: plain structs that describe their own fields with a
/// compile-time schema (config::field, config::group, config::fields), the
/// values each field may take (config::range, config::one_of), and check them
/// with validate().
///
/// Runners, neighborhoods, cost expressions and programs declare their
/// parameters this way.

#include <easylocal/config/condition.hpp>
#include <easylocal/config/domain.hpp>
#include <easylocal/config/fixed_string.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::config
{

/// Whether a parameter block is valid, and why not.
///
/// It converts to true when the block is valid.
struct validation_result
{
    /// Whether the block is valid.
    bool valid{true};
    /// Why the block is not valid; empty when it is.
    std::string_view message{};

    /// Whether the block is valid.
    [[nodiscard]]
    constexpr explicit operator bool() const noexcept
    {
        return valid;
    }

    /// A valid result.
    [[nodiscard]]
    static constexpr validation_result success() noexcept
    {
        return {};
    }

    /// An invalid result, with the reason `message`, which must outlive it (a
    /// string literal usually does).
    [[nodiscard]]
    static constexpr validation_result failure(const std::string_view message) noexcept
    {
        return {.valid = false, .message = message};
    }
};

namespace detail
{

template<class MemberPointer>
struct member_pointer_traits;

template<class Value, class Owner>
struct member_pointer_traits<Value Owner::*>
{
    using owner_type = Owner;
    using value_type = Value;
};

// A name of a field or group: a segment of a path, [A-Za-z_][A-Za-z0-9_]*.
[[nodiscard]]
consteval bool valid_parameter_name(const std::string_view name) noexcept
{
    if (name.empty())
        return false;
    const auto letter = [](const char character) {
        return (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z') || character == '_';
    };
    if (!letter(name.front()))
        return false;
    for (const auto character : name)
        if (!letter(character) && !(character >= '0' && character <= '9'))
            return false;
    return true;
}

template<class... Fields>
[[nodiscard]]
consteval bool unique_field_names() noexcept
{
    if constexpr (sizeof...(Fields) <= 1)
    {
        return true;
    }
    else
    {
        return []<class First, class... Rest>(
                   std::type_identity<First>,
                   std::type_identity<Rest>...) {
            return ((First::name().empty() || First::name() != Rest::name()) && ...)
                && unique_field_names<Rest...>();
        }(std::type_identity<Fields>{}...);
    }
}

} // namespace detail

/// No condition: a field that always matters.
struct no_condition
{
};

/// The descriptor of a field of a parameter block: its name, its member, its
/// description, its domain and its condition, made with field().
template<
    fixed_string Name,
    auto Member,
    class Domain = no_domain,
    class Condition = no_condition>
    requires std::is_member_object_pointer_v<decltype(Member)>
struct parameter_field
{
    static_assert(
        detail::valid_parameter_name(Name.view()),
        "the name of a parameter field is a segment of its path: a letter or '_', "
        "then letters, digits and '_'");

    /// The type of the member pointer.
    using member_pointer_type = decltype(Member);
    /// The parameter block the member belongs to.
    using owner_type = typename detail::member_pointer_traits<
        member_pointer_type>::owner_type;
    /// The type of the member.
    using value_type = typename detail::member_pointer_traits<
        member_pointer_type>::value_type;
    /// The type of the domain: no_domain, a range_domain or a choice_domain.
    using domain_type = Domain;

    static_assert(
        domain_for<Domain, value_type>,
        "the domain does not fit the field: a range needs a number or a limit, "
        "one_of text values a string, one_of numbers a number");

    /// The pointer to the member.
    static constexpr auto member = Member;

    /// The description of the field.
    std::string_view description{};
    /// The values the field may take, checked when the parameters are
    /// validated; no_domain: any value of its type.
    Domain domain{};
    /// When the field matters, as an expression over the fields of its block;
    /// no_condition: always.
    Condition condition{};

    /// The same field, which matters only when condition holds, e.g.
    /// `.only_if(config::value<"calibration_samples"> > 0)`: its domain is
    /// checked only then, and a configurator tunes it only then.
    template<expression Expression>
    [[nodiscard]]
    constexpr parameter_field<Name, Member, Domain, Expression> only_if(
        const Expression& when) const noexcept
    {
        return {.description = description, .domain = domain, .condition = when};
    }

    /// The name of the field, the last component of its path.
    [[nodiscard]]
    static constexpr std::string_view name() noexcept
    {
        return Name.view();
    }
};

/// The descriptor of a field of a parameter block, e.g.
/// `field<"size", &MyParameters::size>("Description")`, with the values it may
/// take as the second argument:
/// `field<"cooling_rate", &P::cooling_rate>("...", range(0.0, 1.0).open())`.
template<fixed_string Name, auto Member, class Domain = no_domain>
    requires std::is_member_object_pointer_v<decltype(Member)> && is_domain_v<Domain>
[[nodiscard]]
constexpr parameter_field<Name, Member, Domain> field(
    const std::string_view description = {},
    const Domain domain = {}) noexcept
{
    return {.description = description, .domain = domain};
}

/// The descriptor of a field that takes any value of its type, e.g.
/// `field<"seed", &P::seed>("Seed", easylocal::unlimited)`: a seed, free text,
/// a number that may be negative.
template<fixed_string Name, auto Member>
    requires std::is_member_object_pointer_v<decltype(Member)>
[[nodiscard]]
constexpr parameter_field<Name, Member, unbounded_domain> field(
    const std::string_view description,
    easylocal::unlimited_t) noexcept
{
    return {.description = description, .domain = {}};
}

/// A member that is itself a parameter block, nested in the schema: its fields
/// are under "Name.", and check_schema of the enclosing block runs its
/// validate(), as in
/// `group<"temperature", &Parameters::temperature>("Temperature schedule")`.
template<fixed_string Name, auto Member>
    requires std::is_member_object_pointer_v<decltype(Member)>
struct parameter_group
{
    static_assert(
        detail::valid_parameter_name(Name.view()),
        "the name of a parameter group is a segment of its paths: a letter or '_', "
        "then letters, digits and '_'");

    /// The type of the member pointer.
    using member_pointer_type = decltype(Member);
    /// The parameter block the member belongs to.
    using owner_type =
        typename detail::member_pointer_traits<member_pointer_type>::owner_type;
    /// The type of the member, a parameter block.
    using value_type =
        typename detail::member_pointer_traits<member_pointer_type>::value_type;

    /// The pointer to the member.
    static constexpr auto member = Member;

    /// The description of the group.
    std::string_view description{};

    /// The name of the group, the prefix of the paths of its fields.
    [[nodiscard]]
    static constexpr std::string_view name() noexcept
    {
        return Name.view();
    }
};

/// The descriptor of a member that is itself a parameter block (a
/// parameter_group).
template<fixed_string Name, auto Member>
    requires std::is_member_object_pointer_v<decltype(Member)>
[[nodiscard]]
constexpr parameter_group<Name, Member> group(
    const std::string_view description = {}) noexcept
{
    return {.description = description};
}

/// Whether a descriptor is a parameter_group.
template<class Descriptor>
inline constexpr bool is_parameter_group_v = false;

/// A parameter_group is one.
template<fixed_string Name, auto Member>
inline constexpr bool is_parameter_group_v<parameter_group<Name, Member>> = true;

/// A requirement between the fields of a block, in its schema: an expression
/// that must hold, and the reason given when it does not.
///
/// It is made with config::require.
template<class Expression>
struct parameter_requirement
{
    /// The expression that must hold.
    Expression expression;
    /// Why the block is not valid when it does not hold.
    std::string_view message{};

    /// No name: a requirement is not a field.
    [[nodiscard]]
    static constexpr std::string_view name() noexcept
    {
        return {};
    }
};

/// A requirement between the fields of a block, in its schema, e.g.
/// `require(value<"final_temperature"> < value<"initial_temperature">,
/// "final_temperature must be smaller than initial_temperature")`: the
/// validation of the parameters checks it, and a configurator never proposes
/// values that break it.
///
/// The message must outlive it (a string literal does).
template<expression Expression>
[[nodiscard]]
constexpr parameter_requirement<Expression> require(
    const Expression& expression,
    const std::string_view message) noexcept
{
    return {.expression = expression, .message = message};
}

/// Whether a descriptor is a parameter_requirement.
template<class Descriptor>
inline constexpr bool is_parameter_requirement_v = false;

/// A parameter_requirement is one.
template<class Expression>
inline constexpr bool is_parameter_requirement_v<parameter_requirement<Expression>> =
    true;

/// Whether a descriptor is a field: neither a group nor a requirement.
template<class Descriptor>
inline constexpr bool is_parameter_field_v =
    !is_parameter_group_v<Descriptor> && !is_parameter_requirement_v<Descriptor>;

/// The schema of a parameter block, as parameter_schema() returns it: its field
/// and group descriptors, and the requirements between its fields.
///
/// The names of fields and groups must be unique within the block.
template<class... Fields>
[[nodiscard]]
constexpr std::tuple<Fields...> fields(Fields... parameter_fields) noexcept
{
    static_assert(
        detail::unique_field_names<Fields...>(),
        "parameter fields must have unique names within a block");
    return {std::move(parameter_fields)...};
}

/// A struct of parameters: its static parameter_schema() describes its fields,
/// its validate() checks them and returns a validation_result.
template<class T>
concept parameter_block =
    requires(const T& parameters)
    {
        { T::parameter_schema() };
        { parameters.validate() } -> std::same_as<validation_result>;
    };

namespace detail
{

// The parameter block of a configurable object, the type its parameters()
// returns.
template<class T>
using configurable_parameters_t = std::remove_cvref_t<decltype(
    std::declval<const std::remove_cvref_t<T>&>().parameters())>;

// An object configured with a parameter block: parameters() gives the current
// one, configure() takes a new one and returns a validation_result.
//
// configure() must accept every block whose validate() succeeds.
template<class T>
concept configurable_endpoint =
    requires(
        std::remove_cvref_t<T>& endpoint,
        const std::remove_cvref_t<T>& const_endpoint,
        configurable_parameters_t<T> parameters)
    {
        requires parameter_block<configurable_parameters_t<T>>;
        {
            const_endpoint.parameters()
        } -> std::same_as<const configurable_parameters_t<T>&>;
        {
            endpoint.configure(std::move(parameters))
        } -> std::same_as<validation_result>;
    };

} // namespace detail

namespace detail
{

// The schema of a block, computed once, at compile time: a schema that is not
// a constant expression, such as one with an inverted range, fails here, with
// the reason, wherever the schema is first read.
template<class Block>
inline constexpr auto schema_v = std::remove_cvref_t<Block>::parameter_schema();

} // namespace detail

/// Calls `function(descriptor, value)` on each field of a block, without its
/// nested groups.
template<parameter_block Parameters, class Function>
constexpr void for_each_parameter(
    Parameters& parameters,
    Function&& function)
{
    std::apply(
        [&parameters, &function](auto... descriptors) {
            (
                [&] {
                    using descriptor_type = decltype(descriptors);
                    if constexpr (!is_parameter_requirement_v<descriptor_type>)
                        static_assert(std::same_as<
                            std::remove_cv_t<typename descriptor_type::owner_type>,
                            std::remove_cv_t<Parameters>>);
                    if constexpr (is_parameter_field_v<descriptor_type>)
                        std::invoke(
                            function,
                            descriptors,
                            parameters.*descriptor_type::member);
                }(),
                ...);
        },
        detail::schema_v<Parameters>);
}

/// The same, on a const block.
template<parameter_block Parameters, class Function>
constexpr void for_each_parameter(
    const Parameters& parameters,
    Function&& function)
{
    std::apply(
        [&parameters, &function](auto... descriptors) {
            (
                [&] {
                    using descriptor_type = decltype(descriptors);
                    if constexpr (!is_parameter_requirement_v<descriptor_type>)
                        static_assert(std::same_as<
                            std::remove_cv_t<typename descriptor_type::owner_type>,
                            std::remove_cv_t<Parameters>>);
                    if constexpr (is_parameter_field_v<descriptor_type>)
                        std::invoke(
                            function,
                            descriptors,
                            parameters.*descriptor_type::member);
                }(),
                ...);
        },
        detail::schema_v<Parameters>);
}

namespace detail
{

template<class Schema, std::size_t... Index>
consteval std::size_t descriptor_index(
    const std::string_view name,
    std::index_sequence<Index...>) noexcept
{
    std::size_t result = sizeof...(Index);
    ((result == sizeof...(Index) && std::tuple_element_t<Index, Schema>::name() == name
             ? result = Index
             : result),
        ...);
    return result;
}

// The member at Path, from Begin, relative to block: a field, or a field of a
// nested group.
template<fixed_string Path, std::size_t Begin = 0, class Block>
[[nodiscard]]
constexpr const auto& field_at(const Block& block) noexcept
{
    constexpr auto path = Path.view();
    constexpr auto dot = path.find('.', Begin);
    constexpr auto name = path.substr(
        Begin,
        dot == std::string_view::npos ? std::string_view::npos : dot - Begin);
    using schema_type = std::remove_cvref_t<decltype(schema_v<Block>)>;
    constexpr auto size = std::tuple_size_v<schema_type>;
    constexpr auto index =
        descriptor_index<schema_type>(name, std::make_index_sequence<size>{});
    static_assert(
        index < size,
        "value<path>: no field or group of the block has this name");
    using descriptor_type = std::tuple_element_t<index, schema_type>;
    const auto& member = block.*descriptor_type::member;
    if constexpr (dot == std::string_view::npos)
    {
        static_assert(
            !is_parameter_group_v<descriptor_type>,
            "value<path>: the path names a group, not a field");
        return member;
    }
    else
    {
        static_assert(
            is_parameter_group_v<descriptor_type>,
            "value<path>: only a group has fields after a dot");
        return field_at<Path, dot + 1>(member);
    }
}

// Reads the value_references of an expression on block.
template<class Block>
struct field_reader
{
    const Block& block;

    template<class Reference>
    [[nodiscard]]
    constexpr const auto& operator()(const Reference&) const noexcept
    {
        return field_at<Reference::path_literal>(block);
    }
};

} // namespace detail

/// The value of an expression on a block: its value_references are the block's
/// fields.
template<class Block, expression Expression>
[[nodiscard]]
constexpr auto evaluate(const Expression& node, const Block& block)
{
    const detail::field_reader<Block> read{block};
    return evaluate_with(node, read);
}

/// Whether a field matters for the block, by the condition of its descriptor.
template<class Descriptor, class Block>
[[nodiscard]]
constexpr bool is_active(const Descriptor& descriptor, const Block& block)
{
    if constexpr (std::same_as<
                      std::remove_cvref_t<decltype(descriptor.condition)>,
                      no_condition>)
        return true;
    else
        return static_cast<bool>(evaluate(descriptor.condition, block));
}

namespace detail
{

// "name is out of its range" or "name is not one of its values", as static
// text that a validation_result can refer to.
template<class Descriptor>
struct out_of_domain_message
{
    static constexpr std::string_view suffix =
        is_range_domain_v<typename Descriptor::domain_type>
        ? std::string_view{" is out of its range"}
        : std::string_view{" is not one of its values"};
    static constexpr auto text = [] {
        constexpr auto name = Descriptor::name();
        std::array<char, name.size() + suffix.size()> result{};
        for (std::size_t index = 0; index < name.size(); ++index)
            result[index] = name[index];
        for (std::size_t index = 0; index < suffix.size(); ++index)
            result[name.size() + index] = suffix[index];
        return result;
    }();
    static constexpr std::string_view value{text.data(), text.size()};
};

} // namespace detail

/// Whether a block meets its schema, and the first reason it does not: each
/// field that matters lies in its domain, each requirement holds, and then each
/// nested group is valid.
///
/// It is the check a validate() makes for what its schema declares.
///
/// A group is checked with its own validate(), or with check_schema when it
/// has none, so that a block made in the code is checked whole.
template<class Block>
[[nodiscard]]
constexpr validation_result check_schema(const Block& block) noexcept
{
    validation_result result;
    const auto& schema = detail::schema_v<Block>;
    std::apply(
        [&](const auto&... descriptors) {
            (
                [&] {
                    using descriptor_type = std::remove_cvref_t<decltype(descriptors)>;
                    if constexpr (is_parameter_requirement_v<descriptor_type>)
                    {
                        if (result
                            && !static_cast<bool>(
                                evaluate(descriptors.expression, block)))
                            result = validation_result::failure(descriptors.message);
                    }
                    else if constexpr (!is_parameter_group_v<descriptor_type>)
                    {
                        if (result && is_active(descriptors, block)
                            && !domain_contains(
                                descriptors.domain,
                                block.*descriptor_type::member))
                        {
                            result = validation_result::failure(
                                detail::out_of_domain_message<descriptor_type>::value);
                        }
                    }
                }(),
                ...);
        },
        schema);
    if (!result)
        return result;
    std::apply(
        [&](const auto&... descriptors) {
            (
                [&] {
                    using descriptor_type = std::remove_cvref_t<decltype(descriptors)>;
                    if constexpr (is_parameter_group_v<descriptor_type>)
                    {
                        if (!result)
                            return;
                        const auto& nested = block.*descriptor_type::member;
                        if constexpr (requires { nested.validate(); })
                            result = nested.validate();
                        else
                            result = check_schema(nested);
                    }
                }(),
                ...);
        },
        schema);
    return result;
}

} // namespace easylocal::config
