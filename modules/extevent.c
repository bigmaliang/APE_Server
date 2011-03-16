#include "extevent.h"

static HASH *evth = NULL;

char *id_v = NULL;
char *id_me = NULL;

void ext_e_init(char *evts)
{
	mevent_t *evt;
	char *tkn[10];
	int nTok = 0;
	nTok = explode(' ', evts, tkn, 10);

	if (!evth) hash_init(&evth, hash_str_hash, hash_str_comp);
	
	while (nTok >= 0) {
		evt = mevent_init_plugin(tkn[nTok]);
		if (evt) {
			alog_dbg("event %s init ok", tkn[nTok]);
			hash_insert(evth, tkn[nTok], (void*)evt);
		} else {
			alog_err("init event %s failure", tkn[nTok]);
		}
		nTok--;
	}
}

NEOERR* ext_e_useron(char *uin)
{
	mevent_t *evt = (mevent_t*)hash_lookup(evth, id_v);
	if (!evt) return nerr_raise(NERR_ASSERT, "%s not found", id_v);

	hdf_set_value(evt->hdfsnd, "uin", uin);
	hdf_set_value(evt->hdfsnd, "srcx", id_me);
	EVT_EXT_TRIGGER(evt, uin, REQ_CMD_USERON, FLAGS_NONE);

	return STATUS_OK;
}

NEOERR* ext_e_useroff(char *uin)
{
	mevent_t *evt = (mevent_t*)hash_lookup(evth, id_v);
	if (!evt) return nerr_raise(NERR_ASSERT, "%s not found", id_v);

	hdf_set_value(evt->hdfsnd, "uin", uin);
	hdf_set_value(evt->hdfsnd, "srcx", id_me);
	EVT_EXT_TRIGGER(evt, uin, REQ_CMD_USEROFF, FLAGS_NONE);

	return STATUS_OK;
}

void ext_static(acetables *g_ape, int lastcall)
{
	mevent_t *evt = (mevent_t*)hash_lookup(evth, "ape_ext_v");
	if (!evt) return;

	alog_dbg("tick");
	
	hdf_set_value(evt->hdfsnd, "srcx", "ape_ext_a");
	hdf_set_value(evt->hdfsnd, "srcuin", "10");
	hdf_set_value(evt->hdfsnd, "dstuin", "20");
	hdf_set_value(evt->hdfsnd, "msg", "hi from ape_ext_a");
	EVT_EXT_TRIGGER_VOID(evt, "foo", 1001, FLAGS_NONE);
}
