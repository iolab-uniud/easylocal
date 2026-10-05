# Checks that the C++ toolchain can compile EasyLocal, with a clear message
# when it cannot: included by the project and by the installed package.

include_guard(GLOBAL)

# AppleClang's libc++ has the floating-point std::from_chars, which EasyLocal
# uses to read numbers, only for a deployment target of macOS 26 or later:
# below it, or with an SDK older than Xcode 26's, every parse of a number fails
# to compile. The check compiles one such parse with the current settings.
function(easylocal_check_toolchain)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        return()
    endif()
    include(CheckCXXSourceCompiles)
    # Checked again for another deployment target or SDK.
    string(MAKE_C_IDENTIFIER
        "EASYLOCAL_FROM_CHARS_${CMAKE_OSX_DEPLOYMENT_TARGET}_${CMAKE_OSX_SYSROOT}"
        result)
    set(CMAKE_REQUIRED_QUIET ON)
    set(CMAKE_REQUIRED_FLAGS "-std=c++17")
    check_cxx_source_compiles(
        [[
#include <charconv>
int main()
{
    double value = 0;
    const char text[] = "1.5";
    return std::from_chars(text, text + 3, value).ec == std::errc{} ? 0 : 1;
}
]]
        ${result})
    if(NOT ${result})
        message(FATAL_ERROR
            "EasyLocal needs std::from_chars for floating-point numbers, which "
            "AppleClang's libc++ provides from Xcode 26 and for a macOS "
            "deployment target of 26.0 or later (CMAKE_OSX_DEPLOYMENT_TARGET is "
            "'${CMAKE_OSX_DEPLOYMENT_TARGET}'). Use Xcode 26 or later and set "
            "CMAKE_OSX_DEPLOYMENT_TARGET=26.0 or later, or build with GCC.")
    endif()
endfunction()
