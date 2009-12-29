#include <mysql/mysql.h>
#include "plugins.h"
#include "global_plugins.h"
#include "libape-push.h"

#include "../src/hnpub.h"

/*
 * libape-push require mevent header and library for MEVENT(a non-block db backend):
 * http://code.google.com/p/cmoon/source/browse/trunk/event/
 */
#include "mevent.h"
#include "data.h"
#include "mevent_uic.h"
#include "mevent_db_community.h"

#define MODULE_NAME "push"

static ace_plugin_infos infos_module = {
	"\"Push\" system", // Module Name
	"1.0",			   // Module Version
	"hunantv",		   // Module Author
	"mod_push.conf"	   // config file (bin/)
};

/*
 * file range
 */
static void keep_up_with_my_friend(USERS *user, acetables *g_ape)
{
	HTBL_ITEM *item;
	HTBL *ulist = GET_USER_FRIEND_TBL(user);
	char *uin = GET_UIN_FROM_USER(user);
	USERS *friend;
	CHANNEL *chan, *jchan;

	chan = getchanf(g_ape, FRIEND_PIP_NAME"%s", uin);
	if (chan == NULL) {
		alog_err("%s baby, you didn't make a channel?", uin);
		return;
	}

	if (ulist != NULL)	{
		for (item = ulist->first; item != NULL; item = item->lnext) {
			/*
			 * invite my friends
			 */
			friend = GET_USER_FROM_APE(g_ape, item->key);
			if (friend != NULL) {
				alog_noise("%s invite %s\n", uin, item->key);
				join(friend, chan, g_ape);
			}

			/*
			 * join my friends
			 */
			jchan = getchanf(g_ape, FRIEND_PIP_NAME"%s", item->key);
			if (jchan != NULL) {
				join(user, jchan, g_ape);
			}
		}
	}
}

static void get_user_info(char *uin, USERS *user, acetables *g_ape)
{
	HTBL *ulist;
	extend *ext;

	mevent_t *evt;
	struct data_cell *pc, *cc;
	char val[64];

	int ret;

	if (!hn_isvaliduin(uin) || user == NULL || g_ape == NULL) return;

	evt = mevent_init_plugin("uic", REQ_CMD_FRIENDLIST, FLAGS_SYNC);
	mevent_add_u32(evt, NULL, "uin", atoi(uin));
	ret = mevent_trigger(evt);
	if (PROCESS_NOK(ret)) {
		alog_err("get friend for user %s failure %d", uin, ret);
		goto get_msgset;
	}

	pc = data_cell_search(evt->rcvdata, false, DATA_TYPE_ARRAY, "friend");
	if (pc != NULL) {
		ext = add_property(&user->properties, "friend", hashtbl_init(),
						   EXTEND_HTBL, EXTEND_ISPRIVATE);
		ulist = (HTBL*)ext->val;
		iterate_data(pc) {
			cc = pc->v.aval->items[t_rsv_i];

			if (cc->type != DATA_TYPE_U32) continue;
			sprintf(val, "%d", cc->v.ival);
			alog_noise("add %s friend %s", uin, cc->key);
			hashtbl_append(ulist, (char*)cc->key, NULL);
		}
	}

 get_msgset:
	mevent_chose_plugin(evt, "uic", REQ_CMD_MYSETTING, FLAGS_SYNC);
	mevent_add_u32(evt, NULL, "uin", atoi(uin));
	ret = mevent_trigger(evt);
	if (PROCESS_NOK(ret)) {
		alog_err("get setting for user %s failure %d", uin, ret);
		goto done;
	}

	pc = data_cell_search(evt->rcvdata, false, DATA_TYPE_ARRAY, "incept");
	if (pc != NULL) {
		ext = add_property(&user->properties, "incept", hashtbl_init(),
						   EXTEND_HTBL, EXTEND_ISPRIVATE);
		ulist = (HTBL*)ext->val;

		iterate_data(pc) {
			cc = pc->v.aval->items[t_rsv_i];

			if (cc->type != DATA_TYPE_U32) continue;
			sprintf(val, "%d", cc->v.ival);
			alog_noise("add %s incpet %s", uin, val);
			hashtbl_append(ulist, (char*)cc->key, NULL);
		}
	}

	pc = data_cell_search(evt->rcvdata, false, DATA_TYPE_U32, "sitemessage");
	if (pc != NULL) {
		sprintf(val, "%d", pc->v.ival);
		alog_noise("add %s sitemessage %s", uin, val);
		add_property(&user->properties, "msgset", val, EXTEND_STR, EXTEND_ISPRIVATE);
	}

 done:
	mevent_free(evt);
	return;
}

