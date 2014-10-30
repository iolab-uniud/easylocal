# Try to find the EasyLocal libraries.
#
# Once done this will define
#
# EASYLOCAL_FOUND - System has EasyLocal
# EASYLOCAL_INCLUDE_DIRS - The EasyLocal include directories
# EASYLOCAL_LIBRARIES - The libraries needed to use EasyLocal

message(STATUS "Searching for EasyLocal")
# First search for a local subproject version of EasyLocal
find_file(EASYLOCAL_PROJECT_DIR
	NAMES easylocal/CMakeLists.txt easylocal-3/CMakeLists.txt EasyLocal/CMakeLists.txt EasyLocalpp/CMakeLists.txt
	HINTS ${PROJECT_SOURCE_DIR}
)
if (EASYLOCAL_PROJECT_DIR)
	get_filename_component(tmp ${EASYLOCAL_PROJECT_DIR} DIRECTORY)
	set(EASYLOCAL_INCLUDE_DIR ${tmp}/include)
	set(EASYLOCAL_LIBRARY EasyLocal)
	add_subdirectory(${tmp})
endif ()
# If a local version of EasyLocal is not available, search for a system-wide installed
if (NOT EASYLOCAL_INCLUDE_DIR)
	find_path(EASYLOCAL_INCLUDE_DIR
		NAMES easylocal/core.hh
		HINTS /usr/local/include /opt/local/include
	)
	find_library(EASYLOCAL_LIBRARY
		NAMES EasyLocal
		HINTS /usr/local/lib /opt/local/lib
	)
endif()

find_package(Threads REQUIRED)
list(APPEND EASYLOCAL_INCLUDE_DIRS ${EASYLOCAL_INCLUDE_DIR})
list(APPEND EASYLOCAL_LIBRARIES ${EASYLOCAL_LIBRARY})
find_package(Boost 1.45.0 COMPONENTS program_options REQUIRED)
if (Boost_FOUND)
	list(APPEND EASYLOCAL_INCLUDE_DIRS ${Boost_INCLUDE_DIR})
	list(APPEND EASYLOCAL_LIBRARIES ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif ()	
find_package(TBB)
if (TBB_FOUND)
	list(APPEND EASYLOCAL_INCLUDE_DIRS ${TBB_INCLUDE_DIRS})
	list(APPEND EASYLOCAL_LIBRARIES ${TBB_LIBRARIES})	
	add_definitions(-DTBB_AVAILABLE)
endif ()

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set EASYLOCAL_FOUND to TRUE
# if all listed variables are TRUE.
find_package_handle_standard_args(EasyLocal DEFAULT_MSG EASYLOCAL_INCLUDE_DIRS EASYLOCAL_LIBRARIES)

#mark_as_advanced(${GECODE_LIBRARY_VARS})