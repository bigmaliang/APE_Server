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

void lcs_event_init(char *evts);
char* lcs_app_secy(char *aname);
HDF* lcs_app_info(char *aname);

char* lcs_get_admin(char *uname, char *aname);
void lcs_remember_user(const char *ip, char *uname, char *aname);
void lcs_add_track(char *aname, char *uname, char *oname,
				   char *ip, char *url, char *title, char *refer, int mode);
void lcs_set_msg(char *msg, char *from, char *to, int type);

void lcs_static(acetables *g_ape, int lastcall);

#endif
