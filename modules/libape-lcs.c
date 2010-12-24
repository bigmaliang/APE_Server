#include "plugins.h"

#include "libape-lcs.h"
#include "lcsevent.h"

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
appBar* abar_new()
{
	appBar *r = xmalloc(sizeof(appBar));
	r->users = queue_new(0, free);
	r->admins = queue_new(0, free);
	r->dirtyusers = queue_new(0, free);

	return r;
}

void abar_free(void *p)
{
	appBar *cnum = (appBar*)p;

	queue_destroy(cnum->users);
	queue_destroy(cnum->admins);
	queue_destroy(cnum->dirtyusers);
	
	SFREE(cnum);
}

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

static bool lcs_user_appjoined(USERS *user, char *aname)
{
	CHANLIST *clist;
	CHANNEL *chan;
	char *aname_c;

	if (!user || !aname) return false;

	clist = user->chan_foot;

	while (clist != NULL) {
		chan = clist->chaninfo;
		if (chan) {
			aname_c = GET_PNAME_FROM_CHANNEL(chan);
			if (aname_c && !strcmp(aname_c, aname))
				return true;
		}
		clist = clist->next;
	}

	return false;
}

static appBar* lcs_app_bar(acetables *g_ape, char *aname)
{
	HTBL *table = GET_ABAR_TBL(g_ape);
	if (!table || !aname) return NULL;

	return (appBar*)hashtbl_seek(table, aname);
}

static void lcs_app_onjoin(acetables *g_ape, char *aname, char *uname,
						   bool admin, CHANNEL *chan)
{
	HTBL *table = GET_ABAR_TBL(g_ape);
	if (!table || !aname || !uname) return;

	appBar *c = (appBar*)hashtbl_seek(table, aname);
	if (!c) {
		c = abar_new();
		hashtbl_append(table, aname, c);
	}

	if (queue_find(c->users, uname, hn_str_cmp) == -1) {
		queue_push_head(c->users, strdup(uname));
	}

	if (admin) {
		if (queue_find(c->admins, uname, hn_str_cmp) == -1) {
			if (queue_is_empty(c->admins)) {
				QueueEntry *qe;
				char *uname;
				USERS *user;
				queue_iterate(c->users, qe) {
					uname = (char*)qe->data;
					if ((user = GET_USER_FROM_APE(g_ape, uname)) != NULL &&
						!GET_USER_ADMIN(user)) {
						join(user, chan, g_ape);
					}
				}
			}
			queue_push_head(c->admins, strdup(uname));
		}
	}
}

static void lcs_app_onleft(acetables *g_ape, char *aname, char *uname,
						   bool admin, CHANNEL *chan)
{
	HTBL *table = GET_ABAR_TBL(g_ape);
	if (!table || !aname || !uname) return;

	appBar *c = (appBar*)hashtbl_seek(table, aname);
	if (!c) {
		c = abar_new();
		hashtbl_append(table, aname, c);
	}

	queue_remove_entry(c->users, uname, hn_str_cmp);

	if (admin) {
		queue_remove_entry(c->admins, uname, hn_str_cmp);
		/* TODO join my users to another admin */
	}
}

static CHANNEL* lcs_app_get_adminchan(callbackp *callbacki, char *aname, char *secy)
{
	if (!aname) return NULL;

	CHANNEL *chan;
	appBar *c = lcs_app_bar(callbacki->g_ape, aname);
	if (!c) goto nobody;

	int max = queue_length(c->admins);
	if (max <= 0) goto nobody;
	
	int sn = neo_rand(max);

	char *admin = (char*)queue_nth_data(c->admins, sn);
	return getchanf(callbacki->g_ape, LCS_PIP_NAME"%s", admin);

nobody:
	secy = secy ? secy: aname;
	chan = getchanf(callbacki->g_ape, LCS_PIP_NAME"%s", secy);
	if (!chan) {
		chan = mkchanf(callbacki->g_ape,
					   CHANNEL_AUTODESTROY, LCS_PIP_NAME"%s", secy);
		if (chan) {
			ADD_ANAME_FOR_CHANNEL(chan, secy);
			ADD_PNAME_FOR_CHANNEL(chan, aname);
		}
	}
	
	return chan;
}

