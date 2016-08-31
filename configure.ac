dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(speakerman, 1.0)

AC_PROG_LIBTOOL
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE()

AC_PROG_CXX
LT_INIT

dnl This library uses C++11 features
AX_CXX_COMPILE_STDCXX_11(noext,mandatory)

PKG_CHECK_MODULES(JACK, [jack >= 1.9.8],)
AC_SUBST(JACK_CFLAGS)
AC_SUBST(JACK_LIBS)

AC_CONFIG_FILES(Makefile src/Makefile scripts/Makefile web/Makefile examples/Makefile configfile/Makefile init/Makefile)
AC_OUTPUT
