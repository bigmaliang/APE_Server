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

EVENT_BASE = ../../../lib/mevent
INC_EVENT = -I $(EVENT_BASE) -I ../../../event/plugin -I /usr/include/ClearSilver
LIB_EVENT = -L $(EVENT_BASE) -lmevent -l neo_utl

all: modules

modules: libape-spidermonkey.c libape-push.c libape-fkq.c

#	$(CC) $(CFLAGS) -Wl,-soname,libmod_spidermonkey.so -o lib/libmod_spidermonkey.so libape-spidermonkey.c -I../deps/js/src/dist/include/ -L../deps/js/src/ -ljs_static -lstdc++ -L./deps/mysac/ -I./deps/mysac/ -lmysac -lmysqlclient_r

#	$(CC) $(CFLAGS) -std=c99 -Wl,-soname,libmod_push.so -o lib/libmod_push.so libape-push.c -I../deps/js/src/dist/include/ -L../deps/js/src/ -ljs_static -lstdc++ -L./deps/mysac/ -I./deps/mysac/ -lmysac -lmysqlclient_r $(INC_EVENT) $(LIB_EVENT)
#	$(CC) $(CFLAGS) -std=c99 -Wl,-soname,libmod_push.so -o lib/libmod_push.so libape-push.c $(INC_EVENT) $(LIB_EVENT)
#	$(CC) $(CFLAGS) -std=c99 -Wl,-soname,libmod_fkq.so -o lib/libmod_fkq.so libape-fkq.c $(INC_EVENT) $(LIB_EVENT)
	$(CC) $(CFLAGS) -std=c99 -Wl,-soname,libmod_lcs.so -o lib/libmod_lcs.so libape-lcs.c $(INC_EVENT) $(LIB_EVENT)
endif
