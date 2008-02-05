# Process this file with autoconf to produce a configure

AC_PREREQ(2.59)

AC_INIT([libEasyLocalpp], [0.9], [l.digaspero@uniud.it])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR([src])
AM_CONFIG_HEADER([src/config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_TARGET
case "${target_os}" in
  *darwin* ) EOFWARNING=-Wnewline-eof
  ;;
  *) EOFWARNING=
  ;;
esac

curdir=`pwd`
curdir=`basename $curdir`
if test "$curdir" == build; then
	EASYLOCALPP=`pwd`/..
else
  EASYLOCALPP=`pwd`
fi
EASYLOCALPP_BUILD=`pwd`
AC_PREFIX_DEFAULT([`pwd`])
AM_INIT_AUTOMAKE([1.9 -Wall foreign])

CXXFLAGS="" # To avoid -g -O2 set by LIBTOOL
AC_LANG([C++])
AC_DISABLE_SHARED
AC_PROG_LIBTOOL
AC_PROG_CXX
AC_MSG_CHECKING([--enable-debug])
AC_ARG_ENABLE(debug, 
  [AS_HELP_STRING([--enable-debug], [enable debugging (default: no)])], 
  [], [])
AC_MSG_RESULT($enable_debug)
AM_CONDITIONAL(DEBUG, test "$enable_debug" = yes)
AC_MSG_CHECKING([--enable-compile-tests])
AC_ARG_ENABLE(tests, 
  [AS_HELP_STRING([--enable-compile-tests], [enable compilation of tests (default: no)])], 
  [], [])
AC_MSG_RESULT($enable_tests)
AC_MSG_CHECKING([--enable-threading])
AC_ARG_ENABLE(threading, 
  [AS_HELP_STRING([--enable-threading], [enable threading and timeout capabilities (default: yes)])], 
  [], [])
AC_MSG_RESULT($enable_threading)
AM_CONDITIONAL(THREADING, test "$enable_threading" != no)
if test "$enable_threading" != no; then
	ACX_PTHREAD
	if test "$PTHREAD_CC" != "$CC"; then
		CC="$PTHREAD_CC"
	fi
	LIBS="$PTHREAD_LIBS $LIBS"
	CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
	CXXFLAGS="$CXXFLAGS $PTHREAD_CFLAGS"
fi
AC_MSG_CHECKING([--enable-cpu-time])
AC_ARG_ENABLE(cpu_time, 
  [AS_HELP_STRING([--enable-cpu-time], [use (experimental) CPU time capabilities instead of wall-clock time (default: no)])], 
  [], [])
AC_MSG_RESULT($enable_cpu_time)
if test "$enable_cpu_time" ==  yes; then
  AC_DEFINE([CPUTIME], [], [Use CPU time capabilities instead of wall-clock time])
fi
AC_MSG_CHECKING([--enable-plot-moves])
AC_ARG_ENABLE(plot_moves, 
  [AS_HELP_STRING([--enable-plot-moves], [plot moves (default: no)])], 
  [], [])
AC_MSG_RESULT($enable_plot_moves)
if test "$enable_plot_moves" ==  yes; then
  AC_DEFINE([PLOT_MOVES], [], [Plot moves])
fi

CPPFLAGS="$CPPFLAGS -Wall"

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE

# Checks for library functions.
AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_FUNCS([memset pow sqrt strstr])
AC_CHECK_FUNCS([clock_gettime])
AC_CHECK_FUNCS([gettimeofday])
AC_CHECK_FUNCS([getrusage])
AC_CHECK_HEADERS([sys/time.h])

AC_CONFIG_FILES([Makefile src/Makefile test/Makefile test/nQueens/Makefile])
AC_SUBST(EASYLOCALPP)
AC_SUBST(EASYLOCALPP_BUILD)
if test "$enable_compile_tests" == yes; then
  COMPILESUBDIRS="src test"
else
  COMPILESUBDIRS="src"
fi
AC_SUBST(COMPILESUBDIRS)

if test "$CXX" = icc; then
   ICCFLAGS=-cxxlib-gcc
fi
AC_SUBST(ICCFLAGS)
AC_DEFUN([AC_DATAROOTDIR_CHECKED])

AC_SUBST(EOFWARNING)

AH_TOP([
#ifndef CONFIG_INCLUDED
#define CONFIG_INCLUDED
])

AH_BOTTOM([
#endif
])

AC_OUTPUT
