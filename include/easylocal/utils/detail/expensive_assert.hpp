#pragma once

// EASYLOCAL_EXPENSIVE_ASSERT(condition): an assert that costs as much as the
// work it checks, such as the validity of the whole solution after each move
// evaluated. It checks only when EASYLOCAL_EXPENSIVE_CHECKS is defined (the
// CMake option of the same name defines it for EasyLocal's own tests and
// examples; a program defines it itself); otherwise the condition is not
// evaluated. Like assert, NDEBUG disables it.

#include <cassert>

#if defined(EASYLOCAL_EXPENSIVE_CHECKS)
#define EASYLOCAL_EXPENSIVE_ASSERT(...) assert(__VA_ARGS__)
#else
#define EASYLOCAL_EXPENSIVE_ASSERT(...) static_cast<void>(0)
#endif
