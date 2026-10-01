#pragma once

namespace easylocal
{

namespace initialization
{

// Static tags are useful when initialization is fixed by the program: an
// unsupported choice is then rejected at compile time.
struct Initial
{
};

struct Random
{
};

inline constexpr Initial initial{};
inline constexpr Random random{};

// Mode is the runtime-facing counterpart, suitable for CLI/configuration.
// Unsupported runtime selections are rejected explicitly; there is never an
// implicit fallback from one initialization mode to another.
enum class Mode
{
    initial,
    random,
};

} // namespace initialization

} // namespace easylocal
