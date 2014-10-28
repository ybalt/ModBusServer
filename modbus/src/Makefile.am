AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\" \
	$(LIBCZMQ_CFLAGS) 

AM_CFLAGS =\
	 -Wall\
	-O0

bin_PROGRAMS = \
	modbusd

modbusd_SOURCES =  \
	modbusd.c \
	modbusd.h \
	message.c \
	message.h \
	fabric.c \
	fabric.h \
	modbus_helper.c \
	modbus_helper.h \
	beacon.c \
	beacon.h \
	config.c \
	config.h \
	module.h \
	module.c \
	process.h \
	process.c \
	procman.h \
	procman.c \
	logger.h \
	logger.c \
	interface.h \
	interface.c \
	logger-file.c \
	logger-file.h \
	ini.c \
	ini.h 


modbusd_CPPFLAGS =-Wall -D_SVID_SOURCE=1

modbusd_LDFLAGS =

modbusd_LDADD = $(LIBCZMQ_LIBS) -lmsgpack

