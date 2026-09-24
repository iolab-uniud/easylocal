#pragma once

#include <coroutine>
#include <cstddef>
#include <exception>
#include <iterator>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>

namespace easylocal::benchmark::neighborhood_traversal
{

template<class T>
class generator : public std::ranges::view_interface<generator<T>>
{
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    generator() = default;

    explicit generator(handle_type handle) noexcept
        : handle_{handle}
    {
    }

    generator(const generator&) = delete;
    auto operator=(const generator&) -> generator& = delete;

    generator(generator&& other) noexcept
        : handle_{std::exchange(other.handle_, {})},
          started_{std::exchange(other.started_, false)}
    {
    }

    auto operator=(generator&& other) noexcept -> generator&
    {
        if (this != &other)
        {
            if (handle_)
            {
                handle_.destroy();
            }

            handle_ = std::exchange(other.handle_, {});
            started_ = std::exchange(other.started_, false);
        }

        return *this;
    }

    ~generator()
    {
        if (handle_)
        {
            handle_.destroy();
        }
    }

    class iterator
    {
    public:
        using iterator_concept = std::input_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;

        iterator() = default;

        explicit iterator(handle_type handle) noexcept
            : handle_{handle}
        {
        }

        [[nodiscard]]
        auto operator*() const noexcept -> const T&
        {
            return *handle_.promise().current_;
        }

        auto operator++() -> iterator&
        {
            handle_.resume();
            rethrow_if_failed();
            return *this;
        }

        void operator++(int)
        {
            ++*this;
        }

        friend auto operator==(
            const iterator& current,
            std::default_sentinel_t) noexcept -> bool
        {
            return !current.handle_ || current.handle_.done();
        }

    private:
        void rethrow_if_failed() const
        {
            if (handle_ && handle_.done() && handle_.promise().failure_)
            {
                std::rethrow_exception(handle_.promise().failure_);
            }
        }

        handle_type handle_{};
    };

    [[nodiscard]]
    auto begin() -> iterator
    {
        if (handle_ && !started_)
        {
            started_ = true;
            handle_.resume();

            if (handle_.done() && handle_.promise().failure_)
            {
                std::rethrow_exception(handle_.promise().failure_);
            }
        }

        return iterator{handle_};
    }

    [[nodiscard]]
    auto end() const noexcept -> std::default_sentinel_t
    {
        return {};
    }

    struct promise_type
    {
        std::optional<T> current_;
        std::exception_ptr failure_;

        [[nodiscard]]
        auto get_return_object() noexcept -> generator
        {
            return generator{handle_type::from_promise(*this)};
        }

        [[nodiscard]]
        static auto initial_suspend() noexcept -> std::suspend_always
        {
            return {};
        }

        [[nodiscard]]
        static auto final_suspend() noexcept -> std::suspend_always
        {
            return {};
        }

        void return_void() const noexcept
        {
        }

        void unhandled_exception() noexcept
        {
            failure_ = std::current_exception();
        }

        [[nodiscard]]
        auto yield_value(T value) noexcept(std::is_nothrow_move_constructible_v<T>)
            -> std::suspend_always
        {
            current_.emplace(std::move(value));
            return {};
        }
    };

private:
    handle_type handle_{};
    bool started_{false};
};

} // namespace easylocal::benchmark::neighborhood_traversal
