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
static int lcs_service_stat(char *appi)
{
	return LCS_SERVICE_NORMAL;
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

	JNEED_STR(callbacki->param, "appid", appid, RETURN_BAD_PARAMS);
	uin = GET_UIN_FROM_USER(user);

	if (lcs_service_stat(appid) >= LCS_SERVICE_NORMAL) {
		chan = getchanf(callbacki->g_ape, LCS_PIP_NAME"%s", appid);
		if (!chan) {
			chan = mkchanf(callbacki->g_ape, CHANNEL_AUTODESTROY | CHANNEL_QUIET,
						   LCS_PIP_NAME"%s", appid);
			if (chan == NULL) {
				alog_err("make channel %s failure", appid);
				hn_senderr(callbacki, "007", "ERR_MAKE_CHANNEL");
				return (RETURN_NOTHING);
			}
		}
		join(user, chan, callbacki->g_ape);
	} else {
		alog_warn("app %s out of service", appid);
		hn_senderr(callbacki, "010", "ERR_OUT_OF_SERVICE");
		return (RETURN_NOTHING);
	}
	
	return (RETURN_NOTHING);
}

static void init_module(acetables *g_ape)
{
	MAKE_USER_TBL(g_ape);
	MAKE_ONLINE_TBL(g_ape);
	/*
	 * CHATNUM_TBL is a appid => chatnum table,
	 * where chatnum > 1
	 */
	MAKE_CHATNUM_TBL(g_ape);
	
    //add_periodical((1000*60*30), 0, tick_static, g_ape, g_ape);
	
	register_cmd("LCS_JOIN", 		lcs_join, 		NEED_SESSID, g_ape);
	//register_cmd("LCS_JOIN_B", 		lcs_join_b,		NEED_SESSID, g_ape);
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
