add_library(nq data/ChessBoard.cc data/Swap.cc 
  helpers/SwapNeighborhoodExplorer.cc helpers/PrimaryDiagonalDeltaCostComponent.cc 
  helpers/QueensOutputManager.cc helpers/SecondaryDiagonalCostComponent.cc 
  helpers/SecondaryDiagonalDeltaCostComponent.cc 
  helpers/QueensStateManager.cc helpers/PrimaryDiagonalCostComponent.cc)
  
target_link_libraries(nq ${Boost_LIBRARIES} EasyLocalpp)

add_executable(nQueens main.cc)
target_link_libraries(nQueens ${Boost_LIBRARIES} EasyLocalpp nq)

find_package(CPPUNIT)
if(CPPUNIT_FOUND)
#  add_subdirectory(unit)
endif(CPPUNIT_FOUND)

