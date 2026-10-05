#pragma once

/// \file
/// Logging (easylocal::logging): records with a level and an origin sent to
/// one process-wide sink, stderr unless set_sink replaces it
/// (docs/logging.md).
///
/// Experimental: the library itself emits no records yet.

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <source_location>
#include <string_view>

namespace easylocal::logging
{

/// The severity of a log record, from the lowest.
enum class level : std::uint8_t
{
    /// Fine-grained tracing.
    trace,
    /// Debugging information.
    debug,
    /// Normal operation.
    info,
    /// Something unexpected, that does not stop the program.
    warning,
    /// A failure.
    error,
};

/// Who emits a log record.
enum class origin : std::uint8_t
{
    /// EasyLocal itself.
    framework,
    /// The program that uses EasyLocal.
    application,
};

/// A log record, as a sink receives it.
struct record
{
    /// The level of the record.
    level severity{};
    /// Who emitted it.
    origin source{origin::application};
    /// Its category, e.g. `application.model`.
    std::string_view category{};
    /// Its message.
    std::string_view message{};
    /// Where it was emitted.
    std::source_location location{};
};

/// A sink is invoked synchronously.
///
/// String views in a record are guaranteed to remain valid only for the
/// duration of the call. Sinks must not throw.
using sink = void (*)(const record&) noexcept;

/// The name of a level, e.g. "warning".
[[nodiscard]]
constexpr std::string_view level_name(const level severity) noexcept
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

/// The default sink: writes warnings and errors to stderr, as
/// `EasyLocal <level>: <message>`, one write per record, and ignores the
/// other records.
inline void stderr_sink(const record& entry) noexcept
{
    if (entry.severity < level::warning)
    {
        return;
    }

    const auto severity = level_name(entry.severity);
    // The line in one write, which the records of other threads do not split;
    // a message too long for the buffer goes piece by piece.
    static constexpr std::string_view prefix{"EasyLocal "};
    std::array<char, 1024> line{};
    const auto size = prefix.size() + severity.size() + 2 + entry.message.size() + 1;
    if (size <= line.size())
    {
        auto* end = std::ranges::copy(prefix, line.data()).out;
        end = std::ranges::copy(severity, end).out;
        *end++ = ':';
        *end++ = ' ';
        end = std::ranges::copy(entry.message, end).out;
        *end = '\n';
        std::fwrite(line.data(), sizeof(char), size, stderr);
        return;
    }
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

/// The active sink, or a null pointer when dispatch is disabled.
[[nodiscard]]
inline sink current_sink() noexcept
{
    return detail::active_sink.load(std::memory_order_acquire);
}

/// Replaces the active sink, and returns the previous one; `nullptr` disables
/// dispatch.
inline sink set_sink(const sink target) noexcept
{
    return detail::active_sink.exchange(target, std::memory_order_acq_rel);
}

/// Sends a record to the active sink, if any; its location defaults to the
/// call site.
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
