
set(HOMEBREW_PREFIX "/opt/homebrew"
    CACHE PATH "Path to Homebrew installation")

# First try with llvm
file(GLOB LLVM_CELLAR_DIRS "${HOMEBREW_PREFIX}/Cellar/llvm/*")
foreach(LLVM_CELLAR_DIR ${LLVM_CELLAR_DIRS})
    if(IS_DIRECTORY ${LLVM_CELLAR_DIR})
        get_filename_component(LLVM_VERSION ${LLVM_CELLAR_DIR} NAME)
        message(STATUS "Found llvm version: ${LLVM_VERSION}")
        break()
    endif()
endforeach()

if (LLVM_VERSION)
    set(CMAKE_C_COMPILER "${HOMEBREW_PREFIX}/Cellar/llvm/${LLVM_VERSION}/bin/clang")
    set(CMAKE_CXX_COMPILER "${HOMEBREW_PREFIX}/Cellar/llvm/${LLVM_VERSION}/bin/clang++")
    set(CMAKE_PREFIX_PATH
    "${HOMEBREW_PREFIX}/Cellar/llvm/${LLVM_VERSION}")    
else()
    # alternatively search for gcc
    file(GLOB GCC_CELLAR_DIRS "${HOMEBREW_PREFIX}/Cellar/gcc/*")
    foreach(GCC_CELLAR_DIR ${GCC_CELLAR_DIRS})
        if(IS_DIRECTORY ${GCC_CELLAR_DIR})
            get_filename_component(GCC_VERSION ${GCC_CELLAR_DIR} NAME)
            message(STATUS "Found gcc version: ${GCC_VERSION}")
            break()
        endif()
    endforeach()
    if (!GCC_VERSION)
        message(FATAL_ERROR "Neither a suitable llvm nor gcc version found in the system. Install them using homebrew: homebrew install llvm")
    endif()
    set(CMAKE_C_COMPILER "${HOMEBREW_PREFIX}/Cellar/gcc/${GCC_VERSION}/bin/gcc")
    set(CMAKE_CXX_COMPILER "${HOMEBREW_PREFIX}/Cellar/gcc/${GCC_VERSION}/bin/g++")
    set(CMAKE_PREFIX_PATH
    "${HOMEBREW_PREFIX}/Cellar/gcc/${GCC_VERSION}")
endif()

list(TRANSFORM CMAKE_PREFIX_PATH APPEND "/include"
     OUTPUT_VARIABLE CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES)
set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES "${CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES}")

set(CMAKE_FIND_FRAMEWORK NEVER)
set(CMAKE_FIND_APPBUNDLE NEVER)

set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)
set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)

# boost needs some hints