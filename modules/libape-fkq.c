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

static int add_visitnum(acetables *g_ape, char *uin, char *huin, int *cnum)
{
	if (!g_ape || !uin || !huin) return 0;

	HTBL *table = GET_VISITNUM_TBL(g_ape);
	if (!table) return 0;

	int ret = 0;
	Queue *queue = hashtbl_seek(table, huin);
	if (!queue) {
		queue = queue_new(0, free);
		hashtbl_append(table, huin, queue);
	}

	if (queue_find(queue, uin, hn_str_cmp) == -1) {
		queue_push_head(queue, strdup(uin));
		ret = 1;
	}
	
	*cnum = queue_length(queue);

	return ret;
}

static int del_visitnum(acetables *g_ape, char *uin, char *huin, int *cnum)
{
	if (!g_ape || !uin || !huin) return 0;

	HTBL *table = GET_VISITNUM_TBL(g_ape);
	if (!table) return 0;

	int ret = 0;
	Queue *queue = hashtbl_seek(table, huin);
	if (!queue) {
		queue = queue_new(0, free);
		hashtbl_append(table, huin, queue);
	}

	ret = queue_remove_entry(queue, uin, hn_str_cmp);

	*cnum = queue_length(queue);

	return ret;
}

static void notice_channel_visitnum(char *huin, int changenum,
									unsigned int currentnum, acetables *g_ape)
{
	json_item *jlist;
	RAW *newraw;
	CHANNEL *chan;

	chan = getchanf(g_ape, FKQ_PIP_NAME"%s", huin);
	if (chan) {
		jlist = json_new_object();

		json_set_property_intZ(jlist, "change", changenum);
		json_set_property_intZ(jlist, "current", currentnum);
		json_set_property_strZ(jlist, "host", huin);
	
		newraw = forge_raw("FKQ_VISITCHANGE", jlist);
		post_raw_channel(newraw, chan, g_ape);
		POSTRAW_DONE(newraw);
	}
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

static void channel_onjoin(acetables *g_ape, char *huin, int chatnum)
{
	HTBL *table = GET_CHATNUM_TBL(g_ape);
	if (!table) return;

	chatNum *cnum = (chatNum*)hashtbl_seek(table, huin);
	if (cnum) {
		if (cnum->fkq < chatnum) cnum->fkq = chatnum;
	} else {
		cnum = chatnum_new(0, chatnum);
		hashtbl_append(table, huin, cnum);
	}
}

static void tick_static(acetables *g_ape, int lastcall)
{
	st_fkq *st = GET_FKQ_STAT(g_ape);
	HTBL *table = GET_CHATNUM_TBL(g_ape);
	HTBL_ITEM *item;
	ListEntry *list = NULL, *node = NULL;
	chatNum *cnum;
	
	int ret, count, repcount;
    char sql[1024], tok[64];
	
    mevent_t *evt;
    evt = mevent_init_plugin("db_community", REQ_CMD_STAT, FLAGS_NONE);
    if (evt == NULL) {
        alog_err("init mevent db_community failure");
        return;
    }
    mevent_add_array(evt, NULL, "sqls");
	
	count = 0;
	for (item = table->first; item != NULL; item = item->lnext) {
		list_prepend(&list, item);
		count++;
	}

	/*
	 * report count
	 */
	snprintf(sql, sizeof(sql), "INSERT INTO mps (type, count) "
			 " VALUES (%d, %lu)", ST_FKQ_MSG_TOTAL, st->msg_total);
	mevent_add_str(evt, "sqls", "0", sql);
	st->msg_total = 0;

	snprintf(sql, sizeof(sql), "INSERT INTO mps (type, count) "
			 " VALUES (%d, %d)", ST_FKQ_ALIVE_GROUP, count);
	mevent_add_str(evt, "sqls", "1", sql);

	/*
	 * report fkq
	 */
	list_sort(&list, hn_chatnum_fkq_cmp);
	repcount = 2;
	node = list;
	while (node != NULL && repcount < 22) {
		item = (HTBL_ITEM*)list_data(node);
		cnum = (chatNum*)item->addrs;
		
		sprintf(tok, "%d", repcount++);
		snprintf(sql, sizeof(sql), "INSERT INTO fkq (userid, type, chatnum, "
				 " visitnum) VALUES (%s, 2, %d, %s)",
				 (char*)item->key, cnum->fkq, "0");
		mevent_add_str(evt, "sqls", tok, sql);
		
		node = list_next(node);
	}
	
	/*
	 * report friend
	 */
	list_sort(&list, hn_chatnum_friend_cmp);
	node = list;
	while (node != NULL && repcount < 42) {
		item = (HTBL_ITEM*)list_data(node);
		cnum = (chatNum*)item->addrs;
		
		sprintf(tok, "%d", repcount++);
		snprintf(sql, sizeof(sql), "INSERT INTO fkq (userid, type, chatnum, "
				 " visitnum) VALUES (%s, 1, %d, %s)",
				 (char*)item->key, cnum->friend, "0");
		mevent_add_str(evt, "sqls", tok, sql);
		
		node = list_next(node);
	}
	
	clist_free(list);
	hashtbl_empty(table, chatnum_free);

    ret = mevent_trigger(evt);
    if (PROCESS_NOK(ret)) {
        alog_err("trigger statistic event failure %d", ret);
    }
	mevent_free(evt);
}

/*
 * pro range
 */

/*
 *input:
 *  {"cmd":"FKQ_INIT", "chl":x, "sessid":"", "params":{"nick":"", "hostUin":""}}
 *    nick: 访客用户昵称
 *    hostUin: 访客群主号码
 *output:
 *  {"raw":"FKQ_INIT", "data":
 {"hostUin": "1798031", "fkqstat":1, "hostfkqlevel":0, "hostfkinfo":{"hostchatnum":0, "hostvisitnum":10}}
 }
 *    fkqstat: 访问者访客群聊天功能状态. 0: 开启, 1: 关闭
 *    hostfkqlevel: 访客群主访客群等级. 0 表示 hostUin 为没有访客群功能的普通用户
 *    hostfkinfo: 访客群群内信息, 当前包含聊天人数, 浏览人数
 */
static unsigned int fkq_init(callbackp *callbacki)
{
	char *uin, *nick, *hostUin;
	USERS *nuser;
	subuser *sub;
	st_fkq *st = GET_FKQ_STAT(callbacki->g_ape);
	json_item *jlist = NULL;
	RAW *newraw;

	nuser = callbacki->call_user;
	sub = callbacki->call_subuser;

	uin = GET_UIN_FROM_USER(nuser);
	
	JNEED_STR(callbacki->param, "nick", nick, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "hostUin", hostUin, RETURN_BAD_PARAMS);

	if (!hn_isvaliduin(uin) || !hn_isvaliduin(hostUin)) {
		return (RETURN_BAD_PARAMS);
	}

	ADD_NICK_FOR_USER(nuser, nick);
	
	int stat, level, chatnum, visitnum;
	stat = get_fkq_stat(uin);
	level = get_fkq_level(hostUin);
	chatnum = visitnum = 0;
	if (level == 1) {
		CHANNEL *chan = getchanf(callbacki->g_ape, FKQ_PIP_NAME"%s", hostUin);
		if (chan == NULL) {
			/* channel can't use AUTO_DESTROY, for the message reenter */
			chan = mkchanf(callbacki->g_ape, CHANNEL_QUIET,
						   FKQ_PIP_NAME"%s", hostUin);
			ADD_FKQ_HOSTUIN(chan, hostUin);
		}

		chatnum = get_channel_usernum(chan);

		if (isonchannel(nuser, chan) && chatnum > 0)
			chatnum--;

		ADD_SUBUSER_FKQINITUIN(sub, hostUin);
		if (add_visitnum(callbacki->g_ape, uin, hostUin, &visitnum))
			notice_channel_visitnum(hostUin, 1, visitnum, callbacki->g_ape);
	}
	
	json_item *info = json_new_object();
	json_set_property_intZ(info, "hostchatnum", chatnum);
	json_set_property_intZ(info, "hostvisitnum", visitnum);

	jlist = json_new_object();
	json_set_property_strZ(jlist, "hostUin", hostUin);
	json_set_property_intZ(jlist, "fkqstat", stat);
	json_set_property_intZ(jlist, "hostfkqlevel", level);
	json_set_property_objZ(jlist, "hostfkinfo", info);
	newraw = forge_raw("FKQ_INIT", jlist);
	post_raw_sub(newraw, sub, callbacki->g_ape);
	POSTRAW_DONE(newraw);

	return (RETURN_NOTHING);
}

/*
 *input:
 *  {"cmd":"FKQ_JOIN", "chl":x, "sessid":"", "params":{"hostUin":""}}
 *    hostUin: 访客群主号码
 *output: 1, 或2, 或3[, 或4, 或3+4 4已走单独命令获取历史记录]
 *1,NOTHING. 用户已经加入了该频道时空返回
 *2,{"raw":"ERR", "data":{"code":"101", "value":"ERR_IS_BLACK"}}
    用户被列为该群黑名单时返回错误
 *3,{"raw":"CHANNEL","data":
        {"users":[
		    {"casttype":"uni","pubid":"dedd36d25f96b13c338889280aa4f112","properties":{"nick":"walker","uin":"1708931"},"level":1},
		    {"casttype":"uni","pubid":"f4fe0a87e9cfb93225521f3b3cfb2118","properties":{"nick":"bluker","uin":"1206839"},"level":1}],
		"pipe":{"casttype":"multi","pubid":"c6c37b0ba30b3a4b6f3d5deef4f8c76e","properties":{"name":"fangkequnpipe1206839"}}
		}
    }
	访客群频道信息
 *4,{"raw":"ERR", "data":{"code":"102", "value":"ERR_GROUP_BUSY"}}
    该群聊天人数超过设置
 */
static unsigned int fkq_join(callbackp *callbacki)
{
	char *uin, *hostUin;

	USERS *user = callbacki->call_user;
	//subuser *sub = callbacki->call_subuser;
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
				hn_senderr(callbacki, "101", "ERR_IS_BLACK:0:host");
				return (RETURN_NULL);
			} else {
				int chatnum = get_channel_usernum(chan);
				if (!isonchannel(user, chan) && strcmp(uin, hostUin) &&
					chatnum >= atoi(READ_CONF("max_user_per_group"))) {
					char tok[64];
					alog_warn("%s's chatnum reached %d", hostUin, chatnum);
					snprintf(tok, sizeof(tok), "ERR_GROUP_BUSY:%s", hostUin);
					hn_senderr(callbacki, "102", tok);
					return (RETURN_NULL);
				}
				if (!isonchannel(user, chan) && chatnum >= 1) {
					channel_onjoin(callbacki->g_ape, hostUin, chatnum+1);
				}
				join(user, chan, callbacki->g_ape);
				if (callbacki->call_subuser != NULL) {
					ADD_SUBUSER_HOSTUIN(callbacki->call_subuser, hostUin);
				}
				//post_raw_sub_recently(callbacki->g_ape, sub, hostUin, RRC_TYPE_GROUP_FKQ);
			}
		}
	}
	
	return (RETURN_NOTHING);
}

