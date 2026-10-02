#pragma once

// Portable spellings of attributes whose standard form a toolchain ignores.
//
// The Microsoft ABI (MSVC, and clang-cl) ignores [[no_unique_address]] and
// honours [[msvc::no_unique_address]] instead.
#if defined(_MSC_VER)
#define EASYLOCAL_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define EASYLOCAL_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif
