## Automake file

bin_PROGRAMS = test

test_SOURCES = ../nQueens/data/ChessBoard.cc test.cc 

AM_CPPFLAGS = -I@EASYLOCALPP@/src -I@EASYLOCALPP_BUILD@ @EOFWARNING@ $(CPPUNIT_CFLAGS) 
AM_LDFLAGS = -L@EASYLOCALPP_BUILD@/src -lEasyLocalpp -lstdc++ $(CPPUNIT_LIBS)

if DEBUG
AM_CXXFLAGS = -g
else
AM_CXXFLAGS = -O3 @ICCFLAGS@
endif

check_PROGRAMS = test

TESTS = $(check_PROGRAMS)