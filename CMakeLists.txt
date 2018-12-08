find_package(Threads REQUIRED)
find_package(Boost 1.58.0 COMPONENTS program_options REQUIRED)

include_directories(${Boost_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../include)

set(subdirs helpers observers runners solvers testers utils modeling)

# Generate list of all files and directories to be added to the library
set(subdir_sources)

# Add files from subdirectories
FOREACH (subdir ${subdirs})

	string(SUBSTRING ${subdir} 0 1 FIRST_LETTER)
	string(TOUPPER ${FIRST_LETTER} FIRST_LETTER)
	string(REGEX REPLACE "^.(.*)" "${FIRST_LETTER}\\1" subdir_cap "${subdir}")

  file(GLOB headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} include/${subdir}/*.hh)
	source_group("Header Files\\${subdir_cap}" FILES ${headers})

	set(subdir_sources ${subdir_sources} ${sources})

ENDFOREACH(subdir)

file(GLOB headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} include/*.hh)
source_group("Header Files" FILES ${headers})

add_library(EasyLocal INTERFACE)
target_include_directories(EasyLocal INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(EasyLocal INTERFACE ${Boost_LIBRARIES})
target_sources(EasyLocal INTERFACE ${headers})
#set_property(TARGET EasyLocal PROPERTY CXX_STANDARD 14) 
#set_property(TARGET EasyLocal PROPERTY CMAKE_CXX_FLAGS "-Wall -Wfatal-errors")
#set_target_properties(EasyLocal PROPERTIES LINKER_LANGUAGE CXX)

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include
  DESTINATION ${CMAKE_INSTALL_PREFIX}/include/easylocal
  PATTERN *
  PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
)