static void tick_static(acetables *g_ape, int lastcall)
{
    HTBL *ulist = GET_USER_LIST(g_ape);
    st_push *st = GET_STAT_LIST(g_ape);
    HTBL_ITEM *item;
    USERS *user;
    char *uin;
    int num = 0, ret;
    char sql[1024], usrlist[512];

	if (ulist == NULL || st == NULL) {
		alog_err("list table null");
		return;
	}

    mevent_t *evt;
    evt = mevent_init_plugin("db_community", REQ_CMD_STAT, FLAGS_NONE);
    if (evt == NULL) {
        alog_err("init mevent db_community failure");
        return;
    }
    mevent_add_array(evt, NULL, "sqls");
    
    memset(sql, 0x0, sizeof(sql));
    memset(usrlist, 0x0, sizeof(usrlist));
	for (item = ulist->first; item != NULL; item = item->lnext) {
		user = (USERS*) item->addrs;
		uin = (char*)get_property(user->properties, "uin")->val;
		num++;
		strcat(usrlist, uin);
		strcat(usrlist, " ");
		if (num > 20) {
			break;
		}
	}
    snprintf(sql, sizeof(sql), "INSERT INTO mps (type, count, remark) "
             " VALUES (%d, %u, '%s');", ST_ONLINE, g_ape->nConnected, usrlist);
    mevent_add_str(evt, "sqls", "1", sql);

    sprintf(sql, "INSERT INTO mps (type, count) VALUES (%d, %lu);",
            ST_NOTICE, st->msg_notice);
    mevent_add_str(evt, "sqls", "2", sql);
    st->msg_notice = 0;

    sprintf(sql, "INSERT INTO mps (type, count) VALUES (%d, %lu);",
            ST_FEED, st->msg_feed);
    mevent_add_str(evt, "sqls", "3", sql);
    st->msg_feed = 0;

    sprintf(sql, "INSERT INTO mps (type, count) VALUES (%d, %lu);",
            ST_NUM_LOGIN, st->num_login);
    mevent_add_str(evt, "sqls", "4", sql);
    st->num_login = 0;

    sprintf(sql, "INSERT INTO mps (type, count) VALUES (%d, %lu);",
            ST_NUM_USER, st->num_user);
    mevent_add_str(evt, "sqls", "5", sql);
    st->num_user = 0;
	hashtbl_empty(GET_ONLINE_TBL(g_ape));

    ret = mevent_trigger(evt);
    if (PROCESS_NOK(ret)) {
        alog_err("trigger statistic event failure %d", ret);
    }
	mevent_free(evt);
}

/*
 * pro range
 */
