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

AC_ARG_WITH(
	[simpledsp-local],
	[AS_HELP_STRING([--with-simpledsp-local],[The RELATIVE location of the lib simpledsp source files. 
	Use this only if you are currently developing libsimpledsp and it resides in that location. In addition,
	it wise not to have libsimpledsp-dev installed in that case...])],
	[
		AC_MSG_NOTICE(["Local simpledsp project is here: $with_simpledsp_local"])
		AC_SUBST(SIMPLEDSP_CFLAGS, "${CXXFLAGS} -I../$with_simpledsp_local/include/")
		AC_SUBST(SIMPLEDSP_LIBS, "../$with_simpledsp_local/src/.libs/libsimpledsp.so")
	],
	[
		PKG_CHECK_MODULES(SIMPLEDSP, libsimpledsp,,)
		AC_SUBST(SIMPLEDSP_CFLAGS)
		AC_SUBST(SIMPLEDSP_LIBS)
	]
)

AC_CONFIG_FILES(Makefile src/Makefile)
AC_OUTPUT

