#ifndef __EXTEVENT_H__
#define __EXTEVENT_H__

#include "plugins.h"

#include "mevent.h"
/*
 * mevent_ape_ext.h is suplied by moon/deliver/v, not moon/event/plugin
 * (event though v isn't a moon/event/plugin/XXX backend, but they use
 *  the same protocol, an can handle each other's request)
 */
//#include "mevent_ape_ext.h"

/* TODO hdf_write_string lead mem_leak */
#define EVT_EXT_TRIGGER(ret, evt, key, cmd, flags)						\
	do {																\
		if (PROCESS_NOK(mevent_trigger(evt, key, cmd, flags))) {		\
			char *zpa = NULL;											\
			hdf_write_string(evt->hdfrcv, &zpa);						\
			alog_err("pro %s %d failure %d %s", evt->ename, cmd, evt->errcode, zpa); \
			if (zpa) free(zpa);											\
			return ret;													\
		}																\
	} while(0)
#define EVT_EXT_TRIGGER_VOID(evt, key, cmd, flags)						\
	do {																\
		if (PROCESS_NOK(mevent_trigger(evt, key, cmd, flags))) {		\
			char *zpa = NULL;											\
			hdf_write_string(evt->hdfrcv, &zpa);						\
			alog_err("pro %s %d failure %d %s", evt->ename, cmd, evt->errcode, zpa); \
			if (zpa) free(zpa);											\
			return;														\
		}																\
	} while(0)
#define EVT_EXT_TRIGGER_NRET(evt, key, cmd, flags)						\
	do {																\
		if (PROCESS_NOK(mevent_trigger(evt, key, cmd, flags))) {		\
			char *zpa = NULL;											\
			hdf_write_string(evt->hdfrcv, &zpa);						\
			alog_err("pro %s %d failure %d %s", evt->ename, cmd, evt->errcode, zpa); \
			if (zpa) free(zpa);											\
		}																\
	} while(0)

void ext_e_init(char *evts);

#endif