static unsigned int push_connect(callbackp *callbacki)
{
	USERS *nuser;
	RAW *newraw;
	json_item *jlist = NULL;
	CHANNEL *chan;
	char *uin;
	st_push *st = GET_STAT_LIST(callbacki->g_ape);

	JNEED_STR(callbacki->param, "uin", uin, RETURN_BAD_PARAMS);

	if (!hn_isvaliduin(uin)) {
		return (RETURN_BAD_PARAMS);
	}

	if (GET_USER_FROM_APE(callbacki->g_ape, uin) != NULL) {
		if (atoi(READ_CONF("enable_user_reconnect")) == 1) {
			deluser(GET_USER_FROM_APE(callbacki->g_ape, uin), callbacki->g_ape);
		} else {
			alog_warn("user %s used", uin);
			hn_senderr(callbacki, "006", "ERR_UIN_USED");
			return (RETURN_NOTHING);
		}
	}
	
	nuser = adduser(NULL, NULL, NULL, callbacki->call_user, callbacki->g_ape);

	st->num_login++;
	
	USERS *tuser = GET_USER_FROM_ONLINE(callbacki->g_ape, uin);
	if (tuser == NULL) {
		st->num_user++;
		SET_USER_FOR_ONLINE(callbacki->g_ape, uin, nuser);
	}

	SET_UIN_FOR_USER(nuser, uin);
	SET_USER_FOR_APE(callbacki->g_ape, uin, nuser);
	get_user_info(uin, nuser, callbacki->g_ape);
	
	/*
	 * make own channel
	 */
	if ((chan = getchanf(callbacki->g_ape, FRIEND_PIP_NAME"%s", uin)) == NULL) {
		chan = mkchanf(callbacki->g_ape, CHANNEL_NONINTERACTIVE | CHANNEL_AUTODESTROY,
					   FRIEND_PIP_NAME"%s", uin);
		if (chan == NULL) {
			alog_err("make channel %s failure", uin);
			//smsalarm_msgf(callbacki->g_ape, "make channel %s failed", uin);
			hn_senderr(callbacki, "007", "ERR_MAKE_CHANNEL");
			return (RETURN_NOTHING);
		}
	}
	join(nuser, chan, callbacki->g_ape);
	keep_up_with_my_friend(nuser, callbacki->g_ape);

	callbacki->call_user = nuser;
	subuser_restor(callbacki->call_subuser, callbacki->g_ape);

	jlist = json_new_object();
	json_set_property_strZ(jlist, "sessid", nuser->sessid);
	newraw = forge_raw(RAW_LOGIN, jlist);
	newraw->priority = RAW_PRI_HI;
	post_raw(newraw, nuser, callbacki->g_ape);
	POSTRAW_DONE(newraw);

	return (RETURN_NOTHING);
}

static unsigned int push_send(callbackp *callbacki)
{
	json_item *jlist = NULL;
	RAW *newraw;
	USERS *user;
	CHANNEL *chan;
	char *uin;
	json_item *msg;

	user = callbacki->call_user;
	uin = GET_UIN_FROM_USER(user);
	JNEED_OBJ(callbacki->param, "msg", msg, RETURN_BAD_PARAMS);

	if (atoi(READ_CONF("refresh_setting_onmsg")) == 1) {
		/*
		 * refresh user's friends..., and join the new friend
		 */
		get_user_info(uin, user, callbacki->g_ape);
		keep_up_with_my_friend(user, callbacki->g_ape);
	}

	chan = getchanf(callbacki->g_ape, FRIEND_PIP_NAME"%s", uin);
	if (chan == NULL || chan->pipe == NULL ||
		chan->pipe->type != CHANNEL_PIPE) {
		alog_warn("channel %s %s not exist", FRIEND_PIP_NAME, uin);
		goto done;
	}

	jlist = json_new_object();
	json_item *jcopy = json_item_copy(msg->father, NULL);
	json_set_property_objZ(jlist, "msg", jcopy);
	json_set_property_objZ(jlist, "pipe", get_json_object_channel(chan));
	newraw = forge_raw(RAW_DATA, jlist);
	post_raw_channel_restricted(newraw, chan, user, callbacki->g_ape);
	POSTRAW_DONE(newraw);

	st_push *st = (st_push*)get_property(callbacki->g_ape->properties,
										 "msgstatic")->val;
	st->msg_feed++;

 done:
	jlist = json_new_object();
	json_set_property_strZ(jlist, "value", "null");
	RAW *raw = forge_raw("CLOSE", jlist);
	send_raw_inline(callbacki->client, callbacki->transport, raw, callbacki->g_ape);

	/* we need close the connection immediatly, for the reason of php sent them */
	return (RETURN_NULL);
}

