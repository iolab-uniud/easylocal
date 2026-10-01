#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <source_location>
#include <string_view>

namespace easylocal::logging
{

enum class level : std::uint8_t
{
    trace,
    debug,
    info,
    warning,
    error,
};

enum class origin : std::uint8_t
{
    framework,
    application,
};

struct record
{
    level severity{};
    origin source{origin::application};
    std::string_view category{};
    std::string_view message{};
    std::source_location location{};
};

// A sink is invoked synchronously. String views in a record are guaranteed to
// remain valid only for the duration of the call. Sinks must not throw.
using sink = void (*)(const record&) noexcept;

[[nodiscard]]
constexpr auto level_name(const level severity) noexcept -> std::string_view
{
    switch (severity)
    {
    case level::trace:
        return "trace";
    case level::debug:
        return "debug";
    case level::info:
        return "info";
    case level::warning:
        return "warning";
    case level::error:
        return "error";
    }

    return "unknown";
}

inline void stderr_sink(const record& entry) noexcept
{
    if (entry.severity < level::warning)
    {
        return;
    }

    const auto severity = level_name(entry.severity);
    std::fputs("EasyLocal ", stderr);
    std::fwrite(severity.data(), sizeof(char), severity.size(), stderr);
    std::fputs(": ", stderr);
    std::fwrite(
        entry.message.data(),
        sizeof(char),
        entry.message.size(),
        stderr);
    std::fputc('\n', stderr);
}

namespace detail
{

inline std::atomic<sink> active_sink{&stderr_sink};

} // namespace detail

[[nodiscard]]
inline auto current_sink() noexcept -> sink
{
    return detail::active_sink.load(std::memory_order_acquire);
}

[[nodiscard]]
inline auto set_sink(const sink target) noexcept -> sink
{
    return detail::active_sink.exchange(target, std::memory_order_acq_rel);
}

inline void emit(
    const level severity,
    const std::string_view category,
    const std::string_view message,
    const origin source = origin::application,
    const std::source_location location = std::source_location::current()) noexcept
{
    if (const auto target = current_sink(); target != nullptr)
    {
        target(record{
            .severity = severity,
            .source = source,
            .category = category,
            .message = message,
            .location = location,
        });
    }
}

} // namespace easylocal::logging
