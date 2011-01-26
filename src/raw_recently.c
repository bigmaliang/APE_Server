/*
  Copyright (C) 2006, 2007, 2008, 2009  Philipp Fuehrer <pf@netzbeben.de>

  This file is part of APE Server.
  APE is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  APE is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with APE ; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/* raw_recently.c */

#include "raw_recently.h"
#include "users.h"
#include "utils.h"
#include "raw.h"
#include "hash.h"
#include "config.h"
#include "queue.h"
#include "hnpub.h"

#define ADD_RRC_QUEUE_TBL(g_ape)										\
	do {																\
		if (get_property(g_ape->properties, "raw_recently_queue") == NULL) { \
			add_property(&g_ape->properties, "raw_recently_queue", hashtbl_init(), \
						 queue_destroy, EXTEND_HTBL, EXTEND_ISPRIVATE);	\
		}																\
	} while (0)
#define GET_RRC_QUEUE_TBL(g_ape)										\
	(get_property(g_ape->properties, "raw_recently_queue") != NULL ?	\
	 (HTBL*)get_property(g_ape->properties, "raw_recently_queue")->val: NULL)

#define ADD_RRC_INDEX_TBL(g_ape)										\
	do {																\
		if (get_property(g_ape->properties, "raw_recently_index") == NULL) { \
			add_property(&g_ape->properties, "raw_recently_index", hashtbl_init(), \
						 queue_destroy, EXTEND_HTBL, EXTEND_ISPRIVATE);	\
		}																\
	} while (0)
#define GET_RRC_INDEX_TBL(g_ape)										\
	(get_property(g_ape->properties, "raw_recently_index") != NULL ?	\
	 (HTBL*)get_property(g_ape->properties, "raw_recently_index")->val: NULL)

#define PREOP_RRC_QUEUE(table, key)										\
	do {																\
		if (!g_ape || !key || rrc_max_msg <= 0) \
			return;														\
		table = GET_RRC_QUEUE_TBL(g_ape);								\
		if (!table)														\
			return;														\
	} while (0);
#define PREOP_RRC_INDEX(table)					\
	do {										\
		if (!g_ape)								\
			return;								\
		table = GET_RRC_INDEX_TBL(g_ape);		\
		if (!table)								\
			return;								\
	} while (0);

/* hkey: 10_11 */
#define ASSEM_QUEUE_KEY(hkey, from, to)						\
	if (strcmp(from, to) < 0) {								\
		snprintf(hkey, sizeof(hkey), "%s_%s", from, to);	\
	} else {												\
		snprintf(hkey, sizeof(hkey), "%s_%s", to, from);	\
	}

static int rrc_max_user = 0;
static int rrc_max_msg = 0;

/*
 * raw_queue_xxx 
 */
static void raw_queue_in(acetables *g_ape, RAW *raw, char *key)
{
	HTBL *table;

	if (!raw) return;
	
	PREOP_RRC_QUEUE(table, key);

	Queue *queue = hashtbl_seek(table, key);
	if (!queue) {
		queue = queue_new(rrc_max_msg, free_raw);
		hashtbl_append(table, key, queue);
	}

	copy_raw_z(raw);
	queue_fixlen_push_tail(queue, raw);
}

static void raw_queue_out(acetables *g_ape, USERS *user, char *key)
{
	HTBL *table;

	PREOP_RRC_QUEUE(table, key);
	
	Queue *queue = hashtbl_seek(table, key);
	QueueEntry *qe;
	if (queue) {
		queue_iterate(queue, qe) {
			post_raw(qe->data, user, g_ape);
		}
	}
}

static void raw_queue_out_sub(acetables *g_ape, subuser *sub, char *key)
{
	HTBL *table;

	PREOP_RRC_QUEUE(table, key);
	
	Queue *queue = hashtbl_seek(table, key);
	QueueEntry *qe;
	if (queue) {
		queue_iterate(queue, qe) {
			post_raw_sub(qe->data, sub, g_ape);
		}
	}
}


