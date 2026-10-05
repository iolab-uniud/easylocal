#pragma once

// detail::throttled_clock: readings of a clock at an interval of checks that
// adapts, so that a fast loop reads it rarely. A search_run's deadline and the
// time-based annealing schedule read their clock through it.

#include <chrono>
#include <cstddef>
#include <optional>

namespace easylocal::detail
{

// The clock is read at the first check, then at an interval of checks that
// adapts so that readings come about a millisecond apart: it doubles while
// they come sooner, halves while they come later. A fast loop reads the clock
// rarely; a slow one, at every check.
template<class Clock = std::chrono::steady_clock>
class throttled_clock
{
public:
    using time_point = typename Clock::time_point;

    // A reading of the clock when this check is due for one, else none.
    [[nodiscard]]
    std::optional<time_point> due()
    {
        if (++checks_since_reading_ < interval_)
            return std::nullopt;
        return now();
    }

    // A reading of the clock now, which restarts the count of checks.
    [[nodiscard]]
    time_point now()
    {
        checks_since_reading_ = 0;
        const auto reading = Clock::now();
        if (read_)
        {
            const auto gap = reading - last_reading_;
            if (gap < spacing / 2 && interval_ < max_interval)
                interval_ *= 2;
            else if (gap > spacing * 2 && interval_ > 1)
                interval_ /= 2;
        }
        last_reading_ = reading;
        read_ = true;
        return reading;
    }

    // Forgets the readings: the next check reads the clock.
    void reset() noexcept
    {
        read_ = false;
        interval_ = 1;
        checks_since_reading_ = 0;
    }

private:
    // A millisecond, in microseconds so that its half is not zero.
    static constexpr std::chrono::microseconds spacing{1000};
    static constexpr std::size_t max_interval = std::size_t{1} << 20U;

    // The last reading, when read_ (not an optional, which GCC 15 at -O3
    // reports as maybe uninitialized).
    time_point last_reading_{};
    bool read_{false};
    std::size_t interval_{1};
    std::size_t checks_since_reading_{};
};

} // namespace easylocal::detail
