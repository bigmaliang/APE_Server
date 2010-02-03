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

#define PREOP_RRC_QUEUE(table, hkey)									\
	do {																\
		if (!g_ape || !key || RRC_TYPE_NOK(type) || rrc_max_size[type] <= 0) \
			return;														\
		snprintf(hkey, sizeof(hkey), "%s_%d", key, type);				\
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

static int rrc_max_size[RRC_TYPE_MAX];

/*
 * raw_queue_xxx 
 */
static void raw_queue_in(acetables *g_ape, RAW *raw, char *key, int type)
{
	HTBL *table;
	char hkey[128];

	if (!raw) return;
	
	PREOP_RRC_QUEUE(table, hkey);

	Queue *queue = hashtbl_seek(table, hkey);
	if (!queue) {
		queue = queue_new(rrc_max_size[type], free_raw);
		hashtbl_append(table, hkey, queue);
	}

	copy_raw_z(raw);
	queue_fixlen_push_tail(queue, raw);
}

static void raw_queue_out(acetables *g_ape, USERS *user, char *key, int type)
{
	HTBL *table;
	char hkey[128];

	PREOP_RRC_QUEUE(table, hkey);
	
	Queue *queue = hashtbl_seek(table, hkey);
	QueueEntry *qe;
	if (queue) {
		queue_iterate(queue, qe) {
			post_raw(qe->data, user, g_ape);
		}
	}
}

static void raw_queue_out_sub(acetables *g_ape, subuser *sub, char *key, int type)
{
	HTBL *table;
	char hkey[128];

	PREOP_RRC_QUEUE(table, hkey);
	
	Queue *queue = hashtbl_seek(table, hkey);
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
static int rrc_index_cmp(void *a, void *b)
{
	char *sa, *sb;
	sa = (char*)a;
	sb = (char*)b;

	return strcmp(sa, sb);
}

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

		if (queue_find(index, to, rrc_index_cmp) == -1) {
			queue_fixlen_push_head(index, strdup(to));
		}

		/* update to's index */
		index = hashtbl_seek(table, to);
		if (!index) {
			index = queue_new(max, free);
			hashtbl_append(table, to, index);
		}

		if (queue_find(index, from, rrc_index_cmp) == -1) {
			queue_fixlen_push_head(index, strdup(from));
		}
	}
}


/*
 * public
 */
void init_raw_recently(acetables *g_ape)
{
	apeconfig *conf = ape_config_get_section(g_ape->srv, "RawRecently");
	
	if (conf) {
		struct _apeconfig_def *def = conf->def;
		int count = 0;
		while (def != NULL && count < RRC_TYPE_MAX) {
			if (!strncmp(def->key, "max_size_", strlen("max_size_"))) {
				rrc_max_size[count++] = atoi(def->val);
			}
			def = def->next;
		}
		
		if (count > 0) {
			ADD_RRC_QUEUE_TBL(g_ape);
			ADD_RRC_INDEX_TBL(g_ape);
		}
	}
}

void free_raw_recently(acetables *g_ape)
{
	//hashtbl_free(GET_RRC_QUEUE_TBL(g_ape), free_raw);
	//hashtbl_free(GET_RRC_INDEX_TBL(g_ape), free);
	del_property(&g_ape->properties, "raw_recently_queue");
	del_property(&g_ape->properties, "raw_recently_index");
}

void push_raw_recently_single(acetables *g_ape, RAW *raw, char *from, char *to)
{
	if (!raw || !from || !to) return;

	HTBL *table;
	char hkey[128];
	
	PREOP_RRC_INDEX(table);

	rrc_index_update(table, from, to,
					 atoi(CONFIG_VAL(RawRecently, max_num_user, g_ape->srv)));
	
	ASSEM_QUEUE_KEY(hkey, from, to);

	raw_queue_in(g_ape, raw, hkey, RRC_TYPE_SINGLE);
}

void push_raw_recently_group(acetables *g_ape, RAW *raw, char *key, int type)
{
	raw_queue_in(g_ape, raw, key, type);
}

void post_raw_recently_single(acetables *g_ape, USERS *user, char *from, char *to)
{
	char hkey[128];

	ASSEM_QUEUE_KEY(hkey, from, to);

	raw_queue_out(g_ape, user, hkey, RRC_TYPE_SINGLE);
}

void post_raw_recently(acetables *g_ape, USERS *user, char *key, int type)
{
	if (!user || !key || RRC_TYPE_NOK(type) || rrc_max_size[type] <= 0)
		return;
	
	if (type == RRC_TYPE_SINGLE) {
		HTBL *table;
		
		PREOP_RRC_INDEX(table);

		Queue *index = hashtbl_seek(table, key);
		QueueEntry *qe;
		
		if (index) {
			queue_iterate(index, qe) {
				post_raw_recently_single(g_ape, user, key, (char*)qe->data);
			}
		}
	} else {
		raw_queue_out(g_ape, user, key, type);
	}
}

void post_raw_sub_recently_single(acetables *g_ape, subuser *sub, char *from, char *to)
{
	char hkey[128];

	ASSEM_QUEUE_KEY(hkey, from, to);

	raw_queue_out_sub(g_ape, sub, hkey, RRC_TYPE_SINGLE);
}

void post_raw_sub_recently(acetables *g_ape, subuser *sub, char *key, int type)
{
	if (!sub || !key || RRC_TYPE_NOK(type) || rrc_max_size[type] <= 0)
		return;
	
	if (type == RRC_TYPE_SINGLE) {
		HTBL *table;
		
		PREOP_RRC_INDEX(table);

		Queue *index = hashtbl_seek(table, key);
		QueueEntry *qe;
		if (index) {
			queue_iterate(index, qe) {
				post_raw_sub_recently_single(g_ape, sub, key, (char*)qe->data);
			}
		}
	} else{
		raw_queue_out_sub(g_ape, sub, key, type);
	}
}
