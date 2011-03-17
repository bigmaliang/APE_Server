#include "extevent.h"

#include "apev.h"

HASH *stbl = NULL;
extern HASH *utbl;

char *id_v = NULL;
char *id_me = NULL;

void ext_e_init(char *evts)
{
	char *tkn[10];
	int nTok = 0;
	nTok = explode(' ', evts, tkn, 10);

	if (!stbl) hash_init(&stbl, hash_str_hash, hash_str_comp);
	
	while (nTok >= 0) {
		SnakeEntry *s = snake_new(tkn[nTok]);
		if (s) {
			alog_dbg("event %s init ok", tkn[nTok]);
			hash_insert(stbl, tkn[nTok], (void*)s);
		} else {
			alog_err("init event %s failure", tkn[nTok]);
		}
		nTok--;
	}
}

NEOERR* ext_e_useron(char *uin)
{
	SnakeEntry *s = (SnakeEntry*)hash_lookup(stbl, id_v);
	if (!s || !uin) return nerr_raise(NERR_ASSERT, "%s not found", id_v);

	hdf_set_value(s->evt->hdfsnd, "uin", uin);
	hdf_set_value(s->evt->hdfsnd, "srcx", id_me);
	EVT_EXT_TRIGGER(s->evt, uin, REQ_CMD_USERON, FLAGS_NONE);

	return STATUS_OK;
}

NEOERR* ext_e_useroff(char *uin)
{
	SnakeEntry *s = (SnakeEntry*)hash_lookup(stbl, id_v);
	if (!s || !uin) return nerr_raise(NERR_ASSERT, "%s not found", id_v);

	hdf_set_value(s->evt->hdfsnd, "uin", uin);
	hdf_set_value(s->evt->hdfsnd, "srcx", id_me);
	EVT_EXT_TRIGGER(s->evt, uin, REQ_CMD_USEROFF, FLAGS_NONE);

	return STATUS_OK;
}

NEOERR* ext_e_msgsnd(char *fuin, char *tuin, char *msg)
{
	if (!fuin || !tuin || !msg) return nerr_raise(NERR_ASSERT, "input error");
	SnakeEntry *s = NULL;
	
	UserEntry *u = (UserEntry*)hash_lookup(utbl, tuin);
	if (u) {
		if (u->online) {
			s = (SnakeEntry*)hash_lookup(stbl, u->server);
			if (!s) return nerr_raise(NERR_ASSERT, "%s not found", u->server);
		} else {
			/*
			 * TODO offline message
			 */
			return STATUS_OK;
		}
	} else {
		s = (SnakeEntry*)hash_lookup(stbl, id_v);
		if (!s) return nerr_raise(NERR_ASSERT, "%s not found", id_v);
	}

	hdf_set_value(s->evt->hdfsnd, "srcx", id_me);
	hdf_set_value(s->evt->hdfsnd, "srcuin", fuin);
	hdf_set_value(s->evt->hdfsnd, "dstuin", tuin);
	hdf_set_value(s->evt->hdfsnd, "msg", msg);
	EVT_EXT_TRIGGER(s->evt, fuin, REQ_CMD_MSGSND, FLAGS_NONE);

	return STATUS_OK;
}

void ext_static(acetables *g_ape, int lastcall)
{
	SnakeEntry *s = (SnakeEntry*)hash_lookup(stbl, "ape_ext_v");
	if (!s) return;

	alog_dbg("tick");
	
	hdf_set_value(s->evt->hdfsnd, "srcx", "ape_ext_a");
	hdf_set_value(s->evt->hdfsnd, "srcuin", "10");
	hdf_set_value(s->evt->hdfsnd, "dstuin", "20");
	hdf_set_value(s->evt->hdfsnd, "msg", "hi from ape_ext_a");
	//EVT_EXT_TRIGGER_VOID(evt, "foo", 1001, FLAGS_NONE);
}
