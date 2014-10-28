AC_INIT(modbusd, 0.1)

AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.11])

AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_CC_C_O
PKG_CHECK_MODULES(LIBCZMQ,libczmq)

AC_OUTPUT([
Makefile
src/Makefile
test/Makefile
modules/Makefile
modules/fastcgi/Makefile
])
