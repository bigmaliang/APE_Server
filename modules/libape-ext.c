#include "plugins.h"

#include "libape-ext.h"
#include "extevent.h"

#define MODULE_NAME "ext"

static ace_plugin_infos infos_module = {
	"\"Extend-APE\" system",	// Module Name
	"1.0",						// Module Version
	"bigml",					// Module Author
	"mod_ext.conf"				// config file
};

static void init_module(acetables *g_ape)
{
	MAKE_USER_TBL(g_ape);		/* erased in deluser() */
	MAKE_ONLINE_TBL(g_ape);		/* erased in deluser() */

	MAKE_EXT_STAT(g_ape, calloc(1, sizeof(stExt)));
	ext_e_init(READ_CONF("event_plugin"));
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
	NULL,
	NULL,

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
