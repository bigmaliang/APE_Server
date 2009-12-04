include ./plateform.mk

CC=gcc -D_GNU_SOURCE

ifdef DARWIN_BUILD

all: modules

modules: libape-spidermonkey.c libape-push.c

	$(CC) -Wall -g -bundle -undefined suppress -flat_namespace -o lib/libmod_spidermonkey.so libape-spidermonkey.c -I../deps/js/src/dist/include/ -L../deps/js/src/ -ljs_static -lstdc++ -L./deps/mysac/ -I./deps/mysac/ -lmysac -L/opt/local/lib/mysql5/mysql/ -lmysqlclient_r

endif
ifdef LINUX_BUILD

CFLAGS=-Wall -g -shared -fPIC -rdynamic

EVENT_BASE = ../../../event
CS_BASE = ../../../lib/cs
INC_EVENT = -I $(EVENT_BASE)/api -I $(EVENT_BASE)/base -I $(EVENT_BASE)/plugin -I $(CS_BASE)/include/ClearSilver
LIB_EVENT = -L $(EVENT_BASE)/api -lmevent -L $(EVENT_BASE)/daemon -lmevent_server -L/usr/local/lib -l neo_utl

all: modules

modules: libape-spidermonkey.c libape-push.c

	$(CC) $(CFLAGS) -Wl,-soname,libmod_spidermonkey.so -o lib/libmod_spidermonkey.so libape-spidermonkey.c -I../deps/js/src/dist/include/ -L../deps/js/src/ -ljs_static -lstdc++ -L./deps/mysac/ -I./deps/mysac/ -lmysac -lmysqlclient_r

#	$(CC) $(CFLAGS) -std=c99 -Wl,-soname,libmod_push.so -o lib/libmod_push.so libape-push.c -I../deps/js/src/dist/include/ -L../deps/js/src/ -ljs_static -lstdc++ -L./deps/mysac/ -I./deps/mysac/ -lmysac -lmysqlclient_r $(INC_EVENT) $(LIB_EVENT)
	$(CC) $(CFLAGS) -std=c99 -Wl,-soname,libmod_push.so -o lib/libmod_push.so libape-push.c $(INC_EVENT) $(LIB_EVENT)
endif