static unsigned int push_regpageclass(callbackp *callbacki)
{
	subuser *sub = callbacki->call_subuser;
	if (sub == NULL) {
		alog_warn("get subuser failuer for %s", callbacki->host);
		hn_senderr(callbacki, "008", "ERR_REG_APP");
		return (RETURN_NOTHING);
	}

	char *apps;
	JNEED_STR(callbacki->param, "apps", apps, RETURN_BAD_PARAMS);

	extend *ext = get_property(sub->properties, "incept");
	if (ext == NULL) {
		ext = add_property(&sub->properties, "incept", hashtbl_init(),
						   EXTEND_HTBL, EXTEND_ISPRIVATE);
	}

	if (ext != NULL && ext->val != NULL) {
		char *incept = strdup(apps);
		char *ids[100];
		size_t num = explode('x', incept, ids, 99);
		int loopi;
		for (loopi = 0; loopi <= num; loopi++) {
			alog_noise("append %s for %s's %s",
					   ids[loopi], sub->user->sessid, sub->channel);
			hashtbl_append(ext->val, ids[loopi], NULL);
		}
		free(incept);

		json_item *jlist = json_new_object();
		json_item *class_list = json_new_array();
		HTBL_ITEM *item;
		HTBL *ulist = ext->val;
		for (item = ulist->first; item != NULL; item = item->lnext) {
			json_item *class = json_new_object();
			json_set_property_strZ(class, "id", item->key);
			json_set_element_obj(class_list, class);
		}
		json_set_property_objZ(jlist, "pageclass", class_list);

		RAW *newraw = forge_raw("REGCLASS", jlist);
		post_raw_sub(newraw, sub, callbacki->g_ape);
		POSTRAW_DONE(newraw);
	} else {
		alog_err("add property error");
		hn_senderr(callbacki, "008", "ERR_REG_APP");
	}

	return (RETURN_NOTHING);
}

static unsigned int push_userlist(callbackp *callbacki)
{
	RAW *newraw;
	json_item *jlist = json_new_object();
	HTBL_ITEM *item;
	HTBL *ulist = GET_USER_LIST(callbacki->g_ape);
	USERS *user;
	int num;

	json_item *user_list = json_new_array();
	json_set_property_objZ(jlist, "users", user_list);

	num = 0;
	if (ulist != NULL) {
		for (item = ulist->first; item != NULL; item = item->lnext) {
			if (num < 20) {
				user = (USERS*)item->addrs;
				json_set_element_obj(user_list, get_json_object_user(user));
			}
			num++;
		}
		json_set_property_intZ(jlist, "num", num);
	}

	newraw = forge_raw("USERLIST", jlist);
	post_raw(newraw, callbacki->call_user, callbacki->g_ape);
	POSTRAW_DONE(newraw);

	return (RETURN_NOTHING);
}

static unsigned int push_friendlist(callbackp *callbacki)
{
	HTBL_ITEM *item;
	HTBL *ulist = GET_USER_FRIEND_TBL(callbacki->call_user);

	RAW *newraw;
	json_item *jlist = json_new_object();
	USERS *friend;

	json_item *frd_list = json_new_array();
	json_set_property_objZ(jlist, "friends", frd_list);
	if (ulist != NULL) {
		for (item = ulist->first; item != NULL; item = item->lnext) {
			friend = GET_USER_FROM_APE(callbacki->g_ape, item->key);
			if (friend != NULL) {
				json_set_element_obj(frd_list, get_json_object_user(friend));
			}
		}
	}
	newraw = forge_raw("FRIENDLIST", jlist);
	post_raw(newraw, callbacki->call_user, callbacki->g_ape);
	POSTRAW_DONE(newraw);

	return (RETURN_NOTHING);
}

static unsigned int push_useronline(callbackp *callbacki)
{
	RAW *newraw;
	json_item *jlist = json_new_object();
	char *users;
	char *uin[MAX_CLIENT_PERPUSH];

	JNEED_STR_COPY(callbacki->param, "users", users, RETURN_BAD_PARAMS);
	size_t num = explode(',', users, uin, MAX_CLIENT_PERPUSH-1);

	int loopi;
	for (loopi = 0; loopi <= num; loopi++) {
		if (hn_isvaliduin(uin[loopi])) {
			if (GET_USER_FROM_APE(callbacki->g_ape, uin[loopi])) {
				json_set_property_strZ(jlist, uin[loopi], "1");
			} else {
				json_set_property_strZ(jlist, uin[loopi], "0");
			}
		}
	}

	free(users);

	newraw = forge_raw(RAW_DATA, jlist);
	post_raw(newraw, callbacki->call_user, callbacki->g_ape);
	POSTRAW_DONE(newraw);

	return (RETURN_NULL);
}

