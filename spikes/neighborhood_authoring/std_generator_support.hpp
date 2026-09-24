#pragma once

#if defined(__has_include)
#  if __has_include(<generator>)
#    include <generator>
#    if defined(__cpp_lib_generator) && __cpp_lib_generator >= 202207L
#      define EASYLOCAL_SPIKE_HAS_STD_GENERATOR 1
#    endif
#  endif
#endif

#ifndef EASYLOCAL_SPIKE_HAS_STD_GENERATOR
#  define EASYLOCAL_SPIKE_HAS_STD_GENERATOR 0
#endif

namespace easylocal::spike::neighborhood_authoring
{

inline constexpr bool has_std_generator =
    EASYLOCAL_SPIKE_HAS_STD_GENERATOR != 0;

} // namespace easylocal::spike::neighborhood_authoring
