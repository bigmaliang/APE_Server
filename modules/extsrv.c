#include "libape-ext.h"
#include "extsrv.h"
#include "extevent.h"

#include "apev.h"

extern HASH *stbl;
HASH *utbl = NULL;

static acetables *ape = NULL;

static NEOERR* ext_cmd_useron(struct queue_entry *q)
{
	char *srcx, *uin;

	REQ_GET_PARAM_STR(q->hdfrcv, "srcx", srcx);
	REQ_GET_PARAM_STR(q->hdfrcv, "uin", uin);

	alog_dbg("server %s's user %s on", srcx, uin);
	
	UserEntry *u = (UserEntry*)hash_lookup(utbl, uin);
	if (!u) {
		u = user_new();
		hash_insert(utbl, (void*)strdup(uin), (void*)u);
	}
	u->online = true;

	SnakeEntry *s = (SnakeEntry*)hash_lookup(stbl, srcx);
	if (!s) u->server = strdup(srcx);
	else {
		u->server = s->name;
		s->num_online++;
	}

	return STATUS_OK;
}

static NEOERR* ext_cmd_useroff(struct queue_entry *q)
{
	char *srcx, *uin;

	REQ_GET_PARAM_STR(q->hdfrcv, "srcx", srcx);
	REQ_GET_PARAM_STR(q->hdfrcv, "uin", uin);

	alog_dbg("server %s's user %s off", srcx, uin);
	
	UserEntry *u = (UserEntry*)hash_lookup(utbl, uin);
	if (!u) {
		u = user_new();
		hash_insert(utbl, (void*)strdup(uin), (void*)u);
	}
	u->online = false;
	SnakeEntry *s = (SnakeEntry*)hash_lookup(stbl, srcx);
	if (s && s->num_online > 0) s->num_online--;
	
	return STATUS_OK;
}

static NEOERR* ext_cmd_msgsnd(struct queue_entry *q)
{
	char *srcuin, *dstuin, *msg;
	
	if (!ape || !q) return nerr_raise(NERR_ASSERT, "input error");

	REQ_GET_PARAM_STR(q->hdfrcv, "srcuin", srcuin);
	REQ_GET_PARAM_STR(q->hdfrcv, "dstuin", dstuin);
	REQ_GET_PARAM_STR(q->hdfrcv, "msg", msg);

	alog_dbg("user %s say %s to %s", srcuin, msg, dstuin);
	
	USERS *user = GET_USER_FROM_APE(ape, dstuin);
	if (user) {
		json_item *jlist;
		RAW *newraw;

		jlist = json_new_object();
		json_set_property_strZ(jlist, "from", srcuin);
		json_set_property_strZ(jlist, "to_uin", dstuin);
		json_set_property_strZ(jlist, "msg", msg);
		newraw = forge_raw("EXT_SEND", jlist);
		post_raw(newraw, user, ape);
		POSTRAW_DONE(newraw);
	} else {
		return nerr_pass(ext_e_useroff(dstuin));
	}

	return STATUS_OK;
}

static NEOERR* ext_cmd_state(struct queue_entry *q)
{
	SnakeEntry *s = hash_lookup(stbl, id_me);
	if (!s) return nerr_raise(NERR_ASSERT, "%s not found", id_me);
	
	hdf_set_value(q->hdfsnd, "server name", s->name);
	hdf_set_int_value(q->hdfsnd, "user online from v", s->num_online);
	hdf_set_int_value(q->hdfsnd, "user online from ape", ape->nConnected);
	
	return STATUS_OK;
}

static NEOERR* ext_process_driver(struct event_entry *e, struct queue_entry *q)
{
	NEOERR *err;

	switch(q->operation) {
	case REQ_CMD_USERON:
		err = ext_cmd_useron(q);
		break;
	case REQ_CMD_USEROFF:
		err = ext_cmd_useroff(q);
		break;
	case REQ_CMD_MSGSND:
		err = ext_cmd_msgsnd(q);
		break;
	case REQ_CMD_MSGBRD:
		//err = ext_cmd_msgbrd(q);
		break;
	case REQ_CMD_STATE:
		err = ext_cmd_state(q);
		break;
	default:
		err = nerr_raise(NERR_ASSERT, "unknown command %u", q->operation);
		break;
	}

	return nerr_pass(err);
}

static NEOERR* ext_start_driver()
{
	return nerr_pass(hash_init(&utbl, hash_str_hash, hash_str_comp));
}

struct event_entry ext_entry = {
	.name = "apex",
	.start_driver = ext_start_driver,
	.process_driver = ext_process_driver,
	.stop_driver = NULL,
	.next = NULL,
};

static void ext_read(ape_socket *client, ape_buffer *buf,
					 size_t offset, acetables *g_ape)
{
	NEOERR *err = udps_recv(client->fd, 0, NULL);
	TRACE_NOK(err);
}

void ext_s_init(acetables *g_ape, char *ip, char *port, char *me)
{
	NEOERR *err;
	
	ape = g_ape;

	err = nerr_init();
	TRACE_NOK(err);

	if (me) ext_entry.name = me;
	err = parse_regist_v(&ext_entry);
	TRACE_NOK(err);
	
	int sock = udps_init(ip, atoi(port));

	prepare_ape_socket(sock, g_ape);

	g_ape->co[sock]->fd = sock;
	g_ape->co[sock]->stream_type = STREAM_DELEGATE;
	g_ape->co[sock]->callbacks.on_read = ext_read;
	g_ape->co[sock]->callbacks.on_write = NULL;

	events_add(g_ape->events, sock, EVENT_READ|EVENT_WRITE);
}
