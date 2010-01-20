## Automake file

lib_LTLIBRARIES = libEasyLocalpp.la
libEasyLocalpp_la_SOURCES = utils/Chronometer.cc utils/Random.cc \
  utils/CLParser.cc  utils/Array.cc utils/Types.cc 
if THREADING	
libEasyLocalpp_la_SOURCES += utils/Synchronize.cc
endif

AM_CPPFLAGS = -I@EASYLOCALPP_BUILD@ 
AM_LDFLAGS = -lstdc++ 

if DEBUG
AM_CXXFLAGS = -g -O0 @EOFWARNING@
else
AM_CXXFLAGS = -O3 @EOFWARNING@ #-cxxlib-gcc
endif
