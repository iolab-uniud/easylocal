## Automake file

lib_LTLIBRARIES = libEasyLocalpp.la
libEasyLocalpp_la_SOURCES = basics/EasyLocalObject.cc basics/EasyLocalSystemObjects.cc \
basics/ObjectNotFoundException.cc basics/EasyLocalException.cc \
basics/RegistrableObject.cc basics/StoppableObject.cc utils/Chronometer.cc \
utils/Random.cc utils/CLParser.cc utils/Argument.cc utils/ArgumentGroup.cc utils/matrix/IndexUtils.cc utils/Types.cc

AM_LDFLAGS = -lstdc++

if DEBUG
AM_CXXFLAGS = -g @EOFWARNING@
else
AM_CXXFLAGS = -O3 @EOFWARNING@ #-cxxlib-gcc
endif
