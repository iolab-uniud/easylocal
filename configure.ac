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

CXXFLAGS="" # To avoid -g -O2 set by LIBTOOL
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

# Checking for C++11 compliance
AC_MSG_CHECKING([for C++11 compiler compliance]) 
AX_CHECK_COMPILE_FLAG([-std=c++0x], [AX_APPEND_FLAG(CXXFLAGS, [-std=c++0x])], [AC_ERROR([This version of EasyLocal++ works only with C++11 compliant compilers])])
CXXFLAGS="$CXXFLAGS -std=c++0x"
# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE

# Checks for library functions.
AC_HEADER_STDC
#AC_HEADER_TIME
AC_CHECK_FUNCS([memset pow sqrt strstr floor])
#AC_CHECK_FUNCS([getrusage])
#AC_CHECK_FUNCS([clock_gettime])
#AC_CHECK_FUNCS([gettimeofday])
#AC_CHECK_HEADERS([sys/time.h])
#AC_CHECK_HEADERS([sys/resource.h])

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