/*
 *input:
 *  {"cmd":"FKQ_OPEN", "chl":x, "sessid":"", "params":{"hostUin":""}}
 *    hostUin: open 完后要 join 到的访客群号码
 *output:
 *  {"raw":"FKQ_OPEND", "data":{}}
 */
static unsigned int fkq_open(callbackp *callbacki)
{
	char *uin;
	json_item *jlist = NULL;
	RAW *newraw;
	subuser *sub = callbacki->call_subuser;

	uin = GET_UIN_FROM_USER(callbacki->call_user);

	set_fkq_stat(uin, true);
	fkq_join(callbacki);

	jlist = json_new_object();
	newraw = forge_raw("FKQ_OPENED", jlist);
	post_raw_sub(newraw, sub, callbacki->g_ape);
	POSTRAW_DONE(newraw);

	return (RETURN_NOTHING);
}

/*
 *input:
 *  {"cmd":"FKQ_CLOSE", "chl":x, "sessid":"", "params":{}}
 *output:
 *  {"raw":"FKQ_CLOSED", "data":{}}
 */
static unsigned int fkq_close(callbackp *callbacki)
{
	char *uin;
	json_item *jlist = NULL;
	RAW *newraw;
	USERS *nuser = callbacki->call_user;
	subuser *sub = callbacki->call_subuser;

	uin = GET_UIN_FROM_USER(callbacki->call_user);

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

/*
 *input:
 *  {"cmd":"FKQ_BLACKLIST", "chl":x, "sessid":"", "params":{}}
 *output:
 *  {"raw":"FKQ_BLACKLIST","data":{"black":[{"1206839":2}]}}
 *    black: 黑名单列表数组, 前面是号码, 后面是黑名单类型. 2: 个人聊天黑名单, 3: 访客群黑名单
 *    没有黑名单时 "black":0
 */
static unsigned int fkq_blacklist(callbackp *callbacki)
{
	json_item *jlist = NULL;
	RAW *newraw;
	
	mevent_t *evt;
	struct data_cell *pc, *cc;

	subuser *sub = callbacki->call_subuser;
	char *uin;
	int ret;

	uin = GET_UIN_FROM_USER(callbacki->call_user);

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

/*
 *input:
 *  {"cmd":"FKQ_BLACKOP", "chl":x, "sessid":"", "params":
         {"op":"", "blackuin":"", "blacktype":x}}
 *  op: 操作类型. add: 添加黑名单, del: 删除黑名单,
 *      is: 判断他是不是我的黑名单用户, am: 判断我是不是他的黑名单用户
 *  blackuin: 对方用户号码
 *  blacktype: 黑名单类型. 2: 个人聊天黑名单, 3: 访客群黑名单
 *output: 1/2/3/4/5/6
 *1,{"raw":"DATA", "data":{"code":"999", "value":"OPERATION_SUCCESS"}}
 *2,{"raw":"ERR", "data":{"code":"1001", "value":"OPERATION_FAILURE"}}
 *3,{"raw":"ERR", "data":{"code":"111", "value":"ERR_ALREADYBLACK"}}
 *4,{"raw":"ERR", "data":{"code":"112", "value":"ERR_NOTBLACK"}}
 *5,{"raw":"DATA", "data":{"code":"1", "value":"IS_BLACK"}}
 *6,{"raw":"DATA", "data":{"code":"0", "value":"NOT_BLACK"}}
 */
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

	return (RETURN_NULL);
}

/*
 *input:
 *  {"cmd":"FKQ_VISITLIST", "chl":x, "sessid":"", "params":{"uin":"", "hostUin":""}}
 *    uin: 获取谁的浏览历史
 *    hostUin: uin 在哪个访客群中的浏览历史
 *output:
 *1,{"raw":"ERR","data":{"code":"009","value":"ERR_UIN_NEXIST"}}
 *2,{"raw":"FKQ_VISITLIST","data":
        {"visit":[
		    {"name":"name1","href":"href1","title":"title1","target":"_blank"},
			{"name":"name2","href":"href2","title":"title2","target":"_blank"}],
		 "hostUin":"1227829"}
		}
 *    visit: 访问历史列表
 *    hostUin: 访问历史群号
 */
static unsigned int fkq_visitlist(callbackp *callbacki)
{
	USERS *user;
	subuser *sub = callbacki->call_subuser;
	char *uin, *hostUin, key[128];

	JNEED_STR(callbacki->param, "uin", uin, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "hostUin", hostUin, RETURN_BAD_PARAMS);
	
	ASSAM_VISIT_KEY(key, hostUin);
	user = GET_USER_FROM_APE(callbacki->g_ape, uin);
	if (!user) {
		hn_senderr(callbacki, "009", "ERR_UIN_NEXIST");
		return (RETURN_NULL);
	}

	json_item *jlist, *jvisit, *jvlist = json_new_array();
	RAW *newraw;

	Queue *queue;
	QueueEntry *qv;
	anchor *anc;

	if ((queue = GET_FKQ_VISIT(user, key)) != NULL) {
		queue_iterate(queue, qv) {
			anc = (anchor*)qv->data;
			jvisit = json_new_object();
			json_set_property_strZ(jvisit, "name", anc->name);
			json_set_property_strZ(jvisit, "href", anc->href);
			json_set_property_strZ(jvisit, "title", anc->title);
			json_set_property_strZ(jvisit, "target", anc->target);

			json_set_element_obj(jvlist, jvisit);
		}
	}

	jlist = json_new_object();
	json_set_property_objZ(jlist, "visit", jvlist);
	json_set_property_strZ(jlist, "hostUin", hostUin);
	newraw = forge_raw("FKQ_VISITLIST", jlist);
	post_raw_sub(newraw, sub, callbacki->g_ape);
	POSTRAW_DONE(newraw);
	
	return (RETURN_NOTHING);
}

/*
 *input:
 *  {"cmd":"FKQ_VISITADD", "chl":x, "sessid":"", "params":
 *    {"hostUin":"", "name":"", "href":""}
 *  }
 *    hostUin: 访客群号
 *    name: 浏览页面标题
 *    href: 浏览页面链接
 *output:
 *  {"raw":"DATA", "data":{"code":"999", "value":"OPERATION_SUCCESS"}}
 */
static unsigned int fkq_visitadd(callbackp *callbacki)
{
	USERS *user = callbacki->call_user;
	char *uin, *hostUin, *name, *href, key[128];
	int maxnum;

	uin = GET_UIN_FROM_USER(user);

	JNEED_STR(callbacki->param, "hostUin", hostUin, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "name", name, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "href", href, RETURN_BAD_PARAMS);

	ASSAM_VISIT_KEY(key, hostUin);
	maxnum = atoi(READ_CONF("fkq_max_visit_size"));
	if (maxnum > 0) {
			anchor *anc = anchor_new(name, href, ANC_DFT_TITLE, ANC_DFT_TARGET);
			Queue *queue = GET_FKQ_VISIT(user, key);
			if (!queue) {
				queue = queue_new(maxnum, anchor_free);
				MAKE_FKQ_VISIT(user, key, queue);
			}
			if (queue_find(queue, anc, anchor_cmp) == -1) {
				queue_fixlen_push_head(queue, anc);
			} else {
				anchor_free(anc);
			}
	}
	hn_senddata(callbacki, "999", "OPERATION_SUCCESS");

	return (RETURN_NOTHING);
}

/*
 *input:
 *  {"cmd":"FKQ_SEND", "chl":x, "sessid":"", "params":{"msg":"", "pipe":""}}
 *    msg: 消息内容
 *    pipe: 管道 ID
 *output:
 *1,NOTHING
    成功发送
 *2,{"raw":"ERR", "data":{"code":"101", "value":"ERR_IS_BLACK:%s:%s"}}
    用户被列为该群黑名单时返回错误
 */
static unsigned int fkq_send(callbackp *callbacki)
{
	json_item *jlist = NULL;
	RAW *newraw;
	char *msg, *pipe, *uinfrom, *uinto = NULL, *nickto = NULL;
	CHANNEL *chan;
	USERS *user = callbacki->call_user;
	st_fkq *st = GET_FKQ_STAT(callbacki->g_ape);

	uinfrom = GET_UIN_FROM_USER(user);
	
	JNEED_STR(callbacki->param, "msg", msg, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "pipe", pipe, RETURN_BAD_PARAMS);

	if (st) st->msg_total++;

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
				nickto = GET_NICK_FROM_USER(user);
				if (uinto && user_is_black(uinto, uinfrom, BLACK_TYPE_MIM_USER)) {
					post = false;
				}
			}
		}

		if (post) {
			jlist = json_new_object();
			json_set_property_strZ(jlist, "msg", msg);
			jlist = post_to_pipe(jlist, RAW_FKQDATA, pipe,
								 callbacki->call_subuser, callbacki->g_ape, true);

			/* push raw to recently */
			if (uinto) {
				newraw = forge_raw("RAW_RECENTLY", jlist);
				if (spipe->type == CHANNEL_PIPE) {
					push_raw_recently_group(callbacki->g_ape, newraw,
											uinto, RRC_TYPE_GROUP_FKQ);
				} else {
					push_raw_recently_single(callbacki->g_ape, newraw,
											 uinfrom, uinto);
				}
				POSTRAW_DONE(newraw);
			}
		} else {
			char tok[128];

			if (spipe->type == CHANNEL_PIPE) {
				snprintf(tok, sizeof(tok), "ERR_IS_BLACK:0:host");
			} else {
				snprintf(tok, sizeof(tok), "ERR_IS_BLACK:%s:%s", uinto, nickto);
			}
			alog_warn("user %s in %s:%d's black", uinfrom, uinto, spipe->type);
			hn_senderr(callbacki, "101", tok);
			return (RETURN_NULL);
		}
	}
	
	return (RETURN_NOTHING);
}

