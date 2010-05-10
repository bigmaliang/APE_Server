#include "plugins.h"
#include "global_plugins.h"
#include "libape-lcs.h"

#include "../src/hnpub.h"

#include "mevent.h"
#include "data.h"

#include "mevent_aic.h"
#include "mevent_rawdb.h"

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
static int lcs_service_state(char *aname)
{
	mevent_t *evt;
	struct data_cell *pc;
	int ret = LCS_ST_STRANGER;
	unsigned int aid = hash_string(aname);
	
	evt = mevent_init_plugin("aic", REQ_CMD_APPINFO, FLAGS_SYNC);
	//mevent_add_str(evt, NULL, "aname", aname);
	mevent_add_u32(evt, NULL, "aid", aid);
	if (PROCESS_NOK(mevent_trigger(evt))) {
		alog_err("get %s stat failure", aname);
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

static unsigned int lcs_join_report(callbackp *callbacki, char *aname,
									char *uname, char *unamea,
									char *url, char *title, int retcode)
{
	if (!callbacki || !aname || !uname) return 0;

	if (!unamea) unamea = "none";
	if (!url) url = "none";
	if (!title) title = "none";
	
	char sql[1024];
	mevent_t *evt;
	struct data_cell *pc;
	evt = mevent_init_plugin("rawdb", REQ_CMD_ACCESS, FLAGS_SYNC);

	unsigned int aid, uid;
	unsigned int ret = 0;
	aid = hash_string(aname);
	uid = hash_string(uname);

	char *refer = "unknown";
	struct _http_header_line *hl = callbacki->hlines;
	while (hl) {
		if (!strcasecmp(hl->key.val, "refer")) {
			refer = hl->value.val;
			break;
		}
		hl = hl->next;
	}

	memset(sql, 0x0, sizeof(sql));
	/* TODO need escape */
	snprintf(sql, sizeof(sql),
			 "INSERT INTO lcsjoin (aid, aname, uid, uname, unamea, "
			 " ip, refer, url, title, retcode) "
			 " VALUES (%u, '%s', %u, '%s', '%s', '%s', '%s', '%s', '%s', %d) "
			 " RETURNING id",
			 aid, aname, uid, uname, unamea,
			 callbacki->ip, refer, url, title, retcode);
	mevent_add_str(evt, NULL, "sqls", sql);
	if (PROCESS_NOK(mevent_trigger(evt))) {
		alog_err("join report for %s %s %s failure",
				 aname, uname, url);
		goto done;
	}

	pc = data_cell_search(evt->rcvdata, false, DATA_TYPE_U32, "id");
	if (pc) ret = pc->v.ival;
	
done:
	mevent_free(evt);
	return ret;
}

static void tick_static(acetables *g_ape, int lastcall)
{
    stLcs *st = GET_LCS_STAT(g_ape);
    char sql[1024];
    int ret;

    mevent_t *evt;
    evt = mevent_init_plugin("rawdb", REQ_CMD_STAT, FLAGS_NONE);
	mevent_add_array(evt, NULL, "sqls");

    memset(sql, 0x0, sizeof(sql));
    snprintf(sql, sizeof(sql), "INSERT INTO counter (type, count) "
             " VALUES (%d, %u);", ST_ONLINE, g_ape->nConnected);
    mevent_add_str(evt, "sqls", "1", sql);
	
    memset(sql, 0x0, sizeof(sql));
    snprintf(sql, sizeof(sql), "INSERT INTO counter (type, count) "
             " VALUES (%d, %lu);", ST_MSG_TOTAL, st->msg_total);
    mevent_add_str(evt, "sqls", "2", sql);
	st->msg_total = 0;
	
    memset(sql, 0x0, sizeof(sql));
    snprintf(sql, sizeof(sql), "INSERT INTO counter (type, count) "
             " VALUES (%d, %lu);", ST_NUM_USER, st->num_user);
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
	char *uname, *aname;

	USERS *user = callbacki->call_user;
	CHANNEL *chan;
	char errcode[64];
	int olnum, olnum_a, err = 0, ret;
	stLcs *st = GET_LCS_STAT(callbacki->g_ape);

	CHANLIST *chanl = user->chan_foot;
	CHANNEL *chana;
	char *unamea = NULL, *url, *title;
	USERS *admin;
	char tok[128];
	bool joined = false;
	
	JNEED_STR(callbacki->param, "aname", aname, RETURN_BAD_PARAMS);
	url = JGET_STR(callbacki->param, "url");
	title = JGET_STR(callbacki->param, "title");
	uname = GET_UIN_FROM_USER(user);

	/*
	 * statistics
	 */
	USERS *tuser = GET_USER_FROM_ONLINE(callbacki->g_ape, uname);
	if (tuser == NULL) {
		st->num_user++;
		SET_USER_FOR_ONLINE(callbacki->g_ape, uname, user);
	}

	/*
	 * pre join
	 */
	chan = getchanf(callbacki->g_ape, LCS_PIP_NAME"%s", aname);
	if (!chan) {
		chan = mkchanf(callbacki->g_ape,
					   CHANNEL_AUTODESTROY, LCS_PIP_NAME"%s", aname);
		if (!chan) {
			alog_err("make channel %s failure", aname);
			hn_senderr_sub(callbacki, "007", "ERR_MAKE_CHANNEL");
			return (RETURN_NOTHING);
		}
	}
	if (isonchannel(user, chan)) {
		goto join;
	}
	olnum = get_channel_usernum(chan);
	olnum_a = lcs_channel_admnum(chan);
	ret = lcs_service_state(aname);
	switch (ret) {
	case LCS_ST_BLACK:
		err = 110;
	case LCS_ST_STRANGER:
		err = 111;
		goto done;
	case LCS_ST_FREE:
	case LCS_ST_VIPED:
		if (olnum > atoi(READ_CONF("max_online_free"))) {
			err = 112;
			goto done;
		}
	case LCS_ST_VIP:
		if (olnum > atoi(READ_CONF("max_online_vip"))) {
			err = 113;
			goto done;
		}
	default:
		break;
	}
	
	/*
	 * join
	 */
join:
	join(user, chan, callbacki->g_ape);
	
	snprintf(tok, sizeof(tok), "%s%s_", LCS_PIP_NAME, aname);
	
	while (chanl) {
		chana  = chanl->chaninfo;
		if (!strncmp(chana->name, tok, strlen(tok))) {
			join(user, chana, callbacki->g_ape);
			joined = true;
			break;
		}
		chanl = chanl->next;
	}
	
	if (!joined && olnum_a > 0) {
		int sn = neo_rand(olnum_a);
		admin = lcs_get_admin(chan, sn);
		if (admin) {
			unamea = GET_UIN_FROM_USER(admin);
			chana = getchanf(callbacki->g_ape, LCS_PIP_NAME"%s_%s", aname, unamea);
			if (chana) {
				join(user, chana, callbacki->g_ape);
			}
		}
	}

	err = 0;
	
done:
	if (err != 0) {
		sprintf(errcode, "%d", err);
		hn_senderr_sub(callbacki, errcode, "ERR_APP_NPASS");
	}
	unsigned int jid = lcs_join_report(callbacki, aname,
									   uname, unamea, url, title, err);
	if (jid > 0 ) {
		sprintf(tok, "%u", jid);
		ADD_JID_FOR_USER(user, tok);
	}
	return (RETURN_NOTHING);
}

static unsigned int lcs_visit(callbackp *callbacki)
{
	char *url, *title;
	unsigned int jid;
	
	JNEED_UINT(callbacki->param, "jid", jid, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "url", url, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "title", title, RETURN_BAD_PARAMS);

	char sql[1024];
	mevent_t *evt;
	evt = mevent_init_plugin("rawdb", REQ_CMD_ACCESS, FLAGS_NONE);
	snprintf(sql, sizeof(sql), "INSERT INTO visit (jid, url, title) "
			 " VALUES (%u, '%s', '%s')", jid, url, title);
	mevent_add_str(evt, NULL, "sqls", sql);
	mevent_trigger(evt);
	mevent_free(evt);
	
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
	register_cmd("LCS_VISIT", 		lcs_visit, 		NEED_SESSID, g_ape);
	//register_cmd("LCS_JOIN_A", 		lcs_join_a,		NEED_SESSID, g_ape);
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
