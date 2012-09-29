## Automake file

lib_LTLIBRARIES = libnQueens.la

bin_PROGRAMS = queens

if CPPUNIT
bin_PROGRAMS += QueensTest
endif

libnQueens_la_SOURCES = data/ChessBoard.cc data/Swap.cc \
  helpers/SwapNeighborhoodExplorer.cc helpers/PrimaryDiagonalDeltaCostComponent.cc \
  helpers/QueensOutputManager.cc helpers/SecondaryDiagonalCostComponent.cc \
  helpers/SecondaryDiagonalDeltaCostComponent.cc \
  helpers/QueensStateManager.cc helpers/PrimaryDiagonalCostComponent.cc

queens_SOURCES = main.cc

AM_CPPFLAGS = -I@EASYLOCALPP@/src -I@EASYLOCALPP_BUILD@ @EOFWARNING@ 
AM_LDFLAGS = -L@EASYLOCALPP_BUILD@/src -L. -lEasyLocalpp -lnQueens -lstdc++ 

if BOOST
AM_CPPFLAGS += @BOOST_CPPFLAGS@
AM_LDFLAGS += @BOOST_LDFLAGS@ -lboost_program_options
endif

if DEBUG
AM_CXXFLAGS = -g
else
AM_CXXFLAGS = -O3 @ICCFLAGS@
endif

if CPPUNIT
check_PROGRAMS = QueensTest

TESTS = $(check_PROGRAMS)

QueensTest_SOURCES = unit/QueensStateManagerTest.cc unit/QueensOutputManagerTest.cc unit/QueensCostComponentsTest.cc \
		unit/SwapNeighborhoodExplorerTest.cc unit/QueensDeltaCostComponentsTest.cc \
		@EASYLOCALPP@/src/unit/UnitTestingMain.cc
QueensTest_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS)
QueensTest_LDFLAGS = $(AM_LDFLAGS) -L. $(CPPUNIT_LIBS)
endif