static void lcs_user_action_notice(acetables *g_ape, USERS *user, char *aname,
								   char *action, char *url, char *title,
								   char *ref, char *ip)
{
	if (!user || !aname || !action) return;

	json_item *jcopy, *jlist;
	RAW *newraw;
	USERS *auser = GET_USER_FROM_APE(g_ape, aname);
	char *from = GET_UIN_FROM_USER(user);
	int type = MSG_TYPE_UNKNOWN;
	
	/*
	 * send visit to admin
	 */
	jlist = json_new_object();
	json_set_property_strZ(jlist, "type", action);
	json_set_property_strZ(jlist, "url", url ? url: "");
	json_set_property_strZ(jlist, "title", title ? title: "");
	json_set_property_strZ(jlist, "ref", ref ? ref: "");
	json_set_property_strZ(jlist, "ip", ip ? ip: "");
	json_set_property_objZ(jlist, "from", get_json_object_user(user));
	//json_set_property_objZ(jlist, "to", get_json_object(auser));
	json_set_property_strZ(jlist, "to_uin", aname);
	jcopy = json_item_copy(jlist, NULL);
	
	newraw = forge_raw(RAW_LCSDATA, jlist);
	if (auser) {
		post_raw(newraw, auser, g_ape);
	}
	POSTRAW_DONE(newraw);
	
	/*
	 * push raw history
	 */
	if (!strcmp(action, "join")) {
		type = MSG_TYPE_JOIN;
	} else if (!strcmp(action, "visit")) {
		type = MSG_TYPE_VISIT;
	} else if (!strcmp(action, "left")) {
		type = MSG_TYPE_LEFT;
	}
	newraw = forge_raw("RAW_RECENTLY", jcopy);
	lcs_set_msg(g_ape, newraw->data, from, aname, type);
	POSTRAW_DONE(newraw);

	if (!auser) {
		appBar *c = lcs_app_bar(g_ape, aname);
		if (!c) {
			c = abar_new();
			hashtbl_append(GET_ABAR_TBL(g_ape), aname, c);
		}
		if (c && queue_find(c->dirtyusers, from, hn_str_cmp) == -1) {
			queue_push_head(c->dirtyusers, strdup(from));
		}
	}
}

static void tick_static(acetables *g_ape, int lastcall)
{
	HTBL *etbl = GET_EVENT_TBL(g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "rawdb");
	if (!evt) return;
	
    stLcs *st = GET_LCS_STAT(g_ape);
    char sql[1024];

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

	MEVENT_TRIGGER_VOID(evt, NULL, REQ_CMD_STAT, FLAGS_NONE);
}

/*
 * pro range
 */