static void fkq_event_delsubuser(subuser *del, acetables *g_ape)
{
	char *uin;
	USERS *user = del->user;
	subuser *cur = user->subuser;
	char *otheruin;
	bool lastone = true;

	uin = GET_SUBUSER_HOSTUIN(del);
	if (uin != NULL) {
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

	uin = GET_SUBUSER_FKQINITUIN(del);
	cur = user->subuser;
	if (uin != NULL) {
		while (cur != NULL) {
			otheruin = GET_SUBUSER_FKQINITUIN(cur);
			if (cur != del && otheruin != NULL && !strcmp(uin, otheruin)) {
				lastone = false;
				break;
			}
			cur = cur->next;
		}
			
		if (lastone) {
			char *myuin = GET_UIN_FROM_USER(user);
			int changenum, currentnum;
			changenum = del_visitnum(g_ape, myuin, uin, &currentnum);
			if (changenum)
				notice_channel_visitnum(uin, changenum, currentnum, g_ape);
		}
	}
} 

static void init_module(acetables *g_ape)
{
	MAKE_USER_TBL(g_ape);
	MAKE_ONLINE_TBL(g_ape);
	MAKE_VISITNUM_TBL(g_ape);
	/*
	 * CHATNUM_TBL is a hostUin => chatnum table,
	 * where chatnum > 1
	 */
	MAKE_CHATNUM_TBL(g_ape);
	MAKE_FKQ_STAT(g_ape, calloc(1, sizeof(st_fkq)));
	
    add_periodical((1000*60*30), 0, tick_static, g_ape, g_ape);
	
	register_cmd("FKQ_INIT", 		fkq_init, 		NEED_SESSID, g_ape);
	register_cmd("FKQ_JOIN", 		fkq_join, 		NEED_SESSID, g_ape);
	register_cmd("FKQ_OPEN", 		fkq_open, 		NEED_SESSID, g_ape);
	register_cmd("FKQ_CLOSE", 		fkq_close, 		NEED_SESSID, g_ape);
	register_cmd("FKQ_BLACKLIST", 	fkq_blacklist, 	NEED_SESSID, g_ape);
	register_cmd("FKQ_BLACKOP", 	fkq_blackop, 	NEED_SESSID, g_ape);
	register_cmd("FKQ_VISITLIST", 	fkq_visitlist, 	NEED_SESSID, g_ape);
	register_cmd("FKQ_VISITADD", 	fkq_visitadd, 	NEED_SESSID, g_ape);
	register_cmd("FKQ_SEND", 		fkq_send, 		NEED_SESSID, g_ape);
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