static unsigned int push_senduniq(callbackp *callbacki)
{
	char *dstuin;
	json_item *msg;

	JNEED_STR(callbacki->param, "dstuin", dstuin, RETURN_BAD_PARAMS);
	JNEED_OBJ(callbacki->param, "msg", msg, RETURN_BAD_PARAMS);

	USERS *user = GET_USER_FROM_APE(callbacki->g_ape, dstuin);

	if (user == NULL) {
		alog_noise("get user failuer %s", dstuin);
		hn_senderr(callbacki, "009", "ERR_UIN_NEXIST");
		return (RETURN_NULL);
	}

	st_push *st = (st_push*)get_property(callbacki->g_ape->properties,
										 "msgstatic")->val;
	st->msg_notice++;
	/*
	 * target user may set 1: accept 2: reject 3: accept friend
	 * senduniq must contain pageclass: 1
	 */
	json_item *it = json_lookup(msg, "pageclass");
	extend *ext = get_property(user->properties, "msgset");
	if (it != NULL && it->jval.vu.integer_value == 1 &&
		ext != NULL && ext->val != NULL) {
		if (!strcmp(ext->val, "2")) {
			alog_info("%s reject message", dstuin);
			hn_senderr(callbacki, "010", "ERR_USER_REFUSE");
			return (RETURN_NULL);
		} else if (!strcmp(ext->val, "3")) {
			alog_info("user %s just rcv friend's message", dstuin);
			char *sender = GET_UIN_FROM_USER(callbacki->call_user);
			if (sender != NULL) {
				HTBL *list = GET_USER_FRIEND_TBL(user);
				if (list != NULL) {
					if (hashtbl_seek(list, sender) == NULL) {
						hn_senderr(callbacki, "012", "ERR_NOT_FRIEND");
						return (RETURN_NULL);
					}
				} else {
					hn_senderr(callbacki, "012", "ERR_NOT_FRIEND");
					return (RETURN_NULL);
				}
			} else {
				hn_senderr(callbacki, "009", "ERR_UIN_NEXIST");
				return (RETURN_NULL);
			}
			alog_warn("user %s not exist", dstuin);
		}
	}

	json_item *jlist = json_new_object();
	/* TODO why should i copy msg->father rather than msg????? */
	json_item *jcopy = json_item_copy(msg->father, NULL);
	RAW *newraw;
	json_set_property_objZ(jlist, "msg", jcopy);
	newraw = forge_raw(RAW_DATA, jlist);
	post_raw(newraw, user, callbacki->g_ape);
	POSTRAW_DONE(newraw);

	hn_senddata(callbacki, "999", "OPERATION_SUCESS");
	
	return (RETURN_NULL);
}

static unsigned int push_sendmulti(callbackp *callbacki)
{
	char *pass, *uins, *sender;
	json_item *msg;
	USERS *dst;

	st_push *st = (st_push*)get_property(callbacki->g_ape->properties,
										 "msgstatic")->val;
	sender = GET_UIN_FROM_USER(callbacki->call_user);
	
	JNEED_STR(callbacki->param, "pass", pass, RETURN_BAD_PARAMS);
	JNEED_OBJ(callbacki->param, "msg", msg, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "uins", uins, RETURN_BAD_PARAMS);

	alog_foo("%s %s SEND %s TO %s", callbacki->ip, sender,
			 json_to_string(msg, NULL, 0)->jstring, uins);
	
	if (strcmp(pass, READ_CONF("push_multi_pass"))) {
		hn_senderr(callbacki, "020", "ERR_REFUSE");
		return (RETURN_NULL);
	}

	json_item *jlist = json_new_object();
	json_set_property_objZ(jlist, "msg", json_item_copy(msg->father, NULL));
	RAW *newraw = forge_raw(RAW_DATA, jlist);

	if (!strcmp(uins, "ALL_ManGos")) {
		HTBL *ulist = GET_USER_LIST(callbacki->g_ape);
		HTBL_ITEM *item;
		for (item = ulist->first; item != NULL; item = item->lnext) {
			st->msg_notice++;
			dst = (USERS*)item->addrs;
			post_raw(newraw, dst, callbacki->g_ape);
		}
	} else {
		char *uin[MAX_CLIENT_PERPUSH];
		size_t num = explode(',', uins, uin, MAX_CLIENT_PERPUSH-1);
		int loopi;
		for (loopi = 0; loopi <= num; loopi++) {
			dst = GET_USER_FROM_APE(callbacki->g_ape, uin[loopi]);
			if (dst != NULL) {
				post_raw(newraw, dst, callbacki->g_ape);
				st->msg_notice++;
			}
		}
	}
	POSTRAW_DONE(newraw);

	hn_senddata(callbacki, "999", "OPERATION_SUCESS");
	return (RETURN_NULL);
}

