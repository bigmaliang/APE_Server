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
static anchor_t* anchor_new(const char *name, const char *href,
							const char *title, const char *target)
{
	if (!name || !href || !title || !target) return NULL;

	anchor_t *anc = xmalloc(sizeof(anchor_t));
	anc->name = strdup(name);
	anc->href = strdup(href);
	anc->title = strdup(title);
	anc->target = strdup(target);

	return anc;
}

static void anchor_free(void *a)
{
	if (!a) return;

	anchor_t *anc = (anchor_t*)a;
	SFREE(anc->name);
	SFREE(anc->href);
	SFREE(anc->title);
	SFREE(anc->target);
	SFREE(anc);
}

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
 *input:
 *  {"cmd":"FKQ_INIT", "chl":x, "sessid":"", "params":{"nick":"", "hostUin":""}}
 *    nick: 访客用户昵称
 *    hostUin: 访客群主号码
 *output:
 *  {"raw":"FKQ_INIT", "data":
        {"fkqstat":1, "hostfkqlevel":0, "hostfkinfo":{"hostchatnum":0}}
    }
 *    fkqstat: 访问者访客群聊天功能状态. 0: 开启, 1: 关闭
 *    hostfkqlevel: 访客群主访客群等级. 0 表示 hostUin 为没有访客群功能的普通用户
 *    hostfkinfo: 访客群群内信息, 当前只包含聊天人数
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

/*
 *input:
 *  {"cmd":"FKQ_JOIN", "chl":x, "sessid":"", "params":{"hostUin":""}}
 *    hostUin: 访客群主号码
 *output: 1, 或2, 或3, 或4, 或3+4
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
 *4,{"raw":"DATA", "data":""}
    该群最近 20 条聊天信息
 */
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

/*
 *input:
 *  {"cmd":"FKQ_OPEN", "chl":x, "sessid":"", "params":{}}
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
 *  {"cmd":"FKQ_BLACKLISTOP", "chl":x, "sessid":"", "params":
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

	return (RETURN_NOTHING);
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
		return (RETURN_NOTHING);
	}

	json_item *jlist, *jvisit, *jvlist = json_new_array();
	RAW *newraw;

	Queue *queue;
	QueueEntry *qv;
	anchor_t *anc;

	if ((queue = GET_FKQ_VISIT(user, key)) != NULL) {
		queue_iterate(queue, qv) {
			anc = (anchor_t*)qv->data;
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
			anchor_t *anc = anchor_new(name, href, ANC_DFT_TITLE, ANC_DFT_TARGET);
			Queue *queue = GET_FKQ_VISIT(user, key);
			if (!queue) {
				queue = queue_new(maxnum);
				MAKE_FKQ_VISIT(user, key, queue);
			}
			queue_fixlen_push_head(queue, anc, anchor_free);
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
 *2,{"raw":"ERR", "data":{"code":"101", "value":"ERR_IS_BLACK"}}
    用户被列为该群黑名单时返回错误
 */
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
