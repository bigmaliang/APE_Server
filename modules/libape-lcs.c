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

static HDF* lcs_app_info(callbackp *callbacki, char *aname)
{
	HTBL *etbl = GET_EVENT_TBL(callbacki->g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "aic");
	if (!evt) return NULL;
	
	hdf_set_value(evt->hdfsnd, "aname", aname);
	if (PROCESS_NOK(mevent_trigger(evt, aname, REQ_CMD_APPINFO, FLAGS_SYNC))) {
		alog_err("get %s stat failure %d", aname, evt->errcode);
		return NULL;
	}

	HDF *hdf;
	hdf_init(&hdf);
	hdf_copy(hdf, NULL, evt->hdfrcv);

	return hdf;
}

static bool lcs_user_appjoined(USERS *user, char *aname)
{
	CHANLIST *clist;
	CHANNEL *chan;
	char tok[MAX_CHAN_LEN];
	int toklen;
	

	if (!user || aname) return false;

	snprintf(tok, sizeof(tok), LCS_PIP_NAME"%s", aname);
	toklen = strlen(tok);

	clist = user->chan_foot;

	while (clist != NULL) {
		chan = clist->chaninfo;
		if (chan && !strncmp(chan->name, tok, toklen)) return true;
		clist = clist->next;
	}

	return false;
}

static chatNum* lcs_app_chatnum(callbackp *callbacki, char *aname)
{
	HTBL *table = GET_CHATNUM_TBL(callbacki->g_ape);
	if (!table || !aname) return NULL;

	return (chatNum*)hashtbl_seek(table, aname);
}

static void lcs_app_onjoin(acetables *g_ape, char *aname, char *uname,
						   bool admin)
{
	HTBL *table = GET_CHATNUM_TBL(g_ape);
	if (!table || !aname || !uname) return;

	chatNum *c = (chatNum*)hashtbl_seek(table, aname);
	if (!c) {
		c = chatnum_new();
		hashtbl_append(table, aname, c);
	}

	if (queue_find(c->users, uname, hn_str_cmp) == -1) {
		queue_push_head(c->users, strdup(uname));
	}

	if (admin) {
		if (queue_find(c->admins, uname, hn_str_cmp) == -1) {
			queue_push_head(c->admins, strdup(uname));
		}
	}
}

static void lcs_app_onleft(acetables *g_ape, char *aname, char *uname,
						   bool admin)
{
	HTBL *table = GET_CHATNUM_TBL(g_ape);
	if (!table || !aname || !uname) return;

	chatNum *c = (chatNum*)hashtbl_seek(table, aname);
	if (!c) {
		c = chatnum_new();
		hashtbl_append(table, aname, c);
	}

	queue_remove_entry(c->users, uname, hn_str_cmp);

	if (admin) {
		queue_remove_entry(c->admins, uname, hn_str_cmp);
	}
}

