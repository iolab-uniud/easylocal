#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <type_traits>
#include <utility>

namespace easylocal
{

struct run_progress
{
    std::size_t evaluations{};
    std::size_t iterations{};
    std::optional<std::size_t> evaluation_limit;
};

class run_control
{
public:
    run_control() noexcept = default;

    explicit run_control(std::stop_token stop_token) noexcept
        : stop_token_{std::move(stop_token)}
    {
    }

    template<class Observer>
        requires std::invocable<Observer&, const run_progress&>
    run_control(std::stop_token stop_token, Observer& observer) noexcept
        : stop_token_{std::move(stop_token)},
          observer_state_{std::addressof(observer)},
          observer_{[](void* state, const run_progress& progress) {
              std::invoke(*static_cast<Observer*>(state), progress);
          }}
    {
    }

    [[nodiscard]] auto stop_requested() const noexcept -> bool
    {
        return stop_token_.stop_requested();
    }

    [[nodiscard]] auto stop_possible() const noexcept -> bool
    {
        return stop_token_.stop_possible();
    }

    [[nodiscard]] auto observes_progress() const noexcept -> bool
    {
        return observer_ != nullptr;
    }

    void report(const run_progress& progress) const
    {
        if (observer_ != nullptr)
        {
            observer_(observer_state_, progress);
        }
    }

private:
    using observer_type = void (*)(void*, const run_progress&);

    std::stop_token stop_token_{};
    void* observer_state_{};
    observer_type observer_{};
};

} // namespace easylocal
