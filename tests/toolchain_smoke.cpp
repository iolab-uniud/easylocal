// Infrastructure only. This is not an EasyLocal API or a numerical-policy test.
#include <algorithm>
#include <array>
#include <concepts>
#include <cstdlib>
#include <iostream>
#include <ranges>
#include <span>
#include <string_view>

static_assert(__cplusplus >= 202002L, "C++20 or later is required.");
static_assert(std::ranges::contiguous_range<std::span<const int>>);
static_assert(std::same_as<std::ranges::range_reference_t<std::span<const int>>,
                           const int&>);

#if defined(EL_EXPECT_LIBCXX) && !defined(_LIBCPP_VERSION)
#error "CI requested libc++, but different standard-library headers were selected."
#endif
#if defined(EL_EXPECT_LIBSTDCXX) && !defined(__GLIBCXX__)
#error "CI requested libstdc++, but different standard-library headers were selected."
#endif

namespace {
void print_toolchain()
{
#if defined(__apple_build_version__)
    std::cout << "compiler=AppleClang " << __clang_version__ << '\n';
    std::cout << "apple_build=" << __apple_build_version__ << '\n';
#elif defined(__clang__)
    std::cout << "compiler=Clang " << __clang_version__ << '\n';
#elif defined(__GNUC__)
    std::cout << "compiler=GCC " << __VERSION__ << '\n';
#endif
    std::cout << "__cplusplus=" << __cplusplus << '\n';
#if defined(_LIBCPP_VERSION)
    std::cout << "stdlib=libc++ header_version=" << _LIBCPP_VERSION << '\n';
#elif defined(__GLIBCXX__)
    std::cout << "stdlib=libstdc++ header_date=" << __GLIBCXX__ << '\n';
#if defined(_GLIBCXX_RELEASE)
    std::cout << "libstdc++_release=" << _GLIBCXX_RELEASE << '\n';
#endif
#else
    std::cout << "stdlib=unidentified\n";
#endif
}
} // namespace

int main(int argc, char* argv[])
{
    print_toolchain();
    if (argc == 2 && std::string_view{argv[1]} == "--fail") {
        std::cerr << "Intentional nonzero exit for the CTest WILL_FAIL check.\n";
        return EXIT_FAILURE;
    }
    if (argc != 1) {
        std::cerr << "Usage: el_toolchain_smoke [--fail]\n";
        return EXIT_FAILURE;
    }

    constexpr std::array values{1, 2, 2, 4};
    const std::span<const int> view{values};
    if (std::ranges::count(view, 2) != 2) {
        std::cerr << "C++20 ranges smoke check failed.\n";
        return EXIT_FAILURE;
    }
    if (std::ranges::find(view, 9) != view.end()) {
        std::cerr << "C++20 ranges end check failed.\n";
        return EXIT_FAILURE;
    }
    // Explicit checks remain active when NDEBUG is defined.
    std::cout << "Infrastructure smoke checks passed.\n";
    return EXIT_SUCCESS;
}
