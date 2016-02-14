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

AC_ARG_ENABLE(local-tdap,
  AS_HELP_STRING([--enable-local-tdap], [Use local TDAP headers]),
  [
  	tdapDirectory="${enableval}"
    if test "x${tdapDirectory}" = "x" 
    then
    	TDAP_DIR=""
    elif test -d "${tdapDirectory}"
    then
    	TDAP_DIR="${tdapDirectory}"
    else
	    AC_MSG_WARN(Value for environment variable SPEAKERMAN_ENABLE_LOCAL_TDAP must be an existing directory;  ${tdapDirectory})
    	TDAP_DIR=""
    fi
  ],[
  	tdapDirectory="${SPEAKERMAN_ENABLE_LOCAL_TDAP}"
    if test "x${tdapDirectory}" = "x" 
    then
    	TDAP_DIR=""
    elif test -d "${tdapDirectory}"
    then
    	TDAP_DIR="${tdapDirectory}"
    else
	    AC_MSG_WARN(Value for environment variable SPEAKERMAN_ENABLE_LOCAL_TDAP must be an existing directory;  ${tdapDirectory})
    	TDAP_DIR=""
    fi
  ]
)

if test "x$TDAP_DIR" = "x" 
then
	PKG_CHECK_MODULES(TDAP, tdap,,)
	AC_MSG_NOTICE([Using TDAP package])
else
	TDAP_CFLAGS="-I ${TDAP_DIR}"
	TDAP_LIBS=""
	AC_MSG_NOTICE([Using local TDAP headers in ${TDAP_DIR}])
fi
AC_SUBST(TDAP_CFLAGS) 
AC_SUBST(TDAP_LIBS)


AC_CONFIG_FILES(Makefile src/Makefile)
AC_OUTPUT

