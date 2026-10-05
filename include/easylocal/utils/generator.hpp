#pragma once

/// \file
/// easylocal::generator<T>: a coroutine that yields values of type T, lazily,
/// as an input range.
///
/// It is std::generator<T> where the standard library provides it, and a
/// minimal equivalent otherwise (libc++ does not ship `<generator>` yet). Its
/// intended use is NeighborhoodExplorer::moves(): the moves are produced one at
/// a time, while the runner consumes them, instead of being materialized in a
/// container.

#include <version>

#if defined(__cpp_lib_generator)

#include <generator>

namespace easylocal
{

/// A coroutine that yields values of type T, lazily, as an input range:
/// std::generator<T>, which the standard library provides.
template<class T>
using generator = std::generator<T>;

} // namespace easylocal

#else

#include <coroutine>
#include <cstddef>
#include <exception>
#include <iterator>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>

namespace easylocal
{

/// The subset of std::generator<T> the framework relies on: co_yield of a
/// value, a single pass over the yielded values, exceptions propagated to the
/// consumer.
///
/// Unlike std::generator, values are always copied or moved into the coroutine
/// frame and co_yield ranges::elements_of(...) is not supported.
template<class T>
class generator : public std::ranges::view_interface<generator<T>>
{
    static_assert(
        std::is_object_v<T> && std::same_as<T, std::remove_cv_t<T>>,
        "easylocal::generator<T> supports non-const object types only");

public:
    /// The promise of the coroutine, used by the compiler.
    class promise_type
    {
    public:
        /// The generator of this coroutine.
        generator get_return_object() noexcept
        {
            return generator{handle_type::from_promise(*this)};
        }

        /// Suspends the coroutine at its start, until begin().
        static std::suspend_always initial_suspend() noexcept
        {
            return {};
        }

        /// Suspends the coroutine at its end, so that the generator destroys
        /// it.
        static std::suspend_always final_suspend() noexcept
        {
            return {};
        }

        /// Stores the value of a `co_yield` and suspends the coroutine.
        std::suspend_always yield_value(T value) noexcept(
            std::is_nothrow_move_constructible_v<T>)
        {
            value_.emplace(std::move(value));
            return {};
        }

        /// A generator only yields: co_await is not allowed in its body.
        template<class Awaitable>
        std::suspend_never await_transform(Awaitable&&) = delete;

        /// Ends the coroutine.
        static void return_void() noexcept {}

        /// Stores an exception thrown by the coroutine, which begin() or `++`
        /// rethrows.
        void unhandled_exception() noexcept
        {
            exception_ = std::current_exception();
        }

    private:
        friend class generator;

        std::optional<T> value_;
        std::exception_ptr exception_;
    };

    /// The handle of the coroutine.
    using handle_type = std::coroutine_handle<promise_type>;

    /// An input iterator over the yielded values.
    class iterator
    {
    public:
        /// The type of the values.
        using value_type = T;
        /// The difference type of the iterator.
        using difference_type = std::ptrdiff_t;
        /// An input iterator.
        using iterator_concept = std::input_iterator_tag;

        /// An iterator of no generator.
        iterator() = default;

        /// The current value, to be moved from.
        T&& operator*() const
        {
            return std::move(*coroutine_.promise().value_);
        }

        /// Resumes the coroutine until it yields the next value or ends.
        iterator& operator++()
        {
            advance(coroutine_);
            return *this;
        }

        /// The same, returning nothing.
        void operator++(int)
        {
            ++*this;
        }

        /// Whether the coroutine has ended.
        friend bool operator==(const iterator& current, std::default_sentinel_t) noexcept
        {
            return current.coroutine_.done();
        }

    private:
        friend class generator;

        explicit iterator(handle_type coroutine) noexcept : coroutine_{coroutine} {}

        handle_type coroutine_{};
    };

    /// Takes the coroutine of other, which is left empty.
    generator(generator&& other) noexcept
        : coroutine_{std::exchange(other.coroutine_, {})}
    {
    }

    /// Takes the coroutine of other, destroying its own.
    generator& operator=(generator other) noexcept
    {
        std::swap(coroutine_, other.coroutine_);
        return *this;
    }

    /// Destroys the coroutine.
    ~generator()
    {
        if (coroutine_)
        {
            coroutine_.destroy();
        }
    }

    /// Starts the coroutine: like std::generator, begin() may be called once.
    iterator begin()
    {
        advance(coroutine_);
        return iterator{coroutine_};
    }

    /// The end of the values: the coroutine has ended.
    static std::default_sentinel_t end() noexcept
    {
        return {};
    }

private:
    explicit generator(handle_type coroutine) noexcept : coroutine_{coroutine} {}

    static void advance(handle_type coroutine)
    {
        coroutine.promise().value_.reset();
        coroutine.resume();
        if (auto exception = std::exchange(coroutine.promise().exception_, {}))
        {
            std::rethrow_exception(std::move(exception));
        }
    }

    handle_type coroutine_;
};

} // namespace easylocal

#endif
