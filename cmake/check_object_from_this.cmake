include(CheckCXXSourceCompiles)

set(OBJECT_FROM_THIS_SNIPPET "struct A {}; 
struct ObjectFromThisTest { 
    void f(this A&& self, int) {}
};

int main() { return 0; }")
check_cxx_source_compiles("${OBJECT_FROM_THIS_SNIPPET}" OBJECT_FROM_THIS_SUPPORTED)

if (OBJECT_FROM_THIS_SUPPORTED)
    message(STATUS "The compiler supports object from this.")
else()
    message(FATAL_ERROR "The compiler does not support object from this. You should upgrade your compiler to a c++23 compliant one (llvm >= 18, gcc >= 14).")
endif()