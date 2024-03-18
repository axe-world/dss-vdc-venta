AC_PREREQ([2.66])
AC_INIT([vdc-venta], [0.0.1], [a-x-e@gmx.net], [vdc-venta], [https://github.com/axe-world/dss-vdc-venta])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.10 -Wall no-define])

DEPSEARCH=
AC_ARG_WITH(dependency-search,
    AC_HELP_STRING([--with-dependency-search=DIR],
                   [search for dependencies in DIR, i.e. look for libraries in
                    DIR/lib and for headers in DIR/include]),
    [
        DEPSEARCH="$withval"
    ]
)
if test -n "$DEPSEARCH"; then
    CFLAGS="$CFLAGS -I$DEPSEARCH/include"
    CPPFLAGS="$CPPFLAGS -I$DEPSEARCH/include"
    LDFLAGS="$LDFLAGS -L$DEPSEARCH/lib"
    export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$DEPSEARCH/lib/pkgconfig
    export PATH=$DEPSEARCH/bin:$PATH
fi

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
ACX_PTHREAD(,AC_MSG_ERROR(POSIX threads missing))
AC_SUBST(PTHREAD_CFLAGS)
AC_SUBST(PTHREAD_LIBS)

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h stdarg.h unistd.h getopt.h syslog.h pthread.h])
AC_CHECK_HEADER([utlist.h], [],
        [AC_MSG_ERROR([required header utlist.h not found])])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset strdup getopt_long])

dnl libdsvdc
PKG_CHECK_MODULES([LIBDSVDC], [libdsvdc])
AC_SUBST(LIBDSVDC_CFLAGS)
AC_SUBST(LIBDSVDC_LIBS)

dnl libdsuid
PKG_CHECK_MODULES([LIBDSUID], [libdsuid])
AC_SUBST(LIBDSUID_CFLAGS)
AC_SUBST(LIBDSUID_LIBS)

dnl libjson
PKG_CHECK_MODULES([JSONC], [json-c], [],
    [
        PKG_CHECK_MODULES([JSONC], [json], [],
            [
                AC_MSG_ERROR([$JSON_PKG_ERRORS])
            ])
    ])
AC_SUBST(JSONC_CFLAGS)
AC_SUBST(JSONC_LIBS)

dnl libconfig
PKG_CHECK_MODULES([LIBCONFIG], [libconfig])
AC_SUBST(LIBCONFIG_CFLAGS)
AC_SUBST(LIBCONFIG_LIBS)

dnl curl
PKG_CHECK_MODULES([CURL], [libcurl])
AC_SUBST(CURL_CFLAGS)
AC_SUBST(CURL_LIBS)

AC_CONFIG_LINKS([
  venta/venta-humifier-16.png:venta/venta-humifier-16.png
  venta/venta-humifier-48.png:venta/venta-humifier-48.png
])

AC_CONFIG_FILES([
  Makefile
  venta/Makefile
])
AC_OUTPUT

