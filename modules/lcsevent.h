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
#define MEVENT_TRIGGER(ret, evt, key, cmd, flags)						\
	do {																\
		if (PROCESS_NOK(mevent_trigger(evt, key, cmd, flags))) {		\
			char *zpa = NULL;											\
			hdf_write_string(evt->hdfrcv, &zpa);						\
			alog_err("pro %s %d failure %d %s", evt->ename, cmd, evt->errcode, zpa); \
			if (zpa) free(zpa);											\
			return ret;													\
		}																\
	} while(0)
#define MEVENT_TRIGGER_VOID(evt, key, cmd, flags)						\
	do {																\
		if (PROCESS_NOK(mevent_trigger(evt, key, cmd, flags))) {		\
			char *zpa = NULL;											\
			hdf_write_string(evt->hdfrcv, &zpa);						\
			alog_err("pro %s %d failure %d %s", evt->ename, cmd, evt->errcode, zpa); \
			if (zpa) free(zpa);											\
			return;														\
		}																\
	} while(0)
#define MEVENT_TRIGGER_NRET(evt, key, cmd, flags)						\
	do {																\
		if (PROCESS_NOK(mevent_trigger(evt, key, cmd, flags))) {		\
			char *zpa = NULL;											\
			hdf_write_string(evt->hdfrcv, &zpa);						\
			alog_err("pro %s %d failure %d %s", evt->ename, cmd, evt->errcode, zpa); \
			if (zpa) free(zpa);											\
		}																\
	} while(0)

char* lcs_app_secy(acetables *g_ape, char *aname);
HDF* lcs_app_info(acetables *g_ape, char *aname);

char* lcs_get_admin(acetables *g_ape, char *uname, char *aname);
void lcs_remember_user(callbackp *callbacki, char *uname, char *aname);
void lcs_add_track(acetables *g_ape, char *aname, char *uname, char *oname,
				   char *ip, char *url, char *title, char *refer, int mode);

void lcs_set_msg(acetables *g_ape, char *msg, char *from, char *to, int type);

#endif
