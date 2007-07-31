## Automake file

lib_LTLIBRARIES = libnQueens.la
libnQueens_la_SOURCES = data/ChessBoard.cc data/Swap.cc \
helpers/QueensNeighborhoodExplorer.cc helpers/PrimaryDiagonalDeltaCostComponent.cc \
helpers/QueensOutputManager.cc helpers/SecondaryDiagonalCostComponent.cc helpers/SecondaryDiagonalDeltaCostComponent.cc \
helpers/QueensStateManager.cc helpers/PrimaryDiagonalCostComponent.cc


bin_PROGRAMS = nQueens

nQueens_SOURCES = main.cc
nQueens_LDADD   = libnQueens.la

AM_CPPFLAGS = -I../../src @EOFWARNING@ -DVERBOSE=4
AM_LDFLAGS = -L../../lib -lEasyLocalpp -lstdc++

if DEBUG
AM_CXXFLAGS = -g
else
AM_CXXFLAGS = -O3 @ICCFLAGS@
endif

