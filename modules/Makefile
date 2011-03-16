include ./plateform.mk
include ./mysql.mk

CC=gcc -D_GNU_SOURCE -DTCP_CORK -DPOSTRAW_CHECK

ifeq ($(HAS_MYSQL), yes)
	MYSQL_FLAGS = -L./deps/mysac/ -I./deps/mysac/ -lmysac -lmysqlclient_r
endif

ifdef DARWIN_BUILD
all: modules
modules: libape-spidermonkey.c libape-push.c
	$(CC) -Wall -bundle -undefined suppress -flat_namespace -o lib/libmod_spidermonkey.so libape-spidermonkey.c -I../deps/js/src/dist/include/ -L../deps/js/src/ -ljs_static -lstdc++ $(MYSQL_FLAGS)
endif

ifdef LINUX_BUILD

CFLAGS = -g -Wall -shared -fPIC -rdynamic

INC_EVENT = -I ../../moon/lib/mevent -I ../../moon/event/plugin -I /usr/local/include/ClearSilver
LIB_EVENT = -L ../../moon/lib/mevent -lmevent -l neo_utl

PATH_V = ../../moon/deliver/v/
INC_V = -I $(PATH_V)

all: modules

modules: libape-spidermonkey.c \
		 libape-lcs.c lcsevent.c lcsevent.h \
		 libape-ext.c extevent.c extevent.h
	$(CC) $(CFLAGS) -std=c99 -Wl,-soname,libmod_lcs.so -o lib/libmod_lcs.so lcsevent.c libape-lcs.c $(INC_EVENT) $(LIB_EVENT)
	$(CC) $(CFLAGS) -std=c99 -Wl,-soname,libmod_ext.so -o lib/libmod_ext.so extsrv.c extevent.c libape-ext.c $(PATH_V)udps.c $(PATH_V)parse.c \
	$(INC_EVENT) $(LIB_EVENT) $(INC_V)

endif
