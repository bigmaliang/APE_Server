#include "plugins.h"
#include "global_plugins.h"
#include "libape-lcs.h"

#include "../src/hnpub.h"

#include "mevent.h"

#include "mevent_aic.h"
#include "mevent_dyn.h"
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
static void lcs_event_init(acetables *g_ape)
{
	MAKE_EVENT_TBL(g_ape);
	HTBL *tbl = GET_EVENT_TBL(g_ape);

	mevent_t *evt;
	char *s = READ_CONF("event_plugin");
	char *tkn[10];
	int nTok = 0;
	nTok = explode(' ', s, tkn, 10);

	while (nTok >= 0) {
		evt = mevent_init_plugin(tkn[nTok]);
		if (evt) {
			hashtbl_append(tbl, tkn[nTok], (void*)evt);
		}
		nTok--;
	}
}

static int lcs_service_state(callbackp *callbacki, char *aname)
{
	HTBL *etbl = GET_EVENT_TBL(callbacki->g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "aic");
	if (!evt) return LCS_ST_FREE;
	
	hdf_set_value(evt->hdfsnd, "aname", aname);
	if (PROCESS_NOK(mevent_trigger(evt, aname, REQ_CMD_APPINFO, FLAGS_SYNC))) {
		alog_err("get %s stat failure %d", aname, evt->errcode);
		return LCS_ST_FREE;
	}

	return hdf_get_int_value(evt->hdfrcv, "state", LCS_ST_STRANGER);
}

static int lcs_user_join_get(callbackp *callbacki, char *uname, char *aname, char **oname)
{
	HTBL *etbl = GET_EVENT_TBL(callbacki->g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "dyn");
	if (!evt) return -1;

	hdf_set_value(evt->hdfsnd, "uname", uname);
	hdf_set_value(evt->hdfsnd, "aname", aname);
	if (PROCESS_NOK(mevent_trigger(evt, uname, REQ_CMD_JOINGET, FLAGS_SYNC))) {
		alog_err("get %s %s info failure %d", uname, aname, evt->errcode);
		return -1;
	}

	HDF *node = hdf_get_obj(evt->hdfrcv, "0");
	char *s;
	while (node) {
		s = hdf_get_value(node, "oname", "");
		if (strcmp(s, "")) {
			*oname = s;
			return 0;
		}
		node = hdf_obj_next(node);
	}

	return 1;
}

static unsigned int lcs_user_join_set(callbackp *callbacki, char *aname,
									  char *uname, char *oname,
									  char *url, char *title, int retcode)
{
	HTBL *etbl = GET_EVENT_TBL(callbacki->g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "dyn");
	if (!evt) return 0;
	
	if (!callbacki || !aname || !uname) return 0;

	char *refer = "";
	struct _http_header_line *hl = callbacki->hlines;
	while (hl) {
		if (!strcasecmp(hl->key.val, "refer")) {
			refer = hl->value.val;
			break;
		}
		hl = hl->next;
	}

	hdf_set_value(evt->hdfsnd, "uname", uname);
	hdf_set_value(evt->hdfsnd, "aname", aname);
	hdf_set_value(evt->hdfsnd, "oname", oname);
	hdf_set_value(evt->hdfsnd, "ip", callbacki->ip);
	hdf_set_value(evt->hdfsnd, "refer", refer);
	hdf_set_value(evt->hdfsnd, "url", url);
	hdf_set_value(evt->hdfsnd, "title", title);

	hdf_set_int_value(evt->hdfsnd, "retcode", retcode);
	
	if (PROCESS_NOK(mevent_trigger(evt, uname, REQ_CMD_JOINSET, FLAGS_SYNC))) {
		alog_err("set for %s %s failure", uname, aname);
		return 0;
	}

	return hdf_get_int_value(evt->hdfrcv, "id", 0);
}

