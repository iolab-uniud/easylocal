## Automake file

lib_LTLIBRARIES = libEasyLocalpp.la
libEasyLocalpp_la_SOURCES = utils/Random.cc \
  utils/CLParser.cc  utils/Array.cc utils/Types.cc

AM_CPPFLAGS = -I@EASYLOCALPP_BUILD@ 
AM_LDFLAGS = -lstdc++ 

if DEBUG
AM_CXXFLAGS = -g -O0 @EOFWARNING@
else
AM_CXXFLAGS = -O3 @EOFWARNING@ #-cxxlib-gcc
endif
