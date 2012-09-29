## Automake file

lib_LTLIBRARIES = libEasyLocalpp.la
libEasyLocalpp_la_SOURCES = utils/Random.cc utils/Types.cc utils/Parameter.cc

AM_CPPFLAGS = -I@EASYLOCALPP_BUILD@ @BOOST_CPPFLAGS@

if DEBUG
AM_CXXFLAGS = -g -O0 @EOFWARNING@
else
AM_CXXFLAGS = -O3 @EOFWARNING@ #-cxxlib-gcc
endif
