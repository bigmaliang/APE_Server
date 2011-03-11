#ifndef __LCSEVENT_H__
#define __LCSEVENT_H__

#include "plugins.h"
#include "libape-lcs.h"

#include "mevent.h"
#include "mevent_aic.h"
#include "mevent_dyn.h"
#include "mevent_rawdb.h"
#include "mevent_msg.h"
#include "mevent_place.h"

/* TODO hdf_write_string lead mem_leak */
#define EVT_LCS_TRIGGER(ret, evt, key, cmd, flags)						\
	do {																\
		if (PROCESS_NOK(mevent_trigger(evt, key, cmd, flags))) {		\
			char *zpa = NULL;											\
			hdf_write_string(evt->hdfrcv, &zpa);						\
			alog_err("pro %s %d failure %d %s", evt->ename, cmd, evt->errcode, zpa); \
			if (zpa) free(zpa);											\
			return ret;													\
		}																\
	} while(0)
#define EVT_LCS_TRIGGER_VOID(evt, key, cmd, flags)						\
	do {																\
		if (PROCESS_NOK(mevent_trigger(evt, key, cmd, flags))) {		\
			char *zpa = NULL;											\
			hdf_write_string(evt->hdfrcv, &zpa);						\
			alog_err("pro %s %d failure %d %s", evt->ename, cmd, evt->errcode, zpa); \
			if (zpa) free(zpa);											\
			return;														\
		}																\
	} while(0)
#define EVT_LCS_TRIGGER_NRET(evt, key, cmd, flags)						\
	do {																\
		if (PROCESS_NOK(mevent_trigger(evt, key, cmd, flags))) {		\
			char *zpa = NULL;											\
			hdf_write_string(evt->hdfrcv, &zpa);						\
			alog_err("pro %s %d failure %d %s", evt->ename, cmd, evt->errcode, zpa); \
			if (zpa) free(zpa);											\
		}																\
	} while(0)

void lcs_event_init(char *evts);
char* lcs_app_secy(char *aname);
HDF* lcs_app_info(char *aname);

char* lcs_get_admin(char *uname, char *aname);
void lcs_remember_user(const char *ip, char *uname, char *aname);
void lcs_add_track(char *aname, char *uname, char *oname,
				   char *ip, char *url, char *title, char *refer, int mode);
void lcs_set_msg(char *msg, char *from, char *to, int type);

void tick_static(acetables *g_ape, int lastcall);

#endif