static void lcs_user_remember_me(callbackp *callbacki, char *uname, char *aname)
{
	HTBL *etbl = GET_EVENT_TBL(callbacki->g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "aic");
	if (!evt) return;

	hdf_set_value(evt->hdfsnd, "uname", uname);
	hdf_set_value(evt->hdfsnd, "aname", aname);
	hdf_set_value(evt->hdfsnd, "ip", callbacki->ip);
	if (PROCESS_NOK(mevent_trigger(evt, uname, REQ_CMD_USERJOIN, FLAGS_NONE))) {
		alog_err("remember %s %s failure %d", uname, aname, evt->errcode);
	}
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
	HTBL *etbl = GET_EVENT_TBL(g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "rawdb");
	if (!evt) return;
	
    stLcs *st = GET_LCS_STAT(g_ape);
    char sql[1024];
    int ret;

    memset(sql, 0x0, sizeof(sql));
    snprintf(sql, sizeof(sql), "INSERT INTO counter (type, count) "
             " VALUES (%d, %u);", ST_ONLINE, g_ape->nConnected);
    hdf_set_value(evt->hdfsnd, "sqls.1", sql);
	
    memset(sql, 0x0, sizeof(sql));
    snprintf(sql, sizeof(sql), "INSERT INTO counter (type, count) "
             " VALUES (%d, %lu);", ST_MSG_TOTAL, st->msg_total);
    hdf_set_value(evt->hdfsnd, "sqls.2", sql);
	st->msg_total = 0;
	
    memset(sql, 0x0, sizeof(sql));
    snprintf(sql, sizeof(sql), "INSERT INTO counter (type, count) "
             " VALUES (%d, %lu);", ST_NUM_USER, st->num_user);
    hdf_set_value(evt->hdfsnd, "sqls.3", sql);
	st->num_user = 0;
	hashtbl_empty(GET_ONLINE_TBL(g_ape), NULL);
	
    ret = mevent_trigger(evt, NULL, REQ_CMD_STAT, FLAGS_NONE);
    if (PROCESS_NOK(ret)) {
        alog_err("trigger statistic event failure %d", ret);
    }
}

/*
 * pro range
 */
static unsigned int lcs_join(callbackp *callbacki)
{
	char *uname, *aname;
	int utime;
	unsigned int jid;
	
	USERS *user = callbacki->call_user;
	CHANNEL *chan;
	char errcode[64];
	int olnum, olnum_a, err = 0, ret;
	stLcs *st = GET_LCS_STAT(callbacki->g_ape);

	CHANNEL *chana;
	char *oname = NULL, *url, *title;
	USERS *admin;
	char tok[128];
	bool joined = false;
	
	JNEED_STR(callbacki->param, "aname", aname, RETURN_BAD_PARAMS);
	JNEED_INT(callbacki->param, "utime", utime, RETURN_BAD_PARAMS);
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
	ret = lcs_service_state(callbacki, aname);
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

	/*
	 * get user joined channel last time, and try to join again
	 */
	if (lcs_user_join_get(callbacki, uname, aname, &oname) == 0) {
		chana = getchanf(callbacki->g_ape, LCS_PIP_NAME"%s_%s", aname, oname);
		if (chana) {
			join(user, chana, callbacki->g_ape);
			joined = true;
		}
	}
		
	/*
	 * last joined cahnnel donot open, join another
	 */
	if (!joined && olnum_a > 0) {
		int sn = neo_rand(olnum_a);
		admin = lcs_get_admin(chan, sn);
		if (admin) {
			oname = GET_UIN_FROM_USER(admin);
			chana = getchanf(callbacki->g_ape, LCS_PIP_NAME"%s_%s", aname, oname);
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
	jid = lcs_user_join_set(callbacki, aname,
							uname, oname, url, title, err);
	if (jid > 0 ) {
		sprintf(tok, "%u", jid);
		ADD_JID_FOR_USER(user, tok);
	}

	/* add user to chatlist */
	if (utime == 2) {
		lcs_user_remember_me(callbacki, uname, aname);
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
	evt = mevent_init_plugin("rawdb");
	snprintf(sql, sizeof(sql), "INSERT INTO visit (jid, url, title) "
			 " VALUES (%u, '%s', '%s')", jid, url, title);
	hdf_set_value(evt->hdfsnd, "sqls", sql);
	mevent_trigger(evt, NULL, REQ_CMD_ACCESS, FLAGS_NONE);
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
	lcs_event_init(g_ape);
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
