#include "plugins.h"
#include "global_plugins.h"
#include "libape-lcs.h"

#include "../src/hnpub.h"

#include "mevent.h"
#include "data.h"

#define MODULE_NAME "lcs"

static ace_plugin_infos infos_module = {
	"\"LiveCS\" system",// Module Name
	"1.0",				// Module Version
	"brightMoon",		// Module Author
	"mod_lcs.conf"		// config file (bin/)
};

/*
 * file range 
 */
static int lcs_service_state(char *appid)
{
	mevent_t *evt;
	struct data_cell *pc;
	int ret = LCS_ST_STRANGER;
	
	evt = mevent_init_plugin("lcs", REQ_CMD_APPINFO, FLAGS_SYNC);
	mevent_add_str(evt, NULL, "appid", appid);
	if (PROCESS_NOK(mevent_trigger(evt))) {
		alog_err("get %s stat failure", appid);
		goto done;
	}

	pc = data_cell_search(evt->rcvdata, false, DATA_TYPE_U32, "state");
	if (pc) ret = pc->v.ival;

done:
	mevent_free(evt);
	return ret;
}

static int lcs_channel_admnum(CHANNEL *chan)
{
	if (!chan) return 0;
	
	struct userslist *ulist = chan->head;
	int num = 0;
	
	while (ulist != NULL) {
		if (ulist->level >= 3) num++;
		ulist = ulist->next;
	}
	
	return num;
}

static USERS* lcs_get_admin(CHANNEL *chan, int sn)
{
	if (!chan || sn <= 0) return NULL;

	struct userslist *ulist = chan->head;
	int num = 0;
	
	while (ulist != NULL) {
		if (ulist->level >= 3) num++;
		if (num == sn) return ulist->userinfo;
		ulist = ulist->next;
	}
	
	return NULL;
}

static void tick_static(acetables *g_ape, int lastcall)
{
    stLcs *st = GET_LCS_STAT(g_ape);
    char sql[1024];
    int ret;

    mevent_t *evt;
    evt = mevent_init_plugin("stat", REQ_CMD_REPORT, FLAGS_NONE);
	mevent_add_array(evt, NULL, "sqls");

    memset(sql, 0x0, sizeof(sql));
    snprintf(sql, sizeof(sql), "INSERT INTO lcs (type, count) "
             " VALUES (%d, %u);", ST_ONLINE, g_ape->nConnected);
    mevent_add_str(evt, "sqls", "1", sql);
	
    memset(sql, 0x0, sizeof(sql));
    snprintf(sql, sizeof(sql), "INSERT INTO lcs (type, count) "
             " VALUES (%d, %u);", ST_MSG_TOTAL, st->msg_total);
    mevent_add_str(evt, "sqls", "2", sql);
	st->msg_total = 0;
	
    memset(sql, 0x0, sizeof(sql));
    snprintf(sql, sizeof(sql), "INSERT INTO lcs (type, count) "
             " VALUES (%d, %u);", ST_NUM_USER, st->num_user);
    mevent_add_str(evt, "sqls", "3", sql);
	st->num_user = 0;
	hashtbl_empty(GET_ONLINE_TBL(g_ape), NULL);
	
    ret = mevent_trigger(evt);
    if (PROCESS_NOK(ret)) {
        alog_err("trigger statistic event failure %d", ret);
    }
	mevent_free(evt);
}

/*
 * pro range
 */
static unsigned int lcs_join(callbackp *callbacki)
{
	char *uin, *appid;

	USERS *user = callbacki->call_user;
	subuser *sub = callbacki->call_subuser;
	CHANNEL *chan;
	int olnum, olnum_a, ret;
	stLcs *st = GET_LCS_STAT(callbacki->g_ape);

	JNEED_STR(callbacki->param, "appid", appid, RETURN_BAD_PARAMS);
	uin = GET_UIN_FROM_USER(user);

	/*
	 * statistics
	 */
	st->num_login++;
	USERS *tuser = GET_USER_FROM_ONLINE(callbacki->g_ape, uin);
	if (tuser == NULL) {
		st->num_user++;
		SET_USER_FOR_ONLINE(callbacki->g_ape, uin, nuser);
	}

	/*
	 * pre join
	 */
	chan = getchanf(callbacki->g_ape, LCS_PIP_NAME"%s", appid);
	if (!chan) {
		chan = mkchanf(callbacki->g_ape,
					   CHANNEL_AUTODESTROY | CHANNEL_QUIET,
					   LCS_PIP_NAME"%s", appid);
		if (!chan) {
			alog_err("make channel %s failure", appid);
			hn_senderr(callbacki, "007", "ERR_MAKE_CHANNEL");
			return (RETURN_NOTHING);
		}
	}
	if (isonchannel(user, chan)) {
		goto join;
	}
	olnum = get_channel_usernum(chan);
	olnum_a = lcs_channel_admnum(chan);
	ret = lcs_service_state(appid);
	switch (ret) {
	case LCS_ST_BLACK:
		hn_senderr(callbacki, "010", "ERR_APP_BLACK");
	case LCS_ST_STRANGER:
		hn_senderr(callbacki, "011", "ERR_APP_STRANGER");
		return (RETURN_NOTHING);
	case LCS_ST_FREE:
	case LCS_ST_VIPED:
		if (olnum > atoi(READ_CONF("max_online_free"))) {
			hn_senderr(callbacki, "012", "ERR_APP_LIMITED");
			return (RETURN_NOTHING);
		}
	case LCS_ST_VIP:
		if (olnum > atoi(READ_CONF("max_online_vip"))) {
			hn_senderr(callbacki, "012", "ERR_APP_LIMITED");
			return (RETURN_NOTHING);
		}
	default:
		break;
	}
	
	/*
	 * join
	 */
join:
	join(user, chan, callbacki->g_ape);
	if (!isonchannel(user, chan) && olnum_a > 0) {
		int sn = neo_rand(olnum_a);
		USERS *admin = lcs_get_admin(chan, sn);
		if (admin) {
			char *uina = GET_UIN_FROM_USER(admin);
			CHANNEL *chana = getchanf(callbacki->g_ape,
									  LCS_PIP_NAME"%s_%s", appid, uina);
			if (chana) {
				join(user, chana, callbacki->g_ape);
			}
		}
	}
	
	return (RETURN_NOTHING);
}

static void init_module(acetables *g_ape)
{
	/*
	 * THESE tables MUST maked on aped start, see hnpub.h
	 */
	MAKE_USER_TBL(g_ape);		/* erased in deluser() */
	MAKE_ONLINE_TBL(g_ape);		/* erased in deluser() */
	MAKE_CHATNUM_TBL(g_ape);
	MAKE_LCS_STAT(g_ape, calloc(1, sizeof(stLcs)));
    add_periodical((1000*60*30), 0, tick_static, g_ape, g_ape);
	
	register_cmd("LCS_JOIN", 		lcs_join, 		NEED_SESSID, g_ape);
	register_cmd("LCS_JOIN_A", 		lcs_join_a,		NEED_SESSID, g_ape);
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
	NULL   				/* Called when a subuser is disconnected */
};

APE_INIT_PLUGIN(MODULE_NAME, init_module, callbacks)