/*
 * rrc_index_xxx
 */
static void rrc_index_update(HTBL *table, char *from, char *to, int max)
{
	Queue *index;
	
	if (table && from && to && max > 0) {
		/* update from's index */
		index = hashtbl_seek(table, from);
		if (!index) {
			index = queue_new(max, free);
			hashtbl_append(table, from, index);
		}

		if (queue_find(index, to, hn_str_cmp) == -1) {
			queue_fixlen_push_head(index, strdup(to));
		}

		/* update to's index */
		index = hashtbl_seek(table, to);
		if (!index) {
			index = queue_new(max, free);
			hashtbl_append(table, to, index);
		}

		if (queue_find(index, from, hn_str_cmp) == -1) {
			queue_fixlen_push_head(index, strdup(from));
		}
	}
}


/*
 * public
 */
void init_raw_recently(acetables *g_ape)
{
	rrc_max_user = atoi(CONFIG_VAL(RawRecently, max_num_user, g_ape->srv));
	rrc_max_msg = atoi(CONFIG_VAL(RawRecently, max_num_msg, g_ape->srv));
	
	if (rrc_max_user > 0 && rrc_max_msg > 0) {
		ADD_RRC_QUEUE_TBL(g_ape);
		ADD_RRC_INDEX_TBL(g_ape);
	}
}

void free_raw_recently(acetables *g_ape)
{
	//hashtbl_free(GET_RRC_QUEUE_TBL(g_ape), free_raw);
	//hashtbl_free(GET_RRC_INDEX_TBL(g_ape), free);
	del_property(&g_ape->properties, "raw_recently_queue");
	del_property(&g_ape->properties, "raw_recently_index");
}

#if 0
void load_raw_recently(acetables *g_ape)
{
	HTBL *table;
	int nTok;
	char *tkn[256], hkey[128], *msgraw, *from, *to;

	HDF *node, *chi;
	HTBL *etbl = GET_EVENT_TBL(g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "msg");
	if (!evt) return;

	alog_dbg("load raw recently ...");

	PREOP_RRC_INDEX(table);

	if (PROCESS_NOK(mevent_trigger(evt, NULL, REQ_CMD_INDEXGET, FLAGS_SYNC))) {
		alog_err("load index error");
		return;
	}

	hdf_init(&node);
	hdf_copy(node, NULL, evt->hdfrcv);
	node = hdf_get_child(node, "index");
	while (node) {
		to = hdf_get_value(node, "msgto", NULL);
		from = hdf_get_value(node, "msgfrom", NULL);
		if (!to || !from) goto conti;
		nTok = explode(';', from, tkn, rrc_max_user);
		for (int i = 0; i < nTok; i++) {
			rrc_index_update(table, tkn[i], to, rrc_max_user);

			ASSEM_QUEUE_KEY(hkey, tkn[i], to);
			hdf_set_value(evt->hdfsnd, "msgkey", hkey);
			if (PROCESS_OK(mevent_trigger(evt, hkey, REQ_CMD_MSGGET, FLAGS_SYNC))) {
				chi = hdf_get_child(evt->hdfrcv, "raws");
				while (chi) {
					msgraw = hdf_get_value(chi, "msgraw", NULL);
					if (msgraw) {
						RAW *new_raw = calloc(1, sizeof(*new_raw));
						new_raw->len = strlen(msgraw);
						new_raw->next = NULL;
						new_raw->priority = RAW_PRI_LO;
						new_raw->refcount = 0;
						new_raw->data = strdup(msgraw);
					
						raw_queue_in(g_ape, new_raw, hkey);
					}
					chi = hdf_obj_next(chi);
				}
			}
		}

	conti:
		node = hdf_obj_next(node);
	}
	hdf_destroy(&node);

	alog_dbg("load raw recently done ...");
}