static unsigned int lcs_join(callbackp *callbacki)
{
	char *uname, *aname, *secy;
	int utime;
	appBar *abar;
	
	USERS *user = callbacki->call_user;
	CHANNEL *chan;
	HDF *apphdf = NULL;
	int olnum = 0, errcode = 0, ret;
	stLcs *st = GET_LCS_STAT(callbacki->g_ape);

	char *oname = NULL, *url, *title, *ref = NULL;

	bool oname_needfree = true;

	struct _http_header_line *hl = callbacki->hlines;
	while (hl) {
		if (!strcasecmp(hl->key.val, "refer")) {
			ref = hl->value.val;
			break;
		}
		hl = hl->next;
	}
	JNEED_STR(callbacki->param, "aname", aname, RETURN_BAD_PARAMS);
	JNEED_INT(callbacki->param, "utime", utime, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "url", url, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "title", title, RETURN_BAD_PARAMS);
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

	abar = lcs_app_bar(callbacki->g_ape, aname);
	if (abar) {
		olnum = queue_length(abar->users);
	}

	secy = lcs_app_secy(callbacki->g_ape, aname);
	
	apphdf = lcs_app_info(callbacki->g_ape, aname);
	if (!apphdf) {
		alog_warn("%s info failure", aname);
		errcode = 111;
		goto done;
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
	 * get user joined channel last time, and try to join again
	 */
	oname = lcs_get_admin(callbacki->g_ape, uname, aname);
	if (oname) {
		chan = getchanf(callbacki->g_ape, LCS_PIP_NAME"%s", oname);
		/*
		 * no admins on, join last admin's channel
		 */
		if (!chan && abar && queue_length(abar->admins) <= 0) {
			chan = mkchanf(callbacki->g_ape, CHANNEL_AUTODESTROY,
						   LCS_PIP_NAME"%s", oname);
			ADD_ANAME_FOR_CHANNEL(chan, oname);
			ADD_PNAME_FOR_CHANNEL(chan, aname);
		}
		if (chan) {
			join(user, chan, callbacki->g_ape);
			goto done;
		}
	}
		
	/*
	 * last joined cahnnel donot open, join another
	 */
	chan = lcs_app_get_adminchan(callbacki, aname, secy);
	SFREE(secy);
	if (chan) {
		join(user, chan, callbacki->g_ape);
		SFREE(oname);
		oname = GET_ANAME_FROM_CHANNEL(chan);
		oname_needfree = false;
	} else {
		/* no admin on, and make aname_channel failure */
		errcode = 7;
	}
	
	/*
	 * user joined my site.
	 */
	lcs_add_track(callbacki->g_ape, aname, uname, oname,
				  (char*)callbacki->ip, url, title, ref, TYPE_JOIN);

done:
	if (errcode != 0) {
		hn_senderr_sub(callbacki, errcode, "ERR_APP_NPASS");
	} else {
		/*
		 * add user to chatlist
		 * keep it track with oname, or aname when oname NULL
		 */
		if ( (utime == 1 &&
			  !(hdf_get_int_value(apphdf, "tune", 0) & LCS_TUNE_QUIET)) ||
			 utime == 2) {
			lcs_remember_user(callbacki, uname, oname ? oname: aname);
		}
	}

	lcs_user_action_notice(callbacki->g_ape, callbacki->call_user,
						   oname ? oname: aname,
						   "join", url, title, ref, (char*)callbacki->ip);

	if (oname_needfree) SFREE(oname);

	
	return (RETURN_NOTHING);
}

static unsigned int lcs_visit(callbackp *callbacki)
{
	char *aname, *pname, *url, *title;
	char *uname = GET_UIN_FROM_USER(callbacki->call_user);
	
	JNEED_STR(callbacki->param, "aname", aname, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "pname", pname, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "url", url, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "title", title, RETURN_BAD_PARAMS);
	
	lcs_add_track(callbacki->g_ape, pname, uname, NULL,
				  (char*)callbacki->ip, url, title, NULL, TYPE_VISIT);
	
	lcs_user_action_notice(callbacki->g_ape, callbacki->call_user, aname,
						   "visit", url, title, NULL, (char*)callbacki->ip);
	
	return (RETURN_NOTHING);
}

static unsigned int lcs_send(callbackp *callbacki)
{
	char *pipe, *msg, *uname = NULL;
	USERS *user = callbacki->call_user;;
	json_item *jlist;
	RAW *newraw;
	char *from = GET_UIN_FROM_USER(user);

	JNEED_STR(callbacki->param, "pipe", pipe, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "msg", msg, RETURN_BAD_PARAMS);

	jlist = json_new_object();
	json_set_property_strZ(jlist, "msg", msg);
	json_set_property_strZ(jlist, "type", "send");
	
	transpipe *spipe = get_pipe(pipe, callbacki->g_ape);
	if (spipe && spipe->type == USER_PIPE) {
		user = (USERS*)(spipe->pipe);
		if (user) uname = GET_UIN_FROM_USER(user);
		if (uname) {
			jlist = post_to_pipe(jlist, RAW_LCSDATA, pipe,
								 callbacki->call_subuser, callbacki->g_ape, true);
			
			newraw = forge_raw("RAW_RECENTLY", jlist);
			lcs_set_msg(callbacki->g_ape, newraw->data, from, uname, MSG_TYPE_SEND);
			POSTRAW_DONE(newraw);
		} else {
			alog_err("get uname failure");
			hn_senderr_sub(callbacki, 120, "ERR_UNAME_NEXIST");
		}
	} else if (spipe && spipe->type == CHANNEL_PIPE) {
		CHANNEL *chan = (CHANNEL*)spipe->pipe;
		char *uname = GET_ANAME_FROM_CHANNEL(chan);
		if (uname && !strcmp(uname, from)) {
			jlist = post_to_pipe(jlist, RAW_LCSDATA, pipe,
								 callbacki->call_subuser, callbacki->g_ape, true);
			
			newraw = forge_raw("RAW_RECENTLY", jlist);
			lcs_set_msg(callbacki->g_ape, newraw->data, from, uname, MSG_TYPE_SEND);
			POSTRAW_DONE(newraw);
		} else {
			alog_warn("%s wan't talk to %s", from, uname);
			hn_senderr_sub(callbacki, 122, "ERR_NOT_OWNER");
		}
	} else {
		alog_err("get pipe failure");
		hn_senderr_sub(callbacki, 121, "ERR_PIPE_ERROR");
	}

	return (RETURN_NOTHING);
}

static unsigned int lcs_msg(callbackp *callbacki)
{
	char *uname, *msg;
	json_item *jlist;
	RAW *newraw;
	USERS *user = callbacki->call_user;
	char *from = GET_UIN_FROM_USER(user);

	JNEED_STR(callbacki->param, "uname", uname, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "msg", msg, RETURN_BAD_PARAMS);

	jlist = json_new_object();
	json_set_property_strZ(jlist, "type", "msg");
	json_set_property_strZ(jlist, "msg", msg);
	json_set_property_objZ(jlist, "from", get_json_object_user(user));
	json_set_property_strZ(jlist, "to_uin", uname);
	newraw = forge_raw("RAW_RECENTLY", jlist);

	lcs_set_msg(callbacki->g_ape, newraw->data, from, uname, MSG_TYPE_OFFLINE_MSG);
	POSTRAW_DONE(newraw);

	appBar *c = lcs_app_bar(callbacki->g_ape, uname);
	if (!c) {
		c = abar_new();
		hashtbl_append(GET_ABAR_TBL(callbacki->g_ape), uname, c);
	}
	if (c && queue_find(c->dirtyusers, from, hn_str_cmp) == -1)
		queue_push_head(c->dirtyusers, strdup(from));
	
	return (RETURN_NOTHING);
}

static unsigned int lcs_joinb(callbackp *callbacki)
{
	char *aname, *masn;
	
	USERS *user = callbacki->call_user;
	CHANNEL *chan;
	int olnum = 0, errcode = 0, ret;
	stLcs *st = GET_LCS_STAT(callbacki->g_ape);

	JNEED_STR(callbacki->param, "aname", aname, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "masn", masn, RETURN_BAD_PARAMS);
	neos_unescape((UINT8*)aname, strlen(aname), '%');

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
	HDF *apphdf = lcs_app_info(callbacki->g_ape, aname);
	if (!apphdf) {
		alog_warn("%s info failure", aname);
		hn_senderr_sub(callbacki, 210, "ERR_APP_INFO");
		hdf_destroy(&apphdf);
		return (RETURN_NOTHING);
	}
	
	if (strcmp(hdf_get_value(apphdf, "masn", "NULL"), masn)) {
		alog_warn("%s attemp illgal login", aname);
		hn_senderr_sub(callbacki, 211, "ERR_APP_LOGIN");
		hdf_destroy(&apphdf);
		return (RETURN_NOTHING);
	}

	chan = getchanf(callbacki->g_ape, LCS_PIP_NAME"%s", aname);
	if (!chan) {
		chan = mkchanf(callbacki->g_ape,
					   CHANNEL_AUTODESTROY, LCS_PIP_NAME"%s", aname);
		if (!chan) {
			alog_err("make channel %s failure", aname);
			hn_senderr_sub(callbacki, 7, "ERR_MAKE_CHANNEL");
			hdf_destroy(&apphdf);
			return (RETURN_NOTHING);
		}
		char *pname = hdf_get_value(apphdf, "pname", aname);
		ADD_ANAME_FOR_CHANNEL(chan, aname);
		ADD_PNAME_FOR_CHANNEL(chan, pname);
	}
	if (isonchannel(user, chan)) {
		goto done;
	}
	
	appBar *abar = lcs_app_bar(callbacki->g_ape, aname);
	if (abar) {
		olnum = queue_length(abar->users);
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
		hn_senderr_sub(callbacki, errcode, "ERR_APP_NPASS");
	}
	
	hdf_destroy(&apphdf);
	return (RETURN_NOTHING);
}

static unsigned int lcs_dearusers(callbackp *callbacki)
{
	char *uname = GET_UIN_FROM_USER(callbacki->call_user);
	json_item *jdear, *jdlist = json_new_array();

	appBar *c = lcs_app_bar(callbacki->g_ape, uname);
	if (c && !queue_is_empty(c->dirtyusers)) {
		QueueEntry *qe;
		queue_iterate(c->dirtyusers, qe) {
			uname = (char*)qe->data;
			jdear = json_new_object();
			json_set_property_intZ(jdear, uname, 1);
			json_set_element_obj(jdlist, jdear);
		}
		/* reset dirty users. we don't use lcs_recently anymore. */
		queue_destroy(c->dirtyusers);
		c->dirtyusers = queue_new(0, free);
	}
	
	RAW *newraw = forge_raw("LCS_DEARUSERS", jdlist);
	post_raw_sub(newraw, callbacki->call_subuser, callbacki->g_ape);
	POSTRAW_DONE(newraw);

	return (RETURN_NOTHING);
}

static unsigned int lcs_recently(callbackp *callbacki)
{
	char *uname = GET_UIN_FROM_USER(callbacki->call_user);
	char *otheruin = NULL;

	otheruin = JGET_STR(callbacki->param, "uin");
	
	appBar *c = lcs_app_bar(callbacki->g_ape, uname);
	if (c && otheruin) {
		queue_remove_entry(c->dirtyusers, otheruin, hn_str_cmp);
	}

	return cmd_raw_recently(callbacki);
}

static int lcs_event_onjoin(USERS *user, CHANNEL *chan, acetables *g_ape)
{
	if (!user || !chan) return RET_PLUGIN_CONTINUE;

	if (strncasecmp(chan->name, LCS_PIP_NAME, strlen(LCS_PIP_NAME)))
		return RET_PLUGIN_CONTINUE;

	char *pname = GET_PNAME_FROM_CHANNEL(chan);
	char *uname = GET_UIN_FROM_USER(user);

	bool admin = false;
	if (GET_USER_ADMIN(user)) admin = true;
	
	lcs_app_onjoin(g_ape, pname, uname, admin, chan);

	return RET_PLUGIN_CONTINUE;
}

static int lcs_event_onleft(USERS *user, CHANNEL *chan, acetables *g_ape)
{
	if (!user || !chan) return RET_PLUGIN_CONTINUE;

	if (strncasecmp(chan->name, LCS_PIP_NAME, strlen(LCS_PIP_NAME)))
		return RET_PLUGIN_CONTINUE;

	char *pname = GET_PNAME_FROM_CHANNEL(chan);
	char *aname = GET_ANAME_FROM_CHANNEL(chan);
	char *uname = GET_UIN_FROM_USER(user);

	bool admin = false;
	if (GET_USER_ADMIN(user)) admin = true;
	
	lcs_app_onleft(g_ape, pname, uname, admin, chan);

	/*
	 * this can be done in userLeft JSF event
	 * but, for history raw, do this way.
	 */
	if (!admin) {
		lcs_user_action_notice(g_ape, user, aname, "left", NULL, NULL, NULL, NULL);
	}
	
	return RET_PLUGIN_CONTINUE;
}

static void init_module(acetables *g_ape)
{
	/*
	 * THESE tables MUST maked on aped start, see hnpub.h
	 */
	MAKE_USER_TBL(g_ape);		/* erased in deluser() */
	MAKE_ONLINE_TBL(g_ape);		/* erased in deluser() */
	MAKE_ABAR_TBL(g_ape);
	MAKE_LCS_STAT(g_ape, calloc(1, sizeof(stLcs)));
	lcs_event_init(g_ape);
    add_periodical((1000*60*30), 0, tick_static, g_ape, g_ape);
	
	register_cmd("LCS_JOIN", 		lcs_join, 		NEED_SESSID, g_ape);
	register_cmd("LCS_VISIT", 		lcs_visit, 		NEED_SESSID, g_ape);
	register_cmd("LCS_SEND", 		lcs_send, 		NEED_SESSID, g_ape);
	register_cmd("LCS_MSG", 		lcs_msg, 		NEED_SESSID, g_ape);
	register_cmd("LCS_DEARUSERS",	lcs_dearusers,	NEED_SESSID, g_ape);
	register_cmd("LCS_RECENTLY",	lcs_recently,	NEED_SESSID, g_ape);

	register_cmd("LCS_JOINB", 		lcs_joinb, 		NEED_SESSID, g_ape);
	//register_cmd("LCS_JOIN_A", 		lcs_join_a,		NEED_SESSID, g_ape);
}

static void free_module(acetables *g_ape)
{
	;
}

static ace_callbacks callbacks = {
	/* pre user event fired */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	
	/* pre channel event fired */
	NULL,
	NULL,
	NULL,
	lcs_event_onjoin,
	lcs_event_onleft,

	/* post user event hooked */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	/* post channel event hooked */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	/* tickuser */
	NULL
};

APE_INIT_PLUGIN(MODULE_NAME, init_module, free_module, callbacks)
