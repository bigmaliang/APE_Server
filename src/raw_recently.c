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

#define GET_RRC_TBL(g_ape)												\
	(get_property(g_ape->properties, "raw_recently") != NULL ?			\
	 (HTBL*)get_property(g_ape->properties, "raw_recently")->val: NULL)

#define PREPARE_RRC(table, hkey)										\
	do {																\
		if (!g_ape || !key || RRC_TYPE_NOK(type) || rrc_max_size[type] <= 0) \
			return;														\
		snprintf(hkey, sizeof(hkey), "%s_%d", key, type);				\
		table = GET_RRC_TBL(g_ape);										\
		if (!table)														\
			return;														\
	} while (0);

static int rrc_max_size[RRC_TYPE_MAX];


/*
 * history_xxx 
 */
static void history_append_element(RAW_DEQUE *deque, RAW *raw)
{
	RAW_NODE *node = xmalloc(sizeof(*node));
	node->value = raw;

	++deque->length;

	if (deque->head != NULL) {		
		node->next = deque->head;
		deque->tail = deque->tail->next = node;
		return;
	}

	node->next = node;
	deque->head = deque->tail = node;
}

void history_push_element(RAW_DEQUE *deque, RAW *raw)
{
	free_raw(deque->head->value);
	deque->head->value = raw;
	deque->tail = deque->head;	
	deque->head = deque->head->next;
}

static void history_free_node(RAW_NODE *node)
{
	free_raw(node->value);
	free(node);
}


/*
 * raw_deque_xxx 
 */
static RAW_DEQUE* raw_deque_init()
{
	RAW_DEQUE *deque = xmalloc(sizeof(*deque));

	deque->length = 0;
	deque->head = deque->tail = NULL;

	return deque;
}

static void raw_deque_free(RAW_DEQUE *deque)
{
	if (!deque) return;
	
	int i = 0;
	for (; i < deque->length; ++i) {
		deque->head = deque->head->next;
		history_free_node(deque->tail->next);
		deque->tail->next = deque->head;
	}
	free(deque);
}

static void raw_deque_push(RAW_DEQUE *deque, RAW *raw, int size)
{
	if (size <= 0) return;
	
	copy_raw_z(raw);
	if (deque->length == size) {
		history_push_element(deque, raw);
		return;
	}

	history_append_element(deque, raw);
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
			rrc_max_size[count++] = atoi(def->val);
			def = def->next;
		}
		
		if (count > 0) {
			if (get_property(g_ape->properties, "raw_recently") == NULL) {
				add_property(&g_ape->properties, "raw_recently", hashtbl_init(),
							 EXTEND_HTBL, EXTEND_ISPRIVATE);
			}
		}
	}
}

void free_raw_recently(acetables *g_ape)
{
	HTBL *table;
	if (get_property(g_ape->properties, "raw_recently") != NULL) {
		table = get_property(g_ape->properties, "raw_recently")->val;
		if (table) {
			size_t i;
			HTBL_ITEM *hTmp;
			HTBL_ITEM *hNext;
			for (i = 0; i < (HACH_TABLE_MAX + 1); i++) {
				hTmp = table->table[i];
				while (hTmp != 0) {
					hNext = hTmp->next;
					raw_deque_free(hTmp->addrs);
					hTmp = hNext;
				}
			}
			hashtbl_free(table);
		}
	}
}

void push_raw_recently(acetables *g_ape, RAW *raw, char *key, int type)
{
	HTBL *table;
	char hkey[128];

	if (!raw) return;
	
	PREPARE_RRC(table, hkey);

	RAW_DEQUE *deque = hashtbl_seek(table, hkey);
	if (!deque) {
		deque = raw_deque_init();
		hashtbl_append(table, hkey, deque);
	}

	raw_deque_push(deque, raw, rrc_max_size[type]);
}

void post_raw_recently(acetables *g_ape, USERS *user, char *key, int type)
{
	HTBL *table;
	char hkey[128];

	PREPARE_RRC(table, hkey);
	
	RAW_DEQUE *deque = hashtbl_seek(table, hkey);
	if (deque) {
		RAW_NODE *node = deque->head;
		if (node) {
			do {
				post_raw(node->value, user, g_ape);
				node = node->next;
			} while (node != deque->head);
		}
	}
}

void post_raw_sub_recently(acetables *g_ape, subuser *sub, char *key, int type)
{
	HTBL *table;
	char hkey[128];

	PREPARE_RRC(table, hkey);
	
	RAW_DEQUE *deque = hashtbl_seek(table, hkey);
	if (deque) {
		RAW_NODE *node = deque->head;
		if (node) {
			do {
				post_raw_sub(node->value, sub, g_ape);
				node = node->next;
			} while (node != deque->head);
		}
	}
}
