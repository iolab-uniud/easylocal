add_executable(nQueens main.cc data/ChessBoard.cc data/Swap.cc 
  helpers/SwapNeighborhoodExplorer.cc helpers/PrimaryDiagonalDeltaCostComponent.cc 
  helpers/QueensOutputManager.cc helpers/SecondaryDiagonalCostComponent.cc 
  helpers/SecondaryDiagonalDeltaCostComponent.cc 
  helpers/QueensStateManager.cc helpers/PrimaryDiagonalCostComponent.cc)

include_directories(${PROJECT_DIR}/src)
target_link_libraries(nQueens ${Boost_LIBRARIES} EasyLocalpp)

find_package(CPPUNIT)
if(CPPUNIT_FOUND)
#  add_subdirectory(unit)
endif(CPPUNIT_FOUND)