static void push_deluser(USERS *user, int istmp, acetables *g_ape)
{
	CHANNEL *chan;
	char *uin = GET_UIN_FROM_USER(user);

	chan = getchanf(g_ape, FRIEND_PIP_NAME"%s", uin);
	if (chan != NULL) {
		HTBL *ulist = GET_USER_FRIEND_TBL(user);
		HTBL_ITEM *item;
		USERS *friend;
		RAW *newraw;
		json_item *jlist;
		if (ulist != NULL) {
			for (item = ulist->first; item != NULL; item = item->lnext) {
				friend = GET_USER_FROM_APE(g_ape, item->key);
				if (friend != NULL) {
					jlist = json_new_object();
					json_set_property_objZ(jlist, "user", get_json_object_user(user));
					newraw = forge_raw(RAW_LEFT, jlist);
					post_raw(newraw, friend, g_ape);
					POSTRAW_DONE(newraw);

					left(friend, chan, g_ape);
				}
			}
		}
	}

	if (uin != NULL && get_property(g_ape->properties, "userlist") != NULL) {
		hashtbl_erase(get_property(g_ape->properties, "userlist")->val, uin);
	}

	/* left_all done by cmd.c */
	//left_all(user, g_ape);
	
	/* kill all users connections */
	clear_subusers(user, g_ape);
	hashtbl_erase(g_ape->hSessid, user->sessid);
	g_ape->nConnected--;

	if (user->prev == NULL) {
		g_ape->uHead = user->next;
	} else {
		user->prev->next = user->next;
	}
	if (user->next != NULL) {
		user->next->prev = user->prev;
	}

	clear_sessions(user);
	clear_properties(&user->properties);
	destroy_pipe(user->pipe, g_ape);

	free(user);
}

static void push_ticksubuser(subuser *sub, acetables *g_ape)
{
	alog_noise("%s %s", GET_UIN_FROM_USER(sub->user), sub->channel);
}

static void push_post_raw_sub(RAW *raw, subuser *sub, acetables *g_ape)
{
	extend *usrp = get_property(sub->user->properties, "incept");
	extend *subp = get_property(sub->properties, "incept");
	extend *msgp = get_property(sub->user->properties, "msgset");

	int post;

	HTBL *list;
	HTBL_ITEM *item;
	int add_size = 16;
	struct _raw_pool_user *pool;

	alog_noise("prepare post %s", raw->data);

	/*
	 * if not a DATA message, post anyway
	 */
	json_item *oit, *it;
	char *rawtype;

	oit = it = NULL;
	oit = init_json_parser(raw->data);
	if (oit != NULL) it = oit->jchild.child;
	if (it == NULL) {
		alog_err("%s not json format", raw->data);
		return;
	}
	JNEED_STR(it, "raw", rawtype, (void)0);

	if (strcmp(rawtype, "DATA"))
		goto do_post;

	/*
	 * if not a feed message, just check pageclass
	 */
	int pageclass;
	JNEED_INT(it, "data.msg.pageclass", pageclass, (void)0);
	if (pageclass != 3)
		goto judge_class;


	/*
	 * check type
	 */
	int type;
	JNEED_INT(it, "data.msg.type", type, (void)0);
	if (usrp != NULL && usrp->val != NULL) {
		post = 0;
		list = usrp->val;
		for (item = list->first; item != NULL; item = item->lnext) {
			if (type == atoi(item->key)) {
				post = 1;
				break;
			}
		}
		if (post == 0) {
			alog_dbg("data's type %d not in my setting", type);
			free_json_item(oit);
			return;
		}
	}

 judge_class:
	/*
	 * check msgset
	 */
	if (msgp != NULL && msgp->val != NULL) {
		if (pageclass == 1) {
			if (!strcmp(msgp->val, "2")) {
				alog_dbg("user set don't accept pageclass=1");
				free_json_item(oit);
				return;
			}
		}
	}

	/*
	 * check pageclass
	 */
	if (subp != NULL && subp->val != NULL) {
		post = 0;
		list = subp->val;
		for (item = list->first; item != NULL; item = item->lnext) {
			if (pageclass == atoi(item->key)) {
				post = 1;
				break;
			}
		}
		if (post == 0) {
			alog_dbg("pageclass %d not in my properties(sub)", pageclass);
			free_json_item(oit);
			return;
		}
	}

 do_post:
	free_json_item(oit);
	pool = (raw->priority == RAW_PRI_LO ?
			&sub->raw_pools.low : &sub->raw_pools.high);

	if (++pool->nraw == pool->size) {
		pool->size += add_size;
		expend_raw_pool(pool->rawfoot, add_size);
	}

	pool->rawfoot->raw = raw;
	pool->rawfoot = pool->rawfoot->next;

	(sub->raw_pools.nraw)++;
	(raw->refcount)++;
	alog_noise("%s done", raw->data);
}

