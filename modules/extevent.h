#ifndef __EXTEVENT_H__
#define __EXTEVENT_H__

#include "plugins.h"

#include "mevent.h"
/*
 * mevent_ape_ext.h is suplied by moon/deliver/v, not moon/event/plugin
 * (event though v isn't a moon/event/plugin/XXX backend, but they use
 *  the same protocol, an can handle each other's request)
 */
#include "mevent_ape_ext.h"

extern char *id_me, *id_v;


/*
 * init stbl, which refer to the other apeds(also named x) and v
 *   we name (x,v) snake for convenient
 */
void ext_e_init(char *evts);


/*
 * notify v, an user onlined on me
 */
NEOERR* ext_e_useron(char *uin);

/*
 * notify v, an user offlined on me
 */
NEOERR* ext_e_useroff(char *uin);

/*
 * notify v OR x, you got a message
 */
NEOERR* ext_e_msgsnd(char *fuin, char *tuin, char *msg);

void ext_event_static(acetables *g_ape, int lastcall);


/* TODO hdf_write_string lead mem_leak */
#define EVT_EXT_TRIGGER(evt, key, cmd, flags)							\
	do {																\
		if (PROCESS_NOK(mevent_trigger(evt, key, cmd, flags))) {		\
			char *zpa = NULL;											\
			hdf_write_string(evt->hdfrcv, &zpa);						\
			return nerr_raise(NERR_ASSERT, "pro %s %d failure %d %s", evt->ename, cmd, evt->errcode, zpa); \
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

#endif