#pragma once

/// \file
/// Expressions over the parameters of a block, for the conditions of its fields
/// (`field(...).only_if(...)`) and the requirements between them
/// (config::require): `config::value<"calibration_samples"> > 0`.
///
/// An expression is evaluated on a block and written as text, for automatic
/// configurators such as irace.

#include <easylocal/config/fixed_string.hpp>
#include <easylocal/utils/detail/number_text.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::config
{

/// A parameter of the block an expression belongs to, by its path relative to
/// the block: `value<"cooling_rate">`, or `value<"temperature.cooling_rate">`
/// for a field of a nested group.
template<fixed_string Path>
struct value_reference
{
    /// The path, as a template argument.
    static constexpr auto path_literal = Path;

    /// The path relative to the block.
    [[nodiscard]]
    static constexpr std::string_view path() noexcept
    {
        return Path.view();
    }
};

/// The parameter at Path, relative to the block, in an expression:
/// `value<"final_temperature"> < value<"initial_temperature">`.
template<fixed_string Path>
inline constexpr value_reference<Path> value{};

/// A constant in an expression: a number, a bool or a text.
template<class Value>
struct constant_expression
{
    /// The constant.
    Value value;
};

/// The operators of expressions.
enum class expression_operator
{
    /// `<`
    less,
    /// `<=`
    less_equal,
    /// `>`
    greater,
    /// `>=`
    greater_equal,
    /// `==`
    equal,
    /// `!=`
    not_equal,
    /// `&&`
    logical_and,
    /// `||`
    logical_or,
    /// `+`
    plus,
    /// `-`
    minus,
    /// `*`
    multiplies,
    /// `/`
    divides,
    /// `!`, unary.
    logical_not,
    /// `-`, unary.
    negate,
};

/// The symbol of an operator, the same in C++ and in R.
[[nodiscard]]
constexpr std::string_view operator_symbol(const expression_operator op) noexcept
{
    switch (op)
    {
    case expression_operator::less:
        return "<";
    case expression_operator::less_equal:
        return "<=";
    case expression_operator::greater:
        return ">";
    case expression_operator::greater_equal:
        return ">=";
    case expression_operator::equal:
        return "==";
    case expression_operator::not_equal:
        return "!=";
    case expression_operator::logical_and:
        return "&&";
    case expression_operator::logical_or:
        return "||";
    case expression_operator::plus:
        return "+";
    case expression_operator::minus:
    case expression_operator::negate:
        return "-";
    case expression_operator::multiplies:
        return "*";
    case expression_operator::divides:
        return "/";
    case expression_operator::logical_not:
        return "!";
    }
    return "?";
}

/// An operator applied to two expressions.
template<expression_operator Op, class Left, class Right>
struct binary_expression
{
    /// The left operand.
    Left left;
    /// The right operand.
    Right right;
};

/// An operator applied to an expression.
template<expression_operator Op, class Operand>
struct unary_expression
{
    /// The operand.
    Operand operand;
};

/// Whether a type is an expression: a value_reference, a constant or an
/// operator applied to expressions.
template<class T>
inline constexpr bool is_expression_v = false;

/// A value_reference is one.
template<fixed_string Path>
inline constexpr bool is_expression_v<value_reference<Path>> = true;

/// A constant is one.
template<class Value>
inline constexpr bool is_expression_v<constant_expression<Value>> = true;

/// An operator on two expressions is one.
template<expression_operator Op, class Left, class Right>
inline constexpr bool is_expression_v<binary_expression<Op, Left, Right>> = true;

/// An operator on an expression is one.
template<expression_operator Op, class Operand>
inline constexpr bool is_expression_v<unary_expression<Op, Operand>> = true;

/// An expression, as `only_if` and `require` take it.
template<class T>
concept expression = is_expression_v<std::remove_cvref_t<T>>;

namespace detail
{

template<class T>
concept expression_constant =
    std::same_as<std::remove_cvref_t<T>, bool> || std::integral<std::remove_cvref_t<T>>
    || std::floating_point<std::remove_cvref_t<T>>
    || std::convertible_to<const T&, std::string_view>;

template<class T>
concept expression_operand = expression<T> || expression_constant<T>;

template<class T>
[[nodiscard]]
constexpr auto as_expression(const T& operand)
{
    if constexpr (expression<T>)
        return operand;
    else if constexpr (std::convertible_to<const T&, std::string_view>
        && !std::is_arithmetic_v<T>)
        return constant_expression<std::string_view>{std::string_view{operand}};
    else
        return constant_expression<T>{operand};
}

template<expression_operator Op, class Left, class Right>
[[nodiscard]]
constexpr auto make_binary(const Left& left, const Right& right)
{
    using left_type = decltype(as_expression(left));
    using right_type = decltype(as_expression(right));
    return binary_expression<Op, left_type, right_type>{
        as_expression(left),
        as_expression(right)};
}

// An operand as it is computed with, as R computes it: a number as a double
// (a floating-point number keeps its type), a limit as its count and
// unlimited as +infinity, a bool and a text as they are.
template<class T>
[[nodiscard]]
constexpr auto computed(const T& operand)
{
    if constexpr (std::same_as<T, bool> || std::floating_point<T>)
        return operand;
    else if constexpr (std::integral<T>)
        return static_cast<double>(operand);
    else if constexpr (std::convertible_to<const T&, std::string_view>)
        return std::string_view{operand};
    else if constexpr (std::convertible_to<const T&, std::size_t>
        && requires { operand.is_unlimited(); })
        return operand.is_unlimited()
            ? std::numeric_limits<double>::infinity()
            : static_cast<double>(static_cast<std::size_t>(operand));
    else if constexpr (std::convertible_to<const T&, std::size_t>)
        return static_cast<double>(static_cast<std::size_t>(operand));
    else
        return operand;
}

} // namespace detail

/// `left < right`, when one of them is an expression.
///
/// Requires operands that are expressions, numbers, booleans, texts or
/// limits.
template<class Left, class Right>
    requires(expression<Left> || expression<Right>)
    && detail::expression_operand<Left> && detail::expression_operand<Right>
[[nodiscard]]
constexpr auto operator<(const Left& left, const Right& right)
{
    return detail::make_binary<expression_operator::less>(left, right);
}

/// `left <= right`, when one of them is an expression.
///
/// Requires operands that are expressions, numbers, booleans, texts or
/// limits.
template<class Left, class Right>
    requires(expression<Left> || expression<Right>)
    && detail::expression_operand<Left> && detail::expression_operand<Right>
[[nodiscard]]
constexpr auto operator<=(const Left& left, const Right& right)
{
    return detail::make_binary<expression_operator::less_equal>(left, right);
}

/// `left > right`, when one of them is an expression.
///
/// Requires operands that are expressions, numbers, booleans, texts or
/// limits.
template<class Left, class Right>
    requires(expression<Left> || expression<Right>)
    && detail::expression_operand<Left> && detail::expression_operand<Right>
[[nodiscard]]
constexpr auto operator>(const Left& left, const Right& right)
{
    return detail::make_binary<expression_operator::greater>(left, right);
}

/// `left >= right`, when one of them is an expression.
///
/// Requires operands that are expressions, numbers, booleans, texts or
/// limits.
template<class Left, class Right>
    requires(expression<Left> || expression<Right>)
    && detail::expression_operand<Left> && detail::expression_operand<Right>
[[nodiscard]]
constexpr auto operator>=(const Left& left, const Right& right)
{
    return detail::make_binary<expression_operator::greater_equal>(left, right);
}

/// `left == right`, when one of them is an expression.
///
/// Requires operands that are expressions, numbers, booleans, texts or
/// limits.
template<class Left, class Right>
    requires(expression<Left> || expression<Right>)
    && detail::expression_operand<Left> && detail::expression_operand<Right>
[[nodiscard]]
constexpr auto operator==(const Left& left, const Right& right)
{
    return detail::make_binary<expression_operator::equal>(left, right);
}

/// `left != right`, when one of them is an expression.
///
/// Requires operands that are expressions, numbers, booleans, texts or
/// limits.
template<class Left, class Right>
    requires(expression<Left> || expression<Right>)
    && detail::expression_operand<Left> && detail::expression_operand<Right>
[[nodiscard]]
constexpr auto operator!=(const Left& left, const Right& right)
{
    return detail::make_binary<expression_operator::not_equal>(left, right);
}

/// `left && right`, when one of them is an expression.
///
/// Requires operands that are expressions, numbers, booleans, texts or
/// limits.
template<class Left, class Right>
    requires(expression<Left> || expression<Right>)
    && detail::expression_operand<Left> && detail::expression_operand<Right>
[[nodiscard]]
constexpr auto operator&&(const Left& left, const Right& right)
{
    return detail::make_binary<expression_operator::logical_and>(left, right);
}

/// `left || right`, when one of them is an expression.
///
/// Requires operands that are expressions, numbers, booleans, texts or
/// limits.
template<class Left, class Right>
    requires(expression<Left> || expression<Right>)
    && detail::expression_operand<Left> && detail::expression_operand<Right>
[[nodiscard]]
constexpr auto operator||(const Left& left, const Right& right)
{
    return detail::make_binary<expression_operator::logical_or>(left, right);
}

/// `left + right`, when one of them is an expression.
///
/// Requires operands that are expressions, numbers, booleans, texts or
/// limits.
template<class Left, class Right>
    requires(expression<Left> || expression<Right>)
    && detail::expression_operand<Left> && detail::expression_operand<Right>
[[nodiscard]]
constexpr auto operator+(const Left& left, const Right& right)
{
    return detail::make_binary<expression_operator::plus>(left, right);
}

/// `left - right`, when one of them is an expression.
///
/// Requires operands that are expressions, numbers, booleans, texts or
/// limits.
template<class Left, class Right>
    requires(expression<Left> || expression<Right>)
    && detail::expression_operand<Left> && detail::expression_operand<Right>
[[nodiscard]]
constexpr auto operator-(const Left& left, const Right& right)
{
    return detail::make_binary<expression_operator::minus>(left, right);
}

/// `left * right`, when one of them is an expression.
///
/// Requires operands that are expressions, numbers, booleans, texts or
/// limits.
template<class Left, class Right>
    requires(expression<Left> || expression<Right>)
    && detail::expression_operand<Left> && detail::expression_operand<Right>
[[nodiscard]]
constexpr auto operator*(const Left& left, const Right& right)
{
    return detail::make_binary<expression_operator::multiplies>(left, right);
}

/// `left / right`, when one of them is an expression.
///
/// Requires operands that are expressions, numbers, booleans, texts or
/// limits.
template<class Left, class Right>
    requires(expression<Left> || expression<Right>)
    && detail::expression_operand<Left> && detail::expression_operand<Right>
[[nodiscard]]
constexpr auto operator/(const Left& left, const Right& right)
{
    return detail::make_binary<expression_operator::divides>(left, right);
}

/// `!operand`.
///
/// Requires an expression.
template<expression Operand>
[[nodiscard]]
constexpr auto operator!(const Operand& operand)
{
    return unary_expression<expression_operator::logical_not, Operand>{operand};
}

/// `-operand`.
///
/// Requires an expression.
template<expression Operand>
[[nodiscard]]
constexpr auto operator-(const Operand& operand)
{
    return unary_expression<expression_operator::negate, Operand>{operand};
}

/// The value of an expression, with read(reference) giving the value of each
/// value_reference.
///
/// Operators compute as R does, so that a condition means the same in the
/// library and in its irace export: numbers in double (`7 / 2` is 3.5), a
/// limit as its count and unlimited as +infinity, a minus after the
/// conversion (`-count` is negative).
template<class Expression, class Read>
    requires expression<Expression>
[[nodiscard]]
constexpr auto evaluate_with(const Expression& node, const Read& read)
{
    if constexpr (requires { Expression::path_literal; })
        return read(node);
    else if constexpr (requires { node.value; })
        return node.value;
    else if constexpr (requires { node.operand; })
    {
        if constexpr (std::same_as<
                          Expression,
                          unary_expression<
                              expression_operator::logical_not,
                              decltype(node.operand)>>)
            return !static_cast<bool>(evaluate_with(node.operand, read));
        else
            return -detail::computed(evaluate_with(node.operand, read));
    }
    else
    {
        return []<expression_operator Op, class Left, class Right>(
                   const binary_expression<Op, Left, Right>& binary,
                   const Read& reader) {
            // The values outlive left and right, which may view their text.
            const auto left_value = evaluate_with(binary.left, reader);
            const auto right_value = evaluate_with(binary.right, reader);
            const auto left = detail::computed(left_value);
            const auto right = detail::computed(right_value);
            if constexpr (Op == expression_operator::less)
                return left < right;
            else if constexpr (Op == expression_operator::less_equal)
                return left <= right;
            else if constexpr (Op == expression_operator::greater)
                return left > right;
            else if constexpr (Op == expression_operator::greater_equal)
                return left >= right;
            else if constexpr (Op == expression_operator::equal)
                return left == right;
            else if constexpr (Op == expression_operator::not_equal)
                return left != right;
            else if constexpr (Op == expression_operator::logical_and)
                return static_cast<bool>(left) && static_cast<bool>(right);
            else if constexpr (Op == expression_operator::logical_or)
                return static_cast<bool>(left) || static_cast<bool>(right);
            else if constexpr (Op == expression_operator::plus)
                return left + right;
            else if constexpr (Op == expression_operator::minus)
                return left - right;
            else if constexpr (Op == expression_operator::multiplies)
                return left * right;
            else
                return left / right;
        }(node, read);
    }
}

/// An expression at run time, whatever its type: what a parameter_set lists
/// and a configurator exporter writes.
struct expression_info
{
    /// The kinds of nodes.
    enum class node
    {
        /// A parameter, by its full path.
        reference,
        /// A number.
        number,
        /// A text.
        text,
        /// true or false.
        boolean,
        /// An operator on two operands.
        binary,
        /// An operator on one operand.
        unary,
    };

    /// The kind of the node.
    node kind{node::boolean};
    /// The full path of a reference, a constant as text, or the symbol of an
    /// operator.
    std::string text{};
    /// The operands of an operator.
    std::vector<expression_info> operands{};

    /// The expression as text, with each reference written by reference(path)
    /// and the constants as R and C++ read them: text in double quotes, with
    /// `"` and `\` escaped; with r_syntax, TRUE and FALSE for booleans and
    /// Inf, -Inf and NaN for the numbers that are not finite.
    [[nodiscard]]
    std::string text_with(
        const std::function<std::string(std::string_view path)>& reference,
        const bool r_syntax = true) const
    {
        switch (kind)
        {
        case node::reference:
            return reference(text);
        case node::number:
            if (r_syntax)
            {
                if (text == "inf")
                    return "Inf";
                if (text == "-inf")
                    return "-Inf";
                if (text == "nan" || text == "-nan")
                    return "NaN";
            }
            return text;
        case node::text:
        {
            std::string quoted{'"'};
            for (const char character : text)
            {
                if (character == '"' || character == '\\')
                    quoted += '\\';
                quoted += character;
            }
            return quoted + '"';
        }
        case node::boolean:
            return r_syntax ? (text == "true" ? "TRUE" : "FALSE") : text;
        case node::unary:
            return text + "(" + operands.front().text_with(reference, r_syntax) + ")";
        case node::binary:
            return "(" + operands[0].text_with(reference, r_syntax) + " " + text + " "
                + operands[1].text_with(reference, r_syntax) + ")";
        }
        return {};
    }

    /// The expression as text, with each reference by its path.
    [[nodiscard]]
    std::string to_string() const
    {
        return text_with(
            [](const std::string_view path) { return std::string{path}; },
            false);
    }

    /// The full paths of the parameters the expression refers to.
    [[nodiscard]]
    std::vector<std::string> references() const
    {
        std::vector<std::string> result;
        if (kind == node::reference)
            result.push_back(text);
        for (const auto& operand : operands)
            for (auto& path : operand.references())
                result.push_back(std::move(path));
        return result;
    }
};

/// The run-time description of an expression, with its references under
/// prefix, the path of the block it belongs to.
template<class Expression>
    requires expression<Expression>
[[nodiscard]]
expression_info describe_expression(const Expression& node, const std::string_view prefix)
{
    using node_kind = expression_info::node;
    if constexpr (requires { Expression::path_literal; })
    {
        std::string path{prefix};
        if (!path.empty())
            path += '.';
        path.append(Expression::path());
        return {.kind = node_kind::reference, .text = std::move(path)};
    }
    else if constexpr (requires { node.value; })
    {
        using value_type = std::remove_cvref_t<decltype(node.value)>;
        if constexpr (std::same_as<value_type, bool>)
            return {.kind = node_kind::boolean, .text = node.value ? "true" : "false"};
        else if constexpr (std::same_as<value_type, std::string_view>)
            return {.kind = node_kind::text, .text = std::string{node.value}};
        else
            return {
                .kind = node_kind::number,
                .text = easylocal::detail::number_text(node.value)};
    }
    else if constexpr (requires { node.operand; })
    {
        const bool logical_not = std::same_as<
            Expression,
            unary_expression<expression_operator::logical_not, decltype(node.operand)>>;
        return {
            .kind = node_kind::unary,
            .text = logical_not ? "!" : "-",
            .operands = {describe_expression(node.operand, prefix)}};
    }
    else
    {
        return []<expression_operator Op, class Left, class Right>(
                   const binary_expression<Op, Left, Right>& binary,
                   const std::string_view at) {
            return expression_info{
                .kind = node_kind::binary,
                .text = std::string{operator_symbol(Op)},
                .operands = {
                    describe_expression(binary.left, at),
                    describe_expression(binary.right, at)}};
        }(node, prefix);
    }
}

} // namespace easylocal::config
