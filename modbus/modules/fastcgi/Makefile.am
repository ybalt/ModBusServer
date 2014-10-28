AM_CPPFLAGS = \
	$(LIBCZMQ_CFLAGS)

AM_CFLAGS =\
	 -Wall

bin_PROGRAMS = \
	modbus_fastcgi

modbus_fastcgi_SOURCES =  \
	modbus_fastcgi.c \
	modbus_fastcgi.h \
	../../src/modbus_helper.h \
	../../src/modbus_helper.c

modbus_fastcgi_CPPFLAGS =-Wall 

modbus_fastcgi_LDFLAGS = 

modbus_fastcgi_LDADD = $(LIBCZMQ_LIBS) -lmsgpack -lfcgi
