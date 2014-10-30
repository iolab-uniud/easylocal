find_package(Threads REQUIRED)
find_package(Boost 1.45.0 COMPONENTS program_options REQUIRED)

include_directories(${Boost_INCLUDE_DIR} ${PROJECT_INCLUDE_DIR})

set(subdirs helpers observers runners solvers testers utils modeling)

# Generate list of all files and directories to be added to the library
set(subdir_sources)
set(subdir_headers)

# Add files from subdirectories
FOREACH (subdir ${subdirs})

	string(SUBSTRING ${subdir} 0 1 FIRST_LETTER)
	string(TOUPPER ${FIRST_LETTER} FIRST_LETTER)
	string(REGEX REPLACE "^.(.*)" "${FIRST_LETTER}\\1" subdir_cap "${subdir}")

	file(GLOB headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_INCLUDE_DIR}/easylocal/${subdir}/*.hh)
	source_group("Header Files\\${subdir_cap}" FILES ${headers})
	
	file(GLOB sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/${subdir}/*.cc)
	source_group("Source Files\\${subdir_cap}" FILES ${sources})
		
	set(subdir_sources ${subdir_sources} ${sources})
	set(subdir_headers ${subdir_headers} ${headers})
	
ENDFOREACH(subdir)

file(GLOB headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_INCLUDE_DIR}/easylocal/*.hh)
source_group("Header Files" FILES ${headers})

add_library(${PROJECT_NAME} ${headers} ${subdir_sources} ${subdir_headers})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

install(
  DIRECTORY ${PROJECT_INCLUDE_DIR}/easylocal
  DESTINATION ${CMAKE_INSTALL_PREFIX}/include 
  PATTERN * 
  PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ 
)

install(
  TARGETS ${PROJECT_NAME} 
  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib 
  PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ 
)
