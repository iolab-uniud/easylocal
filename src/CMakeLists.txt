find_package(Threads REQUIRED)
find_package(Boost 1.54.0 COMPONENTS program_options REQUIRED)

include_directories(${Boost_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../include)

set(subdirs helpers observers runners solvers testers utils modeling)

# Generate list of all files and directories to be added to the library
set(subdir_sources)
set(subdir_headers)

# Add files from subdirectories
FOREACH (subdir ${subdirs})

	string(SUBSTRING ${subdir} 0 1 FIRST_LETTER)
	string(TOUPPER ${FIRST_LETTER} FIRST_LETTER)
	string(REGEX REPLACE "^.(.*)" "${FIRST_LETTER}\\1" subdir_cap "${subdir}")

  file(GLOB headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ../include/easylocal/${subdir}/*.hh)
	source_group("Header Files\\${subdir_cap}" FILES ${headers})

  file(GLOB sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${subdir}/*.cc)
	source_group("Source Files\\${subdir_cap}" FILES ${sources})

	set(subdir_sources ${subdir_sources} ${sources})
	set(subdir_headers ${subdir_headers} ${headers})

ENDFOREACH(subdir)

file(GLOB headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ../include/easylocal/*.hh)
source_group("Header Files" FILES ${headers})


if (MSVC)
  add_definitions(-DMSVC)
  add_library(EasyLocal STATIC ${headers} ${subdir_sources} ${subdir_headers})
else (MSVC)
  add_library(EasyLocal SHARED ${headers} ${subdir_sources} ${subdir_headers})
endif (MSVC)

set_target_properties(EasyLocal PROPERTIES POSITION_INDEPENDENT_CODE 1)
if (APPLE)
	set_target_properties(EasyLocal PROPERTIES MACOSX_RPATH ".")
endif (APPLE)
target_link_libraries(EasyLocal ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(EasyLocal PROPERTIES LINKER_LANGUAGE CXX)

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/easylocal
  DESTINATION ${CMAKE_INSTALL_PREFIX}/include
  PATTERN *
  PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
)

if (CYGWIN)
install(
  TARGETS EasyLocal
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
  PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
)
else (CYGWIN)
install(
  TARGETS EasyLocal
  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
  PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
)
endif (CYGWIN)
