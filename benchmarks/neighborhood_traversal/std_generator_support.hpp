#pragma once

#if defined(__has_include)
#  if __has_include(<generator>)
#    include <generator>
#    if defined(__cpp_lib_generator) && __cpp_lib_generator >= 202207L
#      define EASYLOCAL_BENCHMARK_HAS_STD_GENERATOR 1
#    endif
#  endif
#endif

#ifndef EASYLOCAL_BENCHMARK_HAS_STD_GENERATOR
#  define EASYLOCAL_BENCHMARK_HAS_STD_GENERATOR 0
#endif

namespace easylocal::benchmark::neighborhood_traversal
{

inline constexpr bool has_std_generator =
    EASYLOCAL_BENCHMARK_HAS_STD_GENERATOR != 0;

} // namespace easylocal::benchmark::neighborhood_traversal
