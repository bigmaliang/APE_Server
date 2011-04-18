#include "libape-ext.h"
#include "extevent.h"

#include "apev.h"

char *id_v = NULL;				/* control-center(v)'s id */
char *id_me = NULL;				/* my id, see mod_ext.conf */

HASH *stbl = NULL;				/* snake table */
HASH *utbl = NULL;				/* user(not on me) table */
HASH *ctbl = NULL;				/* channel table (which snake[s] not on my channel) */

mevent_t *e_group = NULL;		/* relation backend */

int LERR_ALLDIE = 0;			/* 25 */

void ext_e_init(char *evts, char *relation)
{
	NEOERR *err;
	char *tkn[10];
	int nTok = 0;
	nTok = explode(' ', evts, tkn, 10);

	if (!stbl) hash_init(&stbl, hash_str_hash, hash_str_comp);
	if (!utbl) hash_init(&utbl, hash_str_hash, hash_str_comp);
	if (!ctbl) hash_init(&ctbl, hash_str_hash, hash_str_comp);
	
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

	e_group = mevent_init_plugin(relation);
	if (!e_group) alog_err("init relation backend %s failure", relation);

	err = nerr_init();
	TRACE_NOK(err);

	err = merr_init((MeventLog)ape_log);
	TRACE_NOK(err);

	err = nerr_register(&LERR_ALLDIE, "当前没有可使用的聊天后台");
	TRACE_NOK(err);
}

NEOERR* ext_e_useron(USERS *user, acetables *ape)
{
	char *uin = GET_UIN_FROM_USER(user);
	SnakeEntry *s = (SnakeEntry*)hash_lookup(stbl, id_v);
	if (!s || !uin) return nerr_raise(NERR_ASSERT, "%s not found", id_v);

	hdf_set_value(s->evt->hdfsnd, "uin", uin);
	hdf_set_value(s->evt->hdfsnd, "srcx", id_me);
	MEVENT_TRIGGER(s->evt, uin, REQ_CMD_USERON, FLAGS_NONE);

	hdf_set_value(e_group->hdfsnd, "uin", uin);
	// TODO REQ_CMD_GROUPINFO
	//MEVENT_TRIGGER(e_group, uin, REQ_CMD_GROUPINFO, FLAGS_SYNC);
	/*
	 * groups {
	 *     0 {
	 *         ctype = 1
	 *         cid = 7876
	 *         users {
	 *             0 = 1123
	 *             1 = 1143
	 *             2 = 1423
	 *         }
	 *     }
	 *     1 {
	 *         ctype = 2
	 *         cid = 776
	 *         users {
	 *             0 = 1323
	 *         }
	 *     }
	 * }
	 */

	HDF *node = hdf_get_child(e_group->hdfrcv, "groups"), *cnode;
	char *ctype, *cid;
	CHANNEL *chan;
	USERS *ouser;
	while (node) {
		/*
		 * process this group
		 */
		ctype = hdf_get_value(node, "ctype", NULL);
		cid = hdf_get_value(node, "cid", NULL);
		cnode = hdf_get_child(node, "users");
		if (ctype && cid) {
			chan = getchanf(ape, EXT_PIP_NAME"%s_%s", ctype, cid);
			if (!chan) chan = mkchanf(ape, CHANNEL_AUTODESTROY,
									  EXT_PIP_NAME"%s_%s", ctype, cid);
			if (!chan) {
				alog_err("make channel ExtendPipe_%s_%s error", ctype, cid);
				continue;
			}

			/*
			 * join myself to my group
			 */
			join(user, chan, ape);

			/*
			 * join member to my group
			 */
			while (cnode) {
				ouser = GET_USER_FROM_APE(ape, hdf_obj_value(cnode));
				if (ouser) join(ouser, chan, ape);
				
				cnode = hdf_obj_next(cnode);
			}
		}
		
		node = hdf_obj_next(node);
	}

	return STATUS_OK;
}

NEOERR* ext_e_useroff(char *uin)
{
	SnakeEntry *s = (SnakeEntry*)hash_lookup(stbl, id_v);
	if (!s || !uin) return nerr_raise(NERR_ASSERT, "%s not found", id_v);

	hdf_set_value(s->evt->hdfsnd, "uin", uin);
	hdf_set_value(s->evt->hdfsnd, "srcx", id_me);
	MEVENT_TRIGGER(s->evt, uin, REQ_CMD_USEROFF, FLAGS_NONE);

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
	MEVENT_TRIGGER(s->evt, fuin, REQ_CMD_MSGSND, FLAGS_NONE);

	return STATUS_OK;
}

NEOERR* ext_e_msgbrd(char *fuin, char *msg, char *cid, char *ctype)
{
	char chan[64], *key;

	snprintf(chan, sizeof(chan), "%s_%s", ctype, cid);
	
	ChanEntry *c = hash_lookup(ctbl, chan);

	SnakeEntry *s = hash_next(stbl, (void**)&key);
	while (s) {
		if (!strcmp(s->name, id_v) ||
			(c && name_find(c->x_missed, chan) >= 0)) {
			/*
			 * this snake hasn't the channel, so, ignore it
			 */
			goto next;
		}

		hdf_set_value(s->evt->hdfsnd, "srcx", id_me);
		hdf_set_value(s->evt->hdfsnd, "srcuin", fuin);
		hdf_set_value(s->evt->hdfsnd, "msg", msg);
		hdf_set_value(s->evt->hdfsnd, "cid", cid);
		hdf_set_value(s->evt->hdfsnd, "ctype", ctype);
		MEVENT_TRIGGER_NRET(s->evt, fuin, REQ_CMD_MSGBRD, FLAGS_NONE);

	next:
		s = hash_next(stbl, (void**)&key);
	}


	/*
	 * centry expired, post to v, and get it from backend
	 */
	return STATUS_OK;
}

NEOERR* ext_e_chan_miss(char *cid, char *ctype, char *id)
{
	SnakeEntry *s = hash_lookup(stbl, id);

	if (!s) return nerr_raise(NERR_ASSERT, "%s not found", id);

	hdf_set_value(s->evt->hdfsnd, "srcx", id_me);
	hdf_set_value(s->evt->hdfsnd, "ctype", ctype);
	hdf_set_value(s->evt->hdfsnd, "cid", cid);
	MEVENT_TRIGGER(s->evt, cid, REQ_CMD_CHAN_MISS, FLAGS_NONE);

	return STATUS_OK;
}

NEOERR* ext_e_chan_attend(char *chan)
{
	char *key;
	SnakeEntry *s = hash_next(stbl, (void**)&key);

	while (s) {
		if (!strcmp(s->name, id_v)) goto next;

		hdf_set_value(s->evt->hdfsnd, "srcx", id_me);
		hdf_set_value(s->evt->hdfsnd, "cname", chan);
		MEVENT_TRIGGER_NRET(s->evt, chan, REQ_CMD_CHAN_ATTEND, FLAGS_NONE);
		
	next:
		s = hash_next(stbl, (void**)&key);
	}
	
	return STATUS_OK;
}

void ext_event_static(acetables *g_ape, int lastcall)
{
	SnakeEntry *s = (SnakeEntry*)hash_lookup(stbl, "ape_ext_v");
	if (!s) return;

	hdf_set_value(s->evt->hdfsnd, "srcx", id_me);
	hdf_set_int_value(s->evt->hdfsnd, "num_online", g_ape->nConnected);
	MEVENT_TRIGGER_VOID(s->evt, id_me, REQ_CMD_HB, FLAGS_NONE);
}