static unsigned int push_trustsend(callbackp *callbacki)
{
	char *dstuin, *msg;

	JNEED_STR(callbacki->param, "dstuin", dstuin, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "msg", msg, RETURN_BAD_PARAMS);

	USERS *user = GET_USER_FROM_APE(callbacki->g_ape, dstuin);

	if (user == NULL) {
		hn_senderr(callbacki, "009", "ERR_UIN_NEXIST");
		return (RETURN_NULL);
	}

	RAW *newraw;
	json_item *jlist = json_new_object();

	json_set_property_strZ(jlist, "msg", msg);
	newraw = forge_raw(RAW_DATA, jlist);
	post_raw(newraw, user, callbacki->g_ape);
	POSTRAW_DONE(newraw);

	json_item *ej = json_new_object();
	json_set_property_strZ(ej, "code", "999");
	json_set_property_strZ(ej, "value", "OPERATION_SUCESS");
	newraw = forge_raw(RAW_DATA, ej);
	send_raw_inline(callbacki->client, callbacki->transport,
					newraw, callbacki->g_ape);

	return (RETURN_NULL);
}

static void init_module(acetables *g_ape)
{
	st_push *stdata = calloc(1, sizeof(st_push));
	add_property(&g_ape->properties, "userlist", hashtbl_init(),
				 EXTEND_HTBL, EXTEND_ISPRIVATE);
	add_property(&g_ape->properties, "msgstatic", stdata,
				 EXTEND_HTBL, EXTEND_ISPRIVATE);
	add_property(&g_ape->properties, "onlineuser", hashtbl_init(),
				 EXTEND_HTBL, EXTEND_ISPRIVATE);
	
    add_periodical((1000*60*30), 0, tick_static, g_ape, g_ape);
	
	register_cmd("CONNECT",	push_connect, NEED_NOTHING, g_ape);
	register_cmd("SEND", push_send, NEED_SESSID, g_ape);
	register_cmd("REGCLASS", push_regpageclass, NEED_SESSID, g_ape);
	register_cmd("USERLIST", push_userlist, NEED_SESSID, g_ape);
	register_cmd("FRIENDLIST", push_friendlist, NEED_SESSID, g_ape);
	register_cmd("USERONLINE", push_useronline, NEED_SESSID, g_ape);
	register_cmd("SENDUNIQ", push_senduniq, NEED_SESSID, g_ape);
	register_cmd("SENDMULTI", push_sendmulti, NEED_SESSID, g_ape);
	//register_cmd("TRUSTSEND", push_trustsend, NEED_NOTHING, g_ape);
}

static ace_callbacks callbacks = {
	NULL,				/* Called when new user is added */
	push_deluser,		/* Called when a user is disconnected */
	NULL,				/* Called when new chan is created */
	NULL,				/* Called when a chan removed */
	NULL,				/* Called when a user join a channel */
	NULL,				/* Called when a user leave a channel */
	NULL,				/* Called at each tick, passing a subuser */
	push_post_raw_sub,	/* Called when a subuser receiv a message */
	NULL,				/* Called when a user allocated */
	NULL,				/* Called when a subuser is added */
	NULL				/* Called when a subuser is disconnected */
};

APE_INIT_PLUGIN(MODULE_NAME, init_module, callbacks)
