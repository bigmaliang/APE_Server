#include <mysql/mysql.h>
#include "plugins.h"
#include "global_plugins.h"
#include "libape-fkq.h"

#include "../src/hnpub.h"

/*
 * libape-fkq require mevent header and library for MEVENT(a non-block db backend):
 * http://code.google.com/p/cmoon/source/browse/trunk/event/
 */
#include "mevent.h"
#include "data.h"
#include "mevent_uic.h"
#include "mevent_db_community.h"

#define MODULE_NAME "fkq"

static ace_plugin_infos infos_module = {
	"\"Fkq\" system", // Module Name
	"1.0",			   // Module Version
	"hunantv",		   // Module Author
	"mod_fkq.conf"	  // config file (bin/)
};

/*
 * file range 
 */
static int get_fkq_stat(char *uin)
{
	mevent_t *evt = NULL;
	struct data_cell *pc;
	int stat = ST_CLOSE, ret;
	
	if (!hn_isvaliduin(uin)) return ST_CLOSE;

	evt = mevent_init_plugin("uic", REQ_CMD_MYSETTING, FLAGS_SYNC);
	mevent_add_u32(evt, NULL, "uin", atoi(uin));
	ret = mevent_trigger(evt);
	if (PROCESS_NOK(ret)) {
		alog_err("get fkq set for user %s failure %d", uin, ret);
		goto done;
	}

	pc = data_cell_search(evt->rcvdata, false, DATA_TYPE_U32, "fkqstat");
	if (pc != NULL) {
		stat = pc->v.ival;
	}

 done:
	mevent_free(evt);
	return stat;
}

static void set_fkq_stat(char *uin, bool open)
{
	mevent_t *evt = mevent_init_plugin("uic", REQ_CMD_UPMYSETTING, FLAGS_NONE);
	if (!evt) return;
	
	mevent_add_u32(evt, NULL, "uin", atoi(uin));
	if (open)
		mevent_add_u32(evt, NULL, "fkqstat", 1);
	else
		mevent_add_u32(evt, NULL, "fkqstat", 0);
	mevent_trigger(evt);
	mevent_free(evt);
}

static int get_fkq_level(char *uin)
{
	mevent_t *evt;
	struct data_cell *pc;
	int level = 0, ret;

	if (!hn_isvaliduin(uin)) return 0;
	
	evt = mevent_init_plugin("uic", REQ_CMD_MYSETTING, FLAGS_SYNC);
	mevent_add_u32(evt, NULL, "uin", atoi(uin));
	ret = mevent_trigger(evt);
	if (PROCESS_NOK(ret)) {
		alog_err("get fkq for user %s failure %d", uin, ret);
		goto done;
	}

	pc = data_cell_search(evt->rcvdata, false, DATA_TYPE_U32, "fkqlevel");
	if (pc != NULL) {
		level = pc->v.ival;
	}

 done:
	mevent_free(evt);
	return level;
}

static int get_channel_usernum(CHANNEL *chan)
{
	if (chan == NULL) return 0;

	struct userslist *ulist = chan->head;
	int num = 0;
	
	while (ulist != NULL) {
		num++;
		ulist = ulist->next;
	}

	return num;
}

/*
 * pro range
 */

/*
 * {"fkqstat": 1, "hostfkqlevel": 1, "hostfkqinfo": {"hostchatnum": 32}}
 */
