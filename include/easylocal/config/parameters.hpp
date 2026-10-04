#pragma once

/// \file
/// Parameter blocks: plain structs that describe their own fields with a
/// compile-time schema (config::field, config::group, config::fields), the
/// values each field may take (config::range, config::one_of), and check them
/// with validate().
///
/// Runners, neighborhoods, cost expressions and programs declare their
/// parameters this way.

#include <easylocal/config/domain.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::config
{

/// A string literal as a template argument: the name of a field or of a group.
template<std::size_t Size>
struct fixed_string
{
    /// The characters, with the terminating null.
    char value[Size]{};

    /// From a string literal.
    consteval fixed_string(const char (&text)[Size])
    {
        for (std::size_t index = 0; index < Size; ++index)
        {
            value[index] = text[index];
        }
    }

    /// The characters, without the terminating null.
    [[nodiscard]]
    constexpr std::string_view view() const noexcept
    {
        static_assert(Size >= 1);
        return {value, Size - 1};
    }
};

/// Deduces the size from the string literal.
template<std::size_t Size>
fixed_string(const char (&)[Size]) -> fixed_string<Size>;

/// Whether a parameter block is valid, and why not.
///
/// It converts to true when the block is valid.
struct validation_result
{
    /// Whether the block is valid.
    bool valid{true};
    /// Why the block is not valid; empty when it is.
    std::string_view message{};

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
        return []<class First, class... Rest>(std::type_identity<First>,
                                              std::type_identity<Rest>...) {
            return ((First::name() != Rest::name()) && ...) &&
                   unique_field_names<Rest...>();
        }(std::type_identity<Fields>{}...);
    }
}

} // namespace detail

/// The descriptor of a field of a parameter block: its name, its member, its
/// description and its domain, made with field().
template<fixed_string Name, auto Member, class Domain = no_domain>
    requires std::is_member_object_pointer_v<decltype(Member)>
struct parameter_field
{
    static_assert(
        !Name.view().empty(),
        "parameter field name must not be empty");

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

/// A member that is itself a parameter block, nested in the schema: its fields
/// are under "Name.", and its validate() runs with the enclosing block's, e.g.
/// group<"temperature", &AnnealingParameters::temperature>("Temperature schedule").
template<fixed_string Name, auto Member>
    requires std::is_member_object_pointer_v<decltype(Member)>
struct parameter_group
{
    static_assert(!Name.view().empty(), "parameter group name must not be empty");

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

/// The schema of a parameter block, as parameter_schema() returns it: its field
/// and group descriptors.
///
/// Their names must be unique within the block.
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

/// The parameter block of a configurable object, the type its parameters()
/// returns.
template<class T>
using configurable_parameters_t = std::remove_cvref_t<decltype(
    std::declval<const std::remove_cvref_t<T>&>().parameters())>;

/// An object configured with a parameter block: parameters() gives the current
/// one, configure() takes a new one and returns a validation_result.
///
/// configure() must accept every block whose validate() succeeds.
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

/// The fields of a block, without its nested groups: function(descriptor, value).
template<parameter_block Parameters, class Function>
constexpr void for_each_parameter(
    Parameters& parameters,
    Function&& function)
{
    auto schema = Parameters::parameter_schema();

    std::apply(
        [&parameters, &function](auto... descriptors) {
            (
                [&] {
                    using descriptor_type = decltype(descriptors);
                    static_assert(std::same_as<
                        std::remove_cv_t<typename descriptor_type::owner_type>,
                        std::remove_cv_t<Parameters>>);
                    if constexpr (!is_parameter_group_v<descriptor_type>)
                        std::invoke(
                            function,
                            descriptors,
                            parameters.*descriptor_type::member);
                }(),
                ...);
        },
        std::move(schema));
}

/// The same, on a const block.
template<parameter_block Parameters, class Function>
constexpr void for_each_parameter(
    const Parameters& parameters,
    Function&& function)
{
    auto schema = Parameters::parameter_schema();

    std::apply(
        [&parameters, &function](auto... descriptors) {
            (
                [&] {
                    using descriptor_type = decltype(descriptors);
                    static_assert(std::same_as<
                        std::remove_cv_t<typename descriptor_type::owner_type>,
                        std::remove_cv_t<Parameters>>);
                    if constexpr (!is_parameter_group_v<descriptor_type>)
                        std::invoke(
                            function,
                            descriptors,
                            parameters.*descriptor_type::member);
                }(),
                ...);
        },
        std::move(schema));
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

/// Whether each field of a block lies in the domain its schema declares, and
/// the first that does not: the check a validate() makes for those domains.
///
/// The fields of nested groups are left to the groups' own validate().
template<class Block>
[[nodiscard]]
constexpr validation_result check_domains(const Block& block) noexcept
{
    validation_result result;
    std::apply(
        [&](const auto&... descriptors) {
            (
                [&] {
                    using descriptor_type = std::remove_cvref_t<decltype(descriptors)>;
                    if constexpr (!is_parameter_group_v<descriptor_type>)
                    {
                        if (result
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
        Block::parameter_schema());
    return result;
}

} // namespace easylocal::config
