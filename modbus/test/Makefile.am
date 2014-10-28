AM_CPPFLAGS = \
	$(LIBCZMQ_CFLAGS)

AM_CFLAGS =\
	 -Wall

bin_PROGRAMS = \
	worker \
	broker 

worker_SOURCES =  \
	worker.c \
	common.h \
	../src/modbus_helper.h \
	../src/modbus_helper.c

broker_SOURCES = \
	broker.c \
	common.h \
	../src/modbus_helper.h \
	../src/modbus_helper.c
	

broker_CPPFLAGS =-Wall 
worker_CPPFLAGS =-Wall 

broker_LDFLAGS = 
worker_LDFLAGS =

broker_LDADD = $(LIBCZMQ_LIBS) -lmsgpack 
worker_LDADD = $(LIBCZMQ_LIBS) -lmsgpack
