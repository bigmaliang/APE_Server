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

static bool user_is_black(char *uin, char *buin, int type)
{
	mevent_t *evt;
	int ret;

	if (!hn_isvaliduin(uin) || !hn_isvaliduin(buin)) return true;
	
	evt = mevent_init_plugin("uic", REQ_CMD_ISBLACK, FLAGS_SYNC);
	mevent_add_u32(evt, NULL, "uin", atoi(uin));
	mevent_add_u32(evt, NULL, "blackuin", atoi(buin));
	mevent_add_u32(evt, NULL, "blacktype", type);
	ret = mevent_trigger(evt);
	mevent_free(evt);

	if (ret == REP_OK_ISBLACK)
		return true;

	if (PROCESS_NOK(ret)) {
		alog_err("fkq for black juedge %s:%s failure %d", uin, buin, ret);
	}

	return false;
}

static int user_set_black(char *uin, char *buin, int type, int op)
{
	mevent_t *evt;
	int ret;

	if (!hn_isvaliduin(uin) || !hn_isvaliduin(buin)) return true;

	evt = mevent_init_plugin("uic", op, FLAGS_SYNC);
	mevent_add_u32(evt, NULL, "uin", atoi(uin));
	mevent_add_u32(evt, NULL, "blackuin", atoi(buin));
	mevent_add_u32(evt, NULL, "blacktype", type);
	ret = mevent_trigger(evt);
	mevent_free(evt);

	if (PROCESS_NOK(ret)) {
		alog_err("fkq for black juedge %s:%s failure %d", uin, buin, ret);
	}

	return ret;
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
	char *uin, *hostUin;

	USERS *user = callbacki->call_user;
	subuser *sub = callbacki->call_subuser;
	CHANNEL *chan;

	JNEED_STR(callbacki->param, "hostUin", hostUin, RETURN_BAD_PARAMS);
	uin = GET_UIN_FROM_USER(user);
	
	if (!hn_isvaliduin(hostUin)) {
		return (RETURN_BAD_PARAMS);
	}
	
	if (get_fkq_level(hostUin) == 1) {
		chan = getchanf(callbacki->g_ape, FKQ_PIP_NAME"%s", hostUin);
		if (chan != NULL) {
			if (user_is_black(hostUin, uin, BLACK_TYPE_MIM_FKQ)) {
				alog_warn("%s in %s's fkq's blacklist", uin, hostUin);
				hn_senderr(callbacki, "101", "ERR_IS_BLACK");
			} else {
				join(user, chan, callbacki->g_ape);
				if (callbacki->call_subuser != NULL) {
					ADD_SUBUSER_HOSTUIN(callbacki->call_subuser, hostUin);
				}
				post_raw_sub_recently(callbacki->g_ape, sub, hostUin,
									  RRC_TYPE_GROUP_FKQ);
			}
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

static unsigned int fkq_blacklist(callbackp *callbacki)
{
	json_item *jlist = NULL;
	RAW *newraw;
	
	mevent_t *evt;
	struct data_cell *pc, *cc;

	USERS *user = callbacki->call_user;
	subuser *sub = callbacki->call_subuser;
	char *uin;
	int btype, ret;

	uin = GET_UIN_FROM_USER(user);

	JNEED_INT(callbacki->param, "blacktype", btype, RETURN_BAD_PARAMS);

	json_item *jblack, *jblist = json_new_array();
	
	evt = mevent_init_plugin("uic", REQ_CMD_BLACKLIST, FLAGS_SYNC);
	mevent_add_u32(evt, NULL, "uin", atoi(uin));
	ret = mevent_trigger(evt);
	if (PROCESS_NOK(ret)) {
		alog_err("get %s black list failure %d", uin, ret);
		goto done;
	}

	pc = data_cell_search(evt->rcvdata, false, DATA_TYPE_ARRAY, "black");
	if (pc) {
		iterate_data(pc) {
			cc = pc->v.aval->items[t_rsv_i];
			
			if (cc->type != DATA_TYPE_U32) continue;
			jblack = json_new_object();
			json_set_property_intZ(jblack, (char*)cc->key, (long int)cc->v.ival);
			json_set_element_obj(jblist, jblack);
		}
	}

 done:
	jlist = json_new_object();
	json_set_property_objZ(jlist, "black", jblist);
	newraw = forge_raw("FKQ_BLACKLIST", jlist);
	post_raw_sub(newraw, sub, callbacki->g_ape);
	POSTRAW_DONE(newraw);

	mevent_free(evt);
	return (RETURN_NOTHING);
}

static unsigned int fkq_blackop(callbackp *callbacki)
{
	USERS *user = callbacki->call_user;
	char *uin, *buin, *op;
	int btype, ret;

	uin = GET_UIN_FROM_USER(user);

	JNEED_STR(callbacki->param, "op", op, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "blackuin", buin, RETURN_BAD_PARAMS);
	JNEED_INT(callbacki->param, "blacktype", btype, RETURN_BAD_PARAMS);

	if (!strcmp(op, "add")) {
		ret = user_set_black(uin, buin, btype, REQ_CMD_ADDBLACK);
		if (PROCESS_OK(ret)) {
			hn_senddata(callbacki, "999", "OPERATION_SUCCESS");
		} else if (ret == REP_ERR_ALREADYBLACK) {
			hn_senderr(callbacki, "111", "ERR_ALREADYBLACK");
		} else {
			hn_senderr(callbacki, "1001", "OPERATION_FAILURE");
		}
	} else if (!strcmp(op, "del")) {
		ret = user_set_black(uin, buin, btype, REQ_CMD_DELBLACK);
		if (PROCESS_OK(ret)) {
			hn_senddata(callbacki, "999", "OPERATION_SUCCESS");
		} else if (ret == REP_ERR_ALREADYBLACK) {
			hn_senderr(callbacki, "112", "ERR_NOTBLACK");
		} else {
			hn_senderr(callbacki, "1001", "OPERATION_FAILURE");
		}
	} else if (!strcmp(op, "is")) {
		if (user_is_black(uin, buin, btype)) {
			hn_senddata(callbacki, "1", "IS_BLACK");
		} else {
			hn_senddata(callbacki, "0", "NOT_BLACK");
		}
	} else if (!strcmp(op, "am")) {
		if (user_is_black(buin, uin, btype)) {
			hn_senddata(callbacki, "1", "AM_BLACK");
		} else {
			hn_senddata(callbacki, "0", "NOT_BLACK");
		}
	} else {
		return (RETURN_BAD_PARAMS);
	}

	return (RETURN_NOTHING);
}

static unsigned int fkq_send(callbackp *callbacki)
{
	json_item *jlist = NULL;
	RAW *newraw;
	char *msg, *pipe, *uinfrom, *uinto = NULL;
	CHANNEL *chan;
	USERS *user = callbacki->call_user;

	uinfrom = GET_UIN_FROM_USER(user);
	
	JNEED_STR(callbacki->param, "msg", msg, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "pipe", pipe, RETURN_BAD_PARAMS);

	bool post = true;
	transpipe *spipe = get_pipe(pipe, callbacki->g_ape);
	if (spipe) {
		if (spipe->type == CHANNEL_PIPE) {
			chan = (CHANNEL*)(spipe->pipe);
			if (chan) {
				uinto = GET_FKQ_HOSTUIN(chan);
				if (uinto && user_is_black(uinto, uinfrom, BLACK_TYPE_MIM_FKQ)) {
					post = false;
				}
			}
		} else {
			user = (USERS*)(spipe->pipe);
			if (user) {
				uinto = GET_UIN_FROM_USER(user);
				if (uinto && user_is_black(uinto, uinfrom, BLACK_TYPE_MIM_USER)) {
					post = false;
				}
			}
		}

		if (post) {
			jlist = json_new_object();
			json_set_property_strZ(jlist, "msg", msg);
			newraw = post_to_pipe(jlist, RAW_FKQDATA, pipe,
								  callbacki->call_subuser, callbacki->g_ape);

			/* push raw to recently */
			if (uinto) {
				if (spipe->type == CHANNEL_PIPE) {
					push_raw_recently_group(callbacki->g_ape, newraw,
											uinto, RRC_TYPE_GROUP_FKQ);
				} else {
					push_raw_recently_single(callbacki->g_ape, newraw,
											 uinfrom, uinto);
				}
			}
			
			POSTRAW_DONE(newraw);
		} else {
			alog_warn("user %s in %s:%d's black", uinfrom, uinto, spipe->type);
			hn_senderr(callbacki, "101", "ERR_IS_BLACK");
		}
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
	register_cmd("FKQ_BLACKLIST", fkq_blacklist, NEED_SESSID, g_ape);
	register_cmd("FKQ_BLACKOP", fkq_blackop, NEED_SESSID, g_ape);
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
