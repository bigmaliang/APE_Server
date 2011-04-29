/*
 * libape-ext.c
 * make APE 1.x backend extendible, and can communicate with each other.
 * through a control-center (https://github.com/bigml/cmoon/tree/bomdoo/deliver/v)
 * see deliver/v/extend.png for network skeleton
 *
 * this module depend on following librarys(see modules/Makefile for detail):
 * ***libneo_utl.a: http://www.clearsilver.net/
 * ***libmevent.a:  https://github.com/bigml/cmoon/tree/master/lib/mevent
 */

#include "plugins.h"
#include "libape-ext.h"

#include "extsrv.h"				/* event server */
#include "extevent.h"			/* event client */

#define MODULE_NAME "ext"

static ace_plugin_infos infos_module = {
	"\"Extend-APE\" system",	// Module Name
	"1.0",						// Module Version
	"bigml",					// Module Author
	"mod_ext.conf"				// config file
};

/*
 * command: EXT_SEND
 * send message to another user, no matter they are on the same or another aped
 * hisotry & offline message should be hand on HOOK_EXT_SEND
 */
static unsigned int ext_send(callbackp *callbacki)
{
	char *uin, *msg;
	USERS *fuser = callbacki->call_user;
	NEOERR *err;

	JNEED_STR(callbacki->param, "uin", uin, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "msg", msg, RETURN_BAD_PARAMS);

	char *fuin = GET_UIN_FROM_USER(fuser);
	if (fuin && !strcmp(fuin, uin)) return (RETURN_NOTHING);
	
	USERS *tuser = GET_USER_FROM_APE(callbacki->g_ape, uin);
	if (tuser) {
		json_item *jlist = json_new_object();
		json_set_property_strZ(jlist, "msg", msg);
		json_set_property_objZ(jlist, "from", get_json_object_user(fuser));
		json_set_property_strZ(jlist, "to_uin", uin);
		RAW *newraw = forge_raw("EXT_SEND", jlist);
		post_raw(newraw, tuser, callbacki->g_ape);
		POSTRAW_DONE(newraw);
	} else {
		err = ext_e_msgsnd(fuin, uin, msg);
		TRACE_NOK(err);
	}
	
	return (RETURN_NOTHING);
}

static unsigned int ext_broad(callbackp *callbacki)
{
	char *msg, *cid, *ctype, *fuin;
	USERS *user = callbacki->call_user;
	CHANNEL *chan;
	NEOERR *err;

	fuin = GET_UIN_FROM_USER(user);
	
	JNEED_STR(callbacki->param, "msg", msg, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "cid", cid, RETURN_BAD_PARAMS);
	JNEED_STR(callbacki->param, "ctype", ctype, RETURN_BAD_PARAMS);

	/*
	 * user must make his channels and join the members
	 * after he connected to me immediately
	 */
	chan = getchanf(callbacki->g_ape, EXT_PIP_NAME"%s_%s", ctype, cid);
	if (!chan) chan = mkchanf(callbacki->g_ape, CHANNEL_AUTODESTROY,
							  EXT_PIP_NAME"%s_%s", ctype, cid);
	if (chan) {
		json_item *jlist;
		RAW *newraw;

		jlist = json_new_object();
		json_set_property_strZ(jlist, "msg", msg);
		json_set_property_objZ(jlist, "from", get_json_object_user(user));
		newraw = forge_raw("EXT_BROAD", jlist);
		post_raw_channel(newraw, chan, callbacki->g_ape);
		POSTRAW_DONE(newraw);
	} else {
		alog_err("make channel for %s %s failure", ctype, cid);
	}
	
	err = ext_e_msgbrd(fuin, msg, cid, ctype);
	TRACE_NOK(err);
	
	return (RETURN_NOTHING);
}

/*
 * excute before deluser(), to notify control-center(also named v)
 */
static int ext_event_deluser(USERS *user, int istmp, acetables *g_ape)
{
	NEOERR *err;
	
	err = ext_e_useroff(GET_UIN_FROM_USER(user));
	TRACE_NOK(err);

	return RET_PLUGIN_CONTINUE;
}

/*
 * excute after adduser(), to notify control-center(also named v)
 */
static void ext_event_adduser(USERS *user, acetables *g_ape)
{
	NEOERR *err;
	
	err = ext_e_useron(user, g_ape);
	TRACE_NOK(err);
}

/*
 * excute after mkchan(), to notify other aped
 */
static void ext_event_mkchan(CHANNEL *chan, acetables *g_ape)
{
	NEOERR *err;

	err = ext_e_chan_attend(chan->name);
	TRACE_NOK(err);
}

static void init_module(acetables *g_ape)
{
	MAKE_USER_TBL(g_ape);		/* erased in deluser() */
	MAKE_ONLINE_TBL(g_ape);		/* erased in deluser() */

	MAKE_EXT_STAT(g_ape, calloc(1, sizeof(stExt)));

	id_v = "ape_ext_v";
	id_me = READ_CONF("me");
	ext_s_init(g_ape, READ_CONF("ip"), READ_CONF("port"), READ_CONF("me"));
	ext_e_init(READ_CONF("event_plugin"), READ_CONF("relation_plugin"));
	
	add_periodical((EVENT_HB_SEC*1000), 0, ext_event_static, g_ape, g_ape);

	register_cmd("EXT_SEND", ext_send, NEED_SESSID, g_ape);
	register_cmd("EXT_BROAD", ext_broad, NEED_SESSID, g_ape);
}

static void free_module(acetables *g_ape)
{
	;
}

static ace_callbacks callbacks = {
	/* pre user event fired */
	NULL,
	NULL,
	ext_event_deluser,
	NULL,
	NULL,
	
	/* pre channel event fired */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	/* post user event hooked */
	NULL,
	ext_event_adduser,
	NULL,
	NULL,
	NULL,

	/* post channel event hooked */
	NULL,
	NULL,
	ext_event_mkchan,
	NULL,
	NULL,
	NULL,

	/* tickuser */
	NULL
};

APE_INIT_PLUGIN(MODULE_NAME, init_module, free_module, callbacks)
