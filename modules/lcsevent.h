#ifndef __LCSEVENT_H__
#define __LCSEVENT_H__

#include "plugins.h"
#include "libape-lcs.h"

#include "mevent.h"
#include "mevent_aic.h"
#include "mevent_dyn.h"
#include "mevent_rawdb.h"
#include "mevent_msg.h"

HDF* lcs_app_info(acetables *g_ape, char *aname);

int lcs_user_join_get(acetables *g_ape, char *uname, char *aname, char **oname);
unsigned int lcs_user_join_set(callbackp *callbacki, char *aname,
							   char *uname, char *oname,
							   char *url, char *title, char *ref, int retcode);
void lcs_user_remember_me(callbackp *callbacki, char *uname, char *aname);

void lcs_set_msg(acetables *g_ape, char *msg, char *from, char *to, int type);

#endif