static CHANNEL* lcs_app_get_adminchan(callbackp *callbacki, char *aname)
{
	if (!aname) return NULL;

	chatNum *c = lcs_app_chatnum(callbacki, aname);
	if (!c) goto nobody;

	int max = queue_length(c->admins);
	if (max <= 0) goto nobody;
	
	int sn = neo_rand(max);

	char *admin = (char*)queue_nth_data(c->admins, sn);
	return getchanf(callbacki->g_ape, LCS_PIP_NAME"%s", admin);

nobody:
	CHANNEL *chan = mkchanf(callbacki->g_ape,
							CHANNEL_AUTODESTROY, LCS_PIP_NAME"%s", aname);
	if (!chan) return NULL;
	
	ADD_ANAME_FOR_CHANNEL(chan, aname);
	ADD_PNAME_FOR_CHANNEL(chan, aname);
	return chan;
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

static bool lcs_check_login(callbackp *callbacki, char *aname, char *masn)
{
	HTBL *etbl = GET_EVENT_TBL(callbacki->g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "aic");
	if (!evt) return false;

	hdf_set_value(evt->hdfsnd, "aname", aname);
	if (PROCESS_NOK(mevent_trigger(evt, aname, REQ_CMD_APPINFO, FLAGS_SYNC))) {
		alog_err("get %s stat failure %d", aname, evt->errcode);
		return LCS_ST_FREE;
	}

	char *masndb = hdf_get_value(evt->hdfrcv, "masn", NULL);
	if (masndb && !strcmp(masndb, masn)) {
		return true;
	}
	return false;
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
	chatNum *chatnum;
	
	USERS *user = callbacki->call_user;
	CHANNEL *chan;
	char errstr[64];
	int olnum = 0, errcode = 0, ret;
	stLcs *st = GET_LCS_STAT(callbacki->g_ape);

	char *oname = NULL, *url, *title;
	char tok[128];

	JNEED_STR(callbacki->param, "aname", aname, RETURN_BAD_PARAMS);
	JNEED_INT(callbacki->param, "utime", utime, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "url", url, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "title", title, RETURN_BAD_PARAMS);
	//url = JGET_STR(callbacki->param, "url");
	//title = JGET_STR(callbacki->param, "title");
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
	if (lcs_user_appjoined(user, aname)) {
		goto done;
	}

	chatnum = lcs_app_chatnum(callbacki, aname);
	if (chatnum) {
		olnum = queue_length(chatnum->users);
	}
	
	ret = lcs_service_state(callbacki, aname);
	switch (ret) {
	case LCS_ST_BLACK:
		errcode = 110;
	case LCS_ST_STRANGER:
		errcode = 111;
		goto done;
	case LCS_ST_FREE:
	case LCS_ST_VIPED:
		if (olnum > atoi(READ_CONF("max_online_free"))) {
			errcode = 112;
			goto done;
		}
	case LCS_ST_VIP:
		if (olnum > atoi(READ_CONF("max_online_vip"))) {
			errcode = 113;
			goto done;
		}
	default:
		break;
	}
	
	/*
	 * get user joined channel last time, and try to join again
	 */
	if (lcs_user_join_get(callbacki, uname, aname, &oname) == 0) {
		chan = getchanf(callbacki->g_ape, LCS_PIP_NAME"%s", oname);
		if (chan) {
			join(user, chan, callbacki->g_ape);
			goto done;
		}
	}
		
	/*
	 * last joined cahnnel donot open, join another
	 */
	chan = lcs_app_get_adminchan(callbacki, aname);
	if (chan) {
		join(user, chan, callbacki->g_ape);
		oname = GET_ANAME_FROM_CHANNEL(chan);
	} else {
		/* no admin on, and make aname_channel failure */
		errcode = 7;
	}
	
done:
	if (errcode != 0) {
		sprintf(errstr, "%d", errcode);
		hn_senderr_sub(callbacki, errstr, "ERR_APP_NPASS");
	}

	
	/*
	 * user joined my site.
	 */
	jid = lcs_user_join_set(callbacki, aname, uname, oname, url, title, errcode);
	sprintf(tok, "%u", jid);
	ADD_JID_FOR_USER(user, tok);

	/*
	 * add user to chatlist
	 * keep it track with oname, or aname when oname NULL
	 */
	if (utime == 2) {
		lcs_user_remember_me(callbacki, uname, oname ? oname: aname);
	}

	return (RETURN_NOTHING);
}

static unsigned int lcs_visit(callbackp *callbacki)
{
	char *url, *title;
	unsigned int jid;
	
	mevent_t *evt = (mevent_t*)hashtbl_seek(GET_EVENT_TBL(callbacki->g_ape), "dyn");
	if (!evt) return (RETURN_NOTHING);
	
	JNEED_UINT(callbacki->param, "jid", jid, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "url", url, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "title", title, RETURN_BAD_PARAMS);

	hdf_set_int_value(evt->hdfsnd, "jid", jid);
	hdf_set_value(evt->hdfsnd, "url", url);
	hdf_set_value(evt->hdfsnd, "title", title);
	
	if (PROCESS_NOK(mevent_trigger(evt, NULL, REQ_CMD_VISITSET, FLAGS_NONE))) {
		alog_err("report visit failure %d", evt->errcode);
	}
	
	return (RETURN_NOTHING);
}

static unsigned int lcs_send(callbackp *callbacki)
{
	char *pipe, *msg, *uname = NULL;
	USERS *user;
	json_item *jlist;
	RAW *newraw;

	JNEED_STR(callbacki->param, "pipe", pipe, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "msg", msg, RETURN_BAD_PARAMS);

	jlist = json_new_object();
	json_set_property_strZ(jlist, "msg", msg);
	
	transpipe *spipe = get_pipe(pipe, callbacki->g_ape);
	if (spipe && spipe->type == USER_PIPE) {
		user = (USERS*)(spipe->pipe);
		if (user) uname = GET_UIN_FROM_USER(user);
		if (uname) {
			jlist = post_to_pipe(jlist, RAW_LCSDATA, pipe,
								 callbacki->call_subuser, callbacki->g_ape, true);
			
			newraw = forge_raw("RAW_RECENTLY", jlist);
			push_raw_recently(callbacki->g_ape, newraw, uname);
			POSTRAW_DONE(newraw);
		} else {
			alog_err("get uname failure");
			hn_senderr_sub(callbacki, "120", "ERR_UNAME_NEXIST");
		}
	} else {
		alog_err("get pipe failure");
		hn_senderr_sub(callbacki, "121", "ERR_PIPE_ERROR");
	}

	return (RETURN_NOTHING);
}

static unsigned int lcs_msg(callbackp *callbacki)
{
	char *uname, *msg;
	json_item *jlist;
	RAW *newraw;

	JNEED_STR(callbacki->param, "uname", uname, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "msg", msg, RETURN_BAD_PARAMS);

	jlist = json_new_object();
	json_set_property_strZ(jlist, "msg", msg);
	newraw = forge_raw("RAW_MSG", jlist);

	push_raw_recently(callbacki->g_ape, newraw, uname);
	POSTRAW_DONE(newraw);

	return (RETURN_NOTHING);
}

static unsigned int lcs_joinb(callbackp *callbacki)
{
	char *aname, *masn;
	
	USERS *user = callbacki->call_user;
	CHANNEL *chan;
	char errstr[64];
	int olnum = 0, errcode = 0, ret;
	stLcs *st = GET_LCS_STAT(callbacki->g_ape);

	JNEED_STR(callbacki->param, "aname", aname, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "masn", masn, RETURN_BAD_PARAMS);

	/*
	 * statistics
	 */
	USERS *tuser = GET_USER_FROM_ONLINE(callbacki->g_ape, aname);
	if (tuser == NULL) {
		st->num_user++;
		SET_USER_FOR_ONLINE(callbacki->g_ape, aname, user);
	}

	/*
	 * pre join
	 */
	HDF *apphdf = lcs_app_info(callbacki, aname);
	if (!apphdf) {
		alog_warn("%s info failure", aname);
		hn_senderr_sub(callbacki, "210", "ERR_APP_INFO");
		return (RETURN_NOTHING);
	}
	
	if (strcmp(hdf_get_value(apphdf, "masn", "NULL"), masn)) {
		alog_warn("%s attemp illgal login", aname);
		hn_senderr_sub(callbacki, "211", "ERR_APP_LOGIN");
		return (RETURN_NOTHING);
	}

	chan = getchanf(callbacki->g_ape, LCS_PIP_NAME"%s", aname);
	if (!chan) {
		chan = mkchanf(callbacki->g_ape,
					   CHANNEL_AUTODESTROY, LCS_PIP_NAME"%s", aname);
		if (!chan) {
			alog_err("make channel %s failure", aname);
			hn_senderr_sub(callbacki, "007", "ERR_MAKE_CHANNEL");
			return (RETURN_NOTHING);
		}
		char *pname = hdf_get_value(apphdf, "pname", aname);
		ADD_ANAME_FOR_CHANNEL(chan, aname);
		ADD_PNAME_FOR_CHANNEL(chan, pname);
	}
	if (isonchannel(user, chan)) {
		goto done;
	}
	
	chatNum *chatnum = lcs_app_chatnum(callbacki, aname);
	if (chatnum) {
		olnum = queue_length(chatnum->users);
	}

	ret = hdf_get_int_value(apphdf, "state", LCS_ST_STRANGER);
	switch (ret) {
	case LCS_ST_BLACK:
		errcode = 110;
	case LCS_ST_STRANGER:
		errcode = 111;
		goto done;
	case LCS_ST_FREE:
	case LCS_ST_VIPED:
		if (olnum > atoi(READ_CONF("max_online_free"))) {
			errcode = 112;
			goto done;
		}
	case LCS_ST_VIP:
		if (olnum > atoi(READ_CONF("max_online_vip"))) {
			errcode = 113;
			goto done;
		}
	default:
		break;
	}
	
	/*
	 * join
	 */
	SET_USER_ADMIN(user);
	join(user, chan, callbacki->g_ape);
	
done:
	if (errcode != 0) {
		sprintf(errstr, "%d", errcode);
		hn_senderr_sub(callbacki, errstr, "ERR_APP_NPASS");
	}
	
	return (RETURN_NOTHING);
}

static void lcs_event_onjoin(USERS *user, CHANNEL *chan, acetables *g_ape)
{
	if (!user || !chan) return;

	if (strncmp(chan->name, LCS_PIP_NAME, strlen(LCS_PIP_NAME))) return;

	char *aname = GET_PNAME_FROM_CHANNEL(chan);
	char *uname = GET_UIN_FROM_USER(user);

	bool admin = false;
	if (GET_USER_ADMIN(user)) admin = true;
	
	lcs_app_onjoin(g_ape, aname, uname, admin);
}

static void lcs_event_onleft(USERS *user, CHANNEL *chan, acetables *g_ape)
{
	if (!user || !chan) return;

	if (strncmp(chan->name, LCS_PIP_NAME, strlen(LCS_PIP_NAME))) return;

	char *aname = GET_PNAME_FROM_CHANNEL(chan);
	char *uname = GET_UIN_FROM_USER(user);

	bool admin = false;
	if (GET_USER_ADMIN(user)) admin = true;
	
	lcs_app_onleft(g_ape, aname, uname, admin);
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
	register_cmd("LCS_SEND", 		lcs_send, 		NEED_SESSID, g_ape);
	register_cmd("LCS_MSG", 		lcs_msg, 		NEED_SESSID, g_ape);

	register_cmd("LCS_JOINB", 		lcs_joinb, 		NEED_SESSID, g_ape);
	//register_cmd("LCS_JOIN_A", 		lcs_join_a,		NEED_SESSID, g_ape);
}

static ace_callbacks callbacks = {
	NULL,				/* Called when new user is added */
	NULL,				/* Called when a user is disconnected */
	NULL,				/* Called when new chan is created */
	NULL,				/* Called when a chan removed */
	NULL,				/* Called when a user join a channel */
	lcs_event_onjoin,
	NULL,				/* Called when a user leave a channel */
	lcs_event_onleft,
	NULL,				/* Called at each tick, passing a subuser */
	NULL,				/* Called when a subuser receiv a message */
	NULL,				/* Called when a user allocated */
	NULL,				/* Called when a subuser is added */
	NULL   				/* Called when a subuser is disconnected */
};

APE_INIT_PLUGIN(MODULE_NAME, init_module, callbacks)
