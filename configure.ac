# Process this file with autoconf to produce a configure

AC_PREREQ(2.59)

AC_INIT([libEasyLocal++], [3.0 francis], [luca.digaspero@uniud.it])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADER([src/config.hh])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_TARGET
case "${target_os}" in
  *darwin* ) EOFWARNING=
  ;;
  *) EOFWARNING=
  ;;
esac

# Search for the EasyLocal source dir (when the build happens in a separate
# directory).
curdir=`pwd`
while test "$curdir" != "/"; do
  if test -e "$curdir/src/EasyLocal.hh"; then
    break
  fi
  curdir=`dirname $curdir`
done
AC_MSG_RESULT([Found EasyLocal++ source directory at $curdir])
EASYLOCALPP=$curdir
EASYLOCALPP_BUILD=`pwd`
AC_PREFIX_DEFAULT([`pwd`])

AM_INIT_AUTOMAKE([1.9 -Wall foreign])
# only to prevent the automatic addition of -g -O2 by AC_PROG_CXX
CFLAGS="$CFLAGS" 
CXXFLAGS="$CXXFLAGS"
AM_PROG_AR
AC_LANG([C++])
AC_DISABLE_SHARED
AC_PROG_LIBTOOL
AC_PROG_CXX

# Checking for debug option
AC_MSG_CHECKING([--enable-debug])
AC_ARG_ENABLE(debug, 
  [AS_HELP_STRING([--enable-debug], [enable debugging (default: no)])], 
  [], [])
AC_MSG_RESULT($enable_debug)
AM_CONDITIONAL(DEBUG, test "$enable_debug" = yes)
if test "$enable_debug" == "yes"; then
  AC_DEFINE([DEBUG], 1, [Compile further debugging controls.])
fi

# Checking for C++11 compliance
AC_MSG_CHECKING([for C++11 compiler compliance]) 
AC_MSG_RESULT() # just for the newline
AX_CHECK_COMPILE_FLAG([-std=c++11], [CXX11PPFLAG=-std=c++11])
if test "$CXX11PPFLAG" == ""; then
  AX_CHECK_COMPILE_FLAG([-std=c++0x], [CXX11PPFLAG=-std=c++0x])
fi
if test "$CXX11PPFLAG" != ""; then
  CXXFLAGS="$CXXFLAGS $CXX11PPFLAG"
	is_cxx11=yes
else
  AC_MSG_CHECKING([whether C++ compiler defines a __cplusplus macro more recent than 199711L])
	AC_EGREP_CPP(yes,
	[	
#if defined(__cplusplus) && __cplusplus > 199711L
  yes
#endif
	], is_cxx11=yes, is_cxx11=no)
fi
if test "$is_cxx11" != "yes"; then
  AC_MSG_ERROR([A C++11 compliant compiler should be used in order to compile this version of EasyLocal++])
fi
# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE

# Checks for library functions.
AC_HEADER_STDC
AC_CHECK_FUNCS([memset pow sqrt strstr floor])

# Checks for std::this_thread::sleep_for availability (in g++ <= 4.8 is not ensured)
AC_MSG_CHECKING([for std::this_thread::sleep_for])
AC_COMPILE_IFELSE(  [AC_LANG_PROGRAM([[#include <thread>]],
                                     [[std::this_thread::sleep_for(std::chrono::seconds(1));]])],
                    [
                      AC_MSG_RESULT([yes])
                      AC_DEFINE_UNQUOTED([HAVE_THIS_THREAD_SLEEP_FOR], 1, [Define to 1 if you have the `sleep_for' function.])
                    ],
                    [
                      AC_MSG_RESULT([no])                      
                    ]
                 )


# For Boost library.
AX_BOOST_BASE([1.40], [AM_CONDITIONAL(BOOST, true)], [AM_CONDITIONAL(BOOST, false)])
# Checks for boost::program_options (macports boost could be compiled with an incompatible c++11 compiler)
AC_MSG_CHECKING([whether the boost_program_options library can be linked (single-threaded version)])
LDFLAGS_SAVED="$LDFLAGS"
LDFLAGS="$LDFLAGS $BOOST_LDFLAGS"
LIBS_SAVED="$LIBS"
LIBS="-lboost_program_options $LIBS_SAVED"
AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([[#include <boost/program_options/options_description.hpp>]],
        [[boost::program_options::options_description tmp("Test program options");]])],
    [
      AC_MSG_RESULT([yes])
      AC_DEFINE_UNQUOTED([HAVE_LINKABLE_BOOST], 1, [Define to 1 if you have a linkable boost.])
      BOOST_PROGRAM_OPTIONS_LIB=-lboost_program_options
    ],
    [AC_MSG_RESULT([no])
    AM_CONDITIONAL(BOOST, false)])
if test "$BOOST_PROGRAM_OPTIONS_LIB" = ""; then
  AC_MSG_CHECKING([whether the boost_program_options library can be linked (multi-threaded version)])
  LIBS="-lboost_program_options-mt $LIBS_SAVED"
  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([[#include <boost/program_options/options_description.hpp>]],
        [[boost::program_options::options_description tmp("Test program options");]])],
    [
      AC_MSG_RESULT([yes])
      AC_DEFINE_UNQUOTED([HAVE_LINKABLE_BOOST], 1, [Define to 1 if you have a linkable boost.])
      BOOST_PROGRAM_OPTIONS_LIB=-lboost_program_options-mt
    ],
    [AC_MSG_RESULT([no])
    AM_CONDITIONAL(BOOST, false)])
fi
LDFLAGS=$LDFLAGS_SAVED
LIBS=$LIBS_SAVED

# For CppUnit testing
AC_ARG_ENABLE(cppunit, 
  [AS_HELP_STRING([--enable-cppunit], [enable unit testing with cppunit (default: no)])], 
  [], [])
AC_MSG_RESULT($enable_cppunit)
AM_CONDITIONAL(CPPUNIT, test "$enable_cppunit" = yes)
if test "$enable_cppunit" = yes; then
  AM_PATH_CPPUNIT(1.9.0, 
	  [AM_CONDITIONAL(CPPUNIT, true)], 
	  [AM_CONDITIONAL(CPPUNIT, false)])
fi


AC_CONFIG_FILES([Makefile src/Makefile test/Makefile test/nQueens/Makefile])
AC_SUBST(EASYLOCALPP)
AC_SUBST(EASYLOCALPP_BUILD)
AC_SUBST(BOOST_PROGRAM_OPTIONS_LIB)
COMPILESUBDIRS="src test"
AC_SUBST(COMPILESUBDIRS)

if test "$CXX" = icc; then
   ICCFLAGS=-cxxlib-gcc
fi
AC_SUBST(ICCFLAGS)
AC_DEFUN([AC_DATAROOTDIR_CHECKED])

AC_SUBST(EOFWARNING)

# For doxygen documentation

DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN([EasyLocal++], [doxygen.cfg])

AH_TOP([
#if !defined(CONFIG_INCLUDED)
#define CONFIG_INCLUDED
])

AH_BOTTOM([
#endif // #define CONFIG_INCLUDED
])

CPPFLAGS="$CPPFLAGS -Wall"

AC_OUTPUT
