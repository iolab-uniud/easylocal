#pragma once

/// \file
/// Reading and writing a problem's values through its optional hooks: an Input
/// from a stream or a file, a Solution from and to a stream or a file, and the
/// text that describes an Input, a Solution or a Move.
///
/// Session and the TextUI use the same functions; a program without them calls
/// them directly.

#include <easylocal/utils/detail/describe.hpp>

#include <concepts>
#include <exception>
#include <filesystem>
#include <fstream>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace easylocal
{

namespace detail::io
{

namespace adl
{

void read_input() = delete;
void read_solution() = delete;
void write_solution() = delete;

template<class Input>
concept has_read_input = requires(std::istream& in) {
    { read_input(std::type_identity<Input>{}, in) } -> std::convertible_to<Input>;
};

template<class Input>
    requires has_read_input<Input>
[[nodiscard]]
Input call_read_input(std::istream& in)
{
    return read_input(std::type_identity<Input>{}, in);
}

template<class Input, class Solution>
concept has_read_solution = requires(const Input& input, std::istream& in) {
    { read_solution(input, in) } -> std::convertible_to<Solution>;
};

template<class Input, class Solution>
    requires has_read_solution<Input, Solution>
[[nodiscard]]
Solution call_read_solution(const Input& input, std::istream& in)
{
    return read_solution(input, in);
}

template<class Input, class Solution>
concept has_write_solution =
    requires(const Input& input, const Solution& solution, std::ostream& out) {
        write_solution(input, solution, out);
    };

template<class Input, class Solution>
    requires has_write_solution<Input, Solution>
void call_write_solution(const Input& input, const Solution& solution, std::ostream& out)
{
    write_solution(input, solution, out);
}

} // namespace adl

template<class Input>
concept has_static_input_read = requires(std::istream& in) {
    { Input::read(in) } -> std::convertible_to<Input>;
};

template<class Input>
concept has_input_stream_extraction = std::default_initializable<Input>
    && requires(std::istream& in, Input& input) { in >> input; };

template<class Input, class Solution>
concept has_static_solution_read = requires(const Input& input, std::istream& in) {
    { Solution::read(input, in) } -> std::convertible_to<Solution>;
};

template<class Input, class Solution>
concept has_solution_stream_extraction = std::constructible_from<Solution, const Input&>
    && requires(std::istream& in, Solution& solution) { in >> solution; };

template<class Input, class Solution>
concept has_member_solution_write =
    requires(const Input& input, const Solution& solution, std::ostream& out) {
        solution.write(input, out);
    };

inline void require_read_success(const std::istream& in, const std::string_view what)
{
    if (in.fail())
        throw std::runtime_error{"failed to read " + std::string{what}};
}

inline void require_write_success(const std::ostream& out, const std::string_view what)
{
    if (out.fail())
        throw std::runtime_error{"failed to write " + std::string{what}};
}

// Reads a file with read(stream); an error names the file.
template<class Read>
[[nodiscard]]
auto read_file(const std::filesystem::path& path, const std::string_view what, Read read)
{
    std::ifstream in{path};
    if (!in)
        throw std::runtime_error{
            "failed to open " + std::string{what} + " file: " + path.string()};
    try
    {
        return read(in);
    }
    catch (const std::exception& error)
    {
        throw std::runtime_error{path.string() + ": " + error.what()};
    }
}

} // namespace detail::io

/// An Input that can be read from a stream: by a static Input::read(in), by a
/// free read_input(std::type_identity<Input>, in) found by ADL, or by
/// operator>> on a default-constructed Input, in this order.
template<class Input>
concept readable_input =
    detail::io::has_static_input_read<Input> || detail::io::adl::has_read_input<Input>
    || detail::io::has_input_stream_extraction<Input>;

/// A Solution that can be read from a stream, given its Input: by a static
/// Solution::read(input, in), by a free read_solution(input, in) found by ADL,
/// or by operator>> on Solution{input}, in this order.
template<class Input, class Solution>
concept readable_solution = detail::io::has_static_solution_read<Input, Solution>
    || detail::io::adl::has_read_solution<Input, Solution>
    || detail::io::has_solution_stream_extraction<Input, Solution>;

/// A Solution that can be written to a stream, given its Input: by a member
/// solution.write(input, out), by a free write_solution(input, solution, out)
/// found by ADL, or by operator<<, in this order.
template<class Input, class Solution>
concept writable_solution = detail::io::has_member_solution_write<Input, Solution>
    || detail::io::adl::has_write_solution<Input, Solution>
    || detail::io::ostream_insertable<Solution>;

/// A value with a describe hook of its own: a member value.describe(), or a
/// free describe(value) found by ADL; operator<< does not count.
template<class T>
concept has_describe =
    detail::io::member_describable<T> || detail::io::adl::has_describe<T>;

/// A value with a text for people: a member value.describe(), a free
/// describe(value) found by ADL, or operator<<, in this order.
template<class T>
concept describable = has_describe<T> || detail::io::ostream_insertable<T>;

/// Reads an Input from a stream with the first hook of readable_input.
///
/// Throws std::runtime_error when the stream fails, or what the hook throws.
template<class Input>
    requires readable_input<Input>
[[nodiscard]]
Input read_input(std::istream& in)
{
    auto input = [&]() -> Input {
        if constexpr (detail::io::has_static_input_read<Input>)
        {
            return Input::read(in);
        }
        else if constexpr (detail::io::adl::has_read_input<Input>)
        {
            return detail::io::adl::call_read_input<Input>(in);
        }
        else
        {
            Input result{};
            in >> result;
            return result;
        }
    }();
    detail::io::require_read_success(in, "Input");
    return input;
}

/// Reads an Input from a file, as read_input does from a stream.
///
/// Throws std::runtime_error, naming the file, when it cannot be opened or
/// read.
template<class Input>
    requires readable_input<Input>
[[nodiscard]]
Input load_input(const std::filesystem::path& path)
{
    return detail::io::read_file(path, "Input", [](std::istream& in) {
        return read_input<Input>(in);
    });
}

/// Reads a Solution of input from a stream with the first hook of
/// readable_solution.
///
/// Throws std::runtime_error when the stream fails, or what the hook throws.
template<class Solution, class Input>
    requires readable_solution<Input, Solution>
[[nodiscard]]
Solution read_solution(const Input& input, std::istream& in)
{
    auto solution = [&]() -> Solution {
        if constexpr (detail::io::has_static_solution_read<Input, Solution>)
        {
            return Solution::read(input, in);
        }
        else if constexpr (detail::io::adl::has_read_solution<Input, Solution>)
        {
            return detail::io::adl::call_read_solution<Input, Solution>(input, in);
        }
        else
        {
            Solution result{input};
            in >> result;
            return result;
        }
    }();
    detail::io::require_read_success(in, "Solution");
    return solution;
}

/// Reads a Solution of input from a file, as read_solution does from a stream.
///
/// Throws std::runtime_error, naming the file, when it cannot be opened or
/// read.
template<class Solution, class Input>
    requires readable_solution<Input, Solution>
[[nodiscard]]
Solution load_solution(const Input& input, const std::filesystem::path& path)
{
    return detail::io::read_file(path, "Solution", [&](std::istream& in) {
        return read_solution<Solution>(input, in);
    });
}

namespace detail::io
{

// easylocal::write_solution: a function object, so that the argument-dependent
// lookup of a problem's write_solution hook never finds it, even for types
// with easylocal among their associated namespaces.
struct write_solution_fn
{
    template<class Input, class Solution>
        requires writable_solution<Input, Solution>
    void operator()(const Input& input, const Solution& solution, std::ostream& out) const
    {
        if constexpr (has_member_solution_write<Input, Solution>)
            solution.write(input, out);
        else if constexpr (adl::has_write_solution<Input, Solution>)
            adl::call_write_solution(input, solution, out);
        else
            out << solution;
        require_write_success(out, "Solution");
    }
};

// easylocal::describe: a function object, for the same reason.
struct describe_fn
{
    template<class T>
        requires describable<T>
    [[nodiscard]]
    std::string operator()(const T& value) const
    {
        return describe_text(value);
    }
};

} // namespace detail::io

/// The function objects of the I/O hooks, easylocal::write_solution and
/// easylocal::describe: an inline namespace keeps them apart from functions of
/// the same name declared as friends in easylocal.
inline namespace io_functions
{

/// Writes a Solution of input to a stream with the first hook of
/// writable_solution: `write_solution(input, solution, out)`.
///
/// Throws std::runtime_error when the stream fails. It is a function object,
/// which the lookup of a problem's own write_solution hook does not find.
inline constexpr detail::io::write_solution_fn write_solution{};

/// The text of a value for people, with the first hook of describable:
/// `describe(value)`.
///
/// It is a function object, which the lookup of a problem's own describe hook
/// does not find.
inline constexpr detail::io::describe_fn describe{};

} // namespace io_functions

/// Writes a Solution of input to a file, as write_solution does to a stream.
///
/// Throws std::runtime_error, naming the file, when it cannot be opened,
/// written, flushed or closed.
template<class Input, class Solution>
    requires writable_solution<Input, Solution>
void save_solution(
    const Input& input,
    const Solution& solution,
    const std::filesystem::path& path)
{
    std::ofstream out{path};
    if (!out)
        throw std::runtime_error{"failed to open Solution file: " + path.string()};
    try
    {
        write_solution(input, solution, out);
    }
    catch (const std::exception& error)
    {
        throw std::runtime_error{path.string() + ": " + error.what()};
    }
    // The data may reach the file only when it is flushed and closed.
    out.close();
    if (out.fail())
        throw std::runtime_error{"failed to write Solution file: " + path.string()};
}

} // namespace easylocal
