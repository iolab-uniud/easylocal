#pragma once

// The check of the test programs, which collect its results and fail when one
// is false: `ok &= expect(condition, "what it checks");`.
#include <iostream>
#include <string_view>

// Whether condition holds; when it does not, writes "FAILED: " and the
// description to std::cerr.
[[nodiscard]]
inline bool expect(bool condition, std::string_view description)
{
    if (!condition)
        std::cerr << "FAILED: " << description << '\n';
    return condition;
}