static unsigned int fkq_init(callbackp *callbacki)
{
	char *uin, *nick, *hostUin;
	USERS *nuser;
	subuser *sub;
	st_fkq *st = GET_FKQ_STAT(callbacki->g_ape);
	json_item *jlist = NULL;
	RAW *newraw;

	JNEED_STR(callbacki->param, "uin", uin, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "nick", nick, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "hostUin", hostUin, RETURN_BAD_PARAMS);

	if (!hn_isvaliduin(uin) || !hn_isvaliduin(hostUin)) {
		return (RETURN_BAD_PARAMS);
	}

	nuser = callbacki->call_user;
	sub = callbacki->call_subuser;

	ADD_NICK_FOR_USER(nuser, nick);
	
	int stat, level, chatnum;
	stat = get_fkq_stat(uin);
	level = get_fkq_level(hostUin);
	chatnum = 0;
	if (level == 1) {
		CHANNEL *chan = getchanf(callbacki->g_ape, FKQ_PIP_NAME"%s", hostUin);
		if (chan == NULL) {
			chan = mkchanf(callbacki->g_ape, CHANNEL_AUTODESTROY,
						   FKQ_PIP_NAME"%s", hostUin);
			ADD_FKQ_HOSTUIN(chan, hostUin);
		}

		chatnum = get_channel_usernum(chan);

		if (isonchannel(nuser, chan) && chatnum > 0)
			chatnum--;
	}
	
	json_item *info = json_new_object();
	json_set_property_intZ(info, "hostchatnum", chatnum);

	jlist = json_new_object();
	json_set_property_intZ(jlist, "fkqstat", stat);
	json_set_property_intZ(jlist, "hostfkqlevel", level);
	json_set_property_objZ(jlist, "hostfkinfo", info);
	newraw = forge_raw("FKQ_INIT", jlist);
	post_raw_sub(newraw, sub, callbacki->g_ape);
	POSTRAW_DONE(newraw);

	return (RETURN_NOTHING);
}

static unsigned int fkq_join(callbackp *callbacki)
{
	char *hostUin;

	USERS *user = callbacki->call_user;
	subuser *sub = callbacki->call_subuser;
	CHANNEL *chan;

	JNEED_STR(callbacki->param, "hostUin", hostUin, RETURN_BAD_PARAMS);
	
	if (!hn_isvaliduin(hostUin)) {
		return (RETURN_BAD_PARAMS);
	}
	
	if (get_fkq_level(hostUin) == 1) {
		chan = getchanf(callbacki->g_ape, FKQ_PIP_NAME"%s", hostUin);
#if 0
		if ((chan = getchanf(callbacki->g_ape, FKQ_PIP_NAME"%s", hostUin))
			== NULL) {
			chan = mkchanf(callbacki->g_ape, CHANNEL_AUTODESTROY,
						   FKQ_PIP_NAME"%s", hostUin);
			/*
			 * don't set channel private here, so, every channel should be return by
			 * session command with subuser_restore().
			 * Client MUST filter these channels, popup hostuin's fangke user
			 */
			//SET_CHANNEL_PRIVATE(chan);
		}
#endif
		if (chan != NULL) {
			join(user, chan, callbacki->g_ape);
			if (callbacki->call_subuser != NULL) {
				ADD_SUBUSER_HOSTUIN(callbacki->call_subuser, hostUin);
			}
			post_raw_sub_recently(callbacki->g_ape, sub, hostUin, RRC_TYPE_GROUP_FKQ);
		}
	}
	
	return (RETURN_NOTHING);
}

static unsigned int fkq_open(callbackp *callbacki)
{
	char *uin;
	json_item *jlist = NULL;
	RAW *newraw;
	subuser *sub = callbacki->call_subuser;

	JNEED_STR(callbacki->param, "uin", uin, RETURN_BAD_PARAMS);

	set_fkq_stat(uin, true);
	fkq_join(callbacki);

	jlist = json_new_object();
	newraw = forge_raw("FKQ_OPENED", jlist);
	post_raw_sub(newraw, sub, callbacki->g_ape);
	POSTRAW_DONE(newraw);

	return (RETURN_NOTHING);
}

static unsigned int fkq_close(callbackp *callbacki)
{
	char *uin;
	json_item *jlist = NULL;
	RAW *newraw;
	USERS *nuser = callbacki->call_user;
	subuser *sub = callbacki->call_subuser;

	JNEED_STR(callbacki->param, "uin", uin, RETURN_BAD_PARAMS);

	set_fkq_stat(uin, false);

	CHANLIST *list = nuser->chan_foot, *tList;

	while (list != NULL) {
		tList = list->next;
		if (strncmp(list->chaninfo->name, FKQ_PIP_NAME, strlen(FKQ_PIP_NAME)) == 0) {
			left(nuser, list->chaninfo, callbacki->g_ape);
		}
		list = tList;
	}

	jlist = json_new_object();
	newraw = forge_raw("FKQ_CLOSED", jlist);
	post_raw_sub(newraw, sub, callbacki->g_ape);
	POSTRAW_DONE(newraw);

	return (RETURN_NOTHING);
}

