set(subdirs data helpers kickers)

# Generate list of all files and directories to add to executable
set(subdir_sources)
set(subdir_headers)

include_directories(${PROJECT_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

# Add files from subdirectories
FOREACH (subdir ${subdirs})

	string(SUBSTRING ${subdir} 0 1 FIRST_LETTER)
	string(TOUPPER ${FIRST_LETTER} FIRST_LETTER)
	string(REGEX REPLACE "^.(.*)" "${FIRST_LETTER}\\1" subdir_cap "${subdir}")

	file(GLOB headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${subdir}/*.hh)
	source_group("Header Files\\${subdir_cap}" FILES ${headers})
	
	file(GLOB sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${subdir}/*.cc)
	source_group("Source Files\\${subdir_cap}" FILES ${sources})
	
	set(subdir_sources ${subdir_sources} ${sources})
	set(subdir_headers ${subdir_headers} ${headers})
	
ENDFOREACH(subdir)

file(GLOB sources *.hh *.cc)
add_executable(nQueens ${sources} ${subdir_sources} ${subdir_headers})
target_link_libraries(nQueens ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} EasyLocalpp)