void save_raw_recently(acetables *g_ape)
{
	HTBL *table;
	HTBL_ITEM *item;
	Queue *queue;
	QueueEntry *qe;
	char *from, *to, *msg;

	HTBL *etbl = GET_EVENT_TBL(g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "msg");
	if (!evt) return;

	/* ============================ */
	alog_dbg("save index table...");

	PREOP_RRC_INDEX(table);
	item = table->first;
	
	STRING str;
	while (item) {
		string_init(&str);
		
		to = item->key;
		queue = (Queue*)(item->addrs);
		if (queue) {
			queue_iterate(queue, qe) {
				from = (char*)(qe->data);
				string_append(&str, from);
				string_append(&str, ":");
			}
			
			hdf_set_value(evt->hdfsnd, "msgto", to);
			hdf_set_value(evt->hdfsnd, "msgfrom", str.buf);

			if (PROCESS_NOK(mevent_trigger(evt, to, REQ_CMD_INDEXSET, FLAGS_NONE))) {
				alog_err("save index failure %d", evt->errcode);
			}
		}
		
		string_clear(&str);
		item = item->lnext;
	}
	alog_dbg("save index table done");

	/* ============================ */
	alog_dbg("save queue table...");

	table = GET_RRC_QUEUE_TBL(g_ape);
	item = table->first;
	while (item) {
		queue = (Queue*)(item->addrs);
		if (queue) {
			queue_iterate(queue, qe) {
				msg = (char*)(qe->data);
				hdf_set_value(evt->hdfsnd, "msgkey", item->key);
				hdf_set_value(evt->hdfsnd, "msgraw", msg);
				if (PROCESS_NOK(mevent_trigger(evt, to, REQ_CMD_MSGSET, FLAGS_NONE))) {
					alog_err("save msg failure %d", evt->errcode);
				}
			}
		}

		item = item->lnext;
	}
	
	alog_dbg("save queue table done");
}
#endif

/*
 * push
 */
void push_raw_recently(acetables *g_ape, RAW *raw, char *key)
{
	raw_queue_in(g_ape, raw, key);
}

void push_raw_recently_byme(acetables *g_ape, RAW *raw, char *from, char *to)
{
	if (!raw || !from || !to) return;

	HTBL *table;
	char hkey[128];
	
	PREOP_RRC_INDEX(table);

	rrc_index_update(table, from, to, rrc_max_user);
	
	ASSEM_QUEUE_KEY(hkey, from, to);

	raw_queue_in(g_ape, raw, hkey);
}

/*
 * post
 */
void post_raw_recently(acetables *g_ape, USERS *user, char *key)
{
	if (!user || !key) return;
	
	raw_queue_out(g_ape, user, key);
}

void post_raw_sub_recently(acetables *g_ape, subuser *sub, char *key)
{
	if (!sub || !key) return;
	
	raw_queue_out_sub(g_ape, sub, key);
}

void post_raw_recently_byme(acetables *g_ape, USERS *user, char *from, char *to)
{
	char hkey[128];

	if (!user || !to) return;

	if (from) {
		ASSEM_QUEUE_KEY(hkey, from, to);
		raw_queue_out(g_ape, user, hkey);
	} else {
		HTBL *table;
		
		PREOP_RRC_INDEX(table);

		Queue *index = hashtbl_seek(table, to);
		QueueEntry *qe;
		
		if (index) {
			queue_iterate(index, qe) {
				post_raw_recently_byme(g_ape, user, (char*)qe->data, to);
			}
		}
	}
}

void post_raw_sub_recently_byme(acetables *g_ape, subuser *sub, char *from, char *to)
{
	char hkey[128];

	if (!sub || !to) return;

	if (from) {
		ASSEM_QUEUE_KEY(hkey, from, to);
		raw_queue_out_sub(g_ape, sub, hkey);
	} else {
		HTBL *table;
		
		PREOP_RRC_INDEX(table);

		Queue *index = hashtbl_seek(table, to);
		QueueEntry *qe;
		if (index) {
			queue_iterate(index, qe) {
				post_raw_sub_recently_byme(g_ape, sub, (char*)qe->data, to);
			}
		}
	}
}

