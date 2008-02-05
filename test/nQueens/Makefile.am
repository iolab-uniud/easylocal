## Automake file

bin_PROGRAMS = nQueens

nQueens_SOURCES = data/ChessBoard.cc data/Swap.cc \
  helpers/QueensNeighborhoodExplorer.cc helpers/PrimaryDiagonalDeltaCostComponent.cc \
  helpers/QueensOutputManager.cc helpers/SecondaryDiagonalCostComponent.cc \
  helpers/SecondaryDiagonalDeltaCostComponent.cc \
  helpers/QueensStateManager.cc helpers/PrimaryDiagonalCostComponent.cc \
  main.cc

AM_CPPFLAGS = -I@EASYLOCALPP@/src -I@EASYLOCALPP_BUILD@ @EOFWARNING@
AM_LDFLAGS = -L@EASYLOCALPP_BUILD@/lib -lEasyLocalpp -lstdc++

if DEBUG
AM_CXXFLAGS = -g
else
AM_CXXFLAGS = -O3 @ICCFLAGS@
endif

