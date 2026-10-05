cmake_minimum_required(VERSION 3.25)

# Writes OUTPUT, a translation unit that includes the standard headers that
# the HEADERS (paths relative to INCLUDE_DIR) include, then defines the
# macros min(a, b) and max(a, b) of windows.h, then includes the HEADERS: it
# compiles only if no header calls min or max in a way the macros expand,
# such as std::max(a, b) or std::numeric_limits<T>::max().

foreach(_required IN ITEMS HEADERS INCLUDE_DIR OUTPUT)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} must be provided")
    endif()
endforeach()

set(_standard_headers)
foreach(_header IN LISTS HEADERS)
    file(STRINGS "${INCLUDE_DIR}/${_header}" _lines REGEX "^#include <[a-z_0-9]+>")
    list(APPEND _standard_headers ${_lines})
endforeach()
list(REMOVE_DUPLICATES _standard_headers)
list(SORT _standard_headers)
# <generator> is included only where the library has it.
list(REMOVE_ITEM _standard_headers "#include <generator>")

set(_content "")
foreach(_line IN LISTS _standard_headers)
    string(APPEND _content "${_line}\n")
endforeach()
string(APPEND _content
    "#if defined(__cpp_lib_generator)\n#include <generator>\n#endif\n\n"
    "#define min(a, b) (((a) < (b)) ? (a) : (b))\n"
    "#define max(a, b) (((a) > (b)) ? (a) : (b))\n\n")
foreach(_header IN LISTS HEADERS)
    string(APPEND _content "#include <${_header}>\n")
endforeach()
string(APPEND _content "\nint main() { return 0; }\n")

file(CONFIGURE OUTPUT "${OUTPUT}" CONTENT "${_content}" @ONLY)