static unsigned int fkq_send(callbackp *callbacki)
{
	json_item *jlist = NULL;
	RAW *newraw;
	char *msg, *pipe, *uinfrom, *uinto;
	CHANNEL *chan;
	USERS *user = callbacki->call_user;

	uinfrom = GET_UIN_FROM_USER(user);
	
	JNEED_STR(callbacki->param, "msg", msg, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "pipe", pipe, RETURN_BAD_PARAMS);

	transpipe *spipe = get_pipe(pipe, callbacki->g_ape);
	if (spipe) {
		jlist = json_new_object();
		json_set_property_strZ(jlist, "msg", msg);
		newraw = post_to_pipe(jlist, RAW_FKQDATA, pipe,
							  callbacki->call_subuser, callbacki->g_ape);

		/* push raw to recently */
		switch (spipe->type) {
		case CHANNEL_PIPE:
			chan = (CHANNEL*)(spipe->pipe);
			if (chan) {
				uinto = GET_FKQ_HOSTUIN(chan);
				if (uinto) {
					push_raw_recently_group(callbacki->g_ape, newraw,
											uinto, RRC_TYPE_GROUP_FKQ);
				}
			}
			break;
		default:
			user = (USERS*)(spipe->pipe);
			if (user) {
				uinto = GET_UIN_FROM_USER(user);
				if (uinto) {
					push_raw_recently_single(callbacki->g_ape, newraw,
											 uinfrom, uinto);
				}
			}
			break;
		}
		
		POSTRAW_DONE(newraw);
	}
	
	return (RETURN_NOTHING);
}

static void fkq_event_delsubuser(subuser *del, acetables *g_ape)
{
	char *uin = GET_SUBUSER_HOSTUIN(del);
	if (uin != NULL) {
		USERS *user = del->user;
		subuser *cur = user->subuser;
		char *otheruin;
		bool lastone = true;
		while (cur != NULL) {
			otheruin = GET_SUBUSER_HOSTUIN(cur);
			if (cur != del && otheruin != NULL && !strcmp(uin, otheruin)) {
				lastone = false;
				break;
			}
			cur = cur->next;
		}
			
		if (lastone) {
			CHANNEL *chan = getchanf(g_ape, FKQ_PIP_NAME"%s", uin);
			if (chan != NULL) {
				left(del->user, chan, g_ape);
			}
		}
	}
} 

static void init_module(acetables *g_ape)
{
	MAKE_USER_TBL(g_ape);
	MAKE_ONLINE_TBL(g_ape);
	MAKE_FKQ_STAT(g_ape, calloc(1, sizeof(st_fkq)));
	
	register_cmd("FKQ_INIT", fkq_init, NEED_SESSID, g_ape);
	register_cmd("FKQ_JOIN", fkq_join, NEED_SESSID, g_ape);
	register_cmd("FKQ_OPEN", fkq_open, NEED_SESSID, g_ape);
	register_cmd("FKQ_CLOSE", fkq_close, NEED_SESSID, g_ape);
	register_cmd("FKQ_SEND", fkq_send, NEED_SESSID, g_ape);
}

static ace_callbacks callbacks = {
	NULL,				/* Called when new user is added */
	NULL,				/* Called when a user is disconnected */
	NULL,				/* Called when new chan is created */
	NULL,				/* Called when a chan removed */
	NULL,				/* Called when a user join a channel */
	NULL,				/* Called when a user leave a channel */
	NULL,				/* Called at each tick, passing a subuser */
	NULL,				/* Called when a subuser receiv a message */
	NULL,				/* Called when a user allocated */
	NULL,				/* Called when a subuser is added */
	fkq_event_delsubuser   /* Called when a subuser is disconnected */
};

APE_INIT_PLUGIN(MODULE_NAME, init_module, callbacks)
