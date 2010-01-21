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

/* channel_history.c */

#include "channel_history.h"
#include "users.h"
#include "utils.h"
#include "raw.h"

void history_append_element(CHANNEL_HISTORY_DEQUE *deque, RAW *raw) {
	CHANNEL_HISTORY_NODE *node = xmalloc(sizeof(*node));
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

void history_push_element(CHANNEL_HISTORY_DEQUE *deque, RAW *raw) {
	free_raw(deque->head->value);
	deque->head->value = raw;
	deque->tail = deque->head;	
	deque->head = deque->head->next;
}

void history_free_node(CHANNEL_HISTORY_NODE *node) {
	free_raw(node->value);
	free(node);
}

void deque_truncate_buffer(CHANNEL_HISTORY_DEQUE *deque, const int size) {
	int i = size;
	for (; i < deque->length; ++i) {
		deque->head = deque->head->next;
		history_free_node(deque->tail->next);
		deque->tail->next = deque->head;
	}
	if (size == 0 && i > size) {
		free(deque);
	}
}


void free_channel_history(CHANNEL_HISTORY_DEQUE *deque) {
	if (deque == NULL) return;
	deque_truncate_buffer(deque, 0);
}

void update_chan_history_size(CHANNEL *chan) {
	const int max_history_size = GET_CHANNEL_MAX_HISTORY_SIZE(chan);

	if (max_history_size < chan->history->length) {
		if (max_history_size == 0) {
			free_channel_history(chan->history);
		} else {
			deque_truncate_buffer(chan->history, max_history_size);
		}		
	}
}


CHANNEL_HISTORY_DEQUE *init_channel_history(void) {
	struct CHANNEL_HISTORY_DEQUE *deque = xmalloc(sizeof(*deque));

	deque->length = 0;
	deque->head = deque->tail = NULL;

	return deque;
}

/* preconditions: memory for raw has been allocated and will not be freed later elsewhere, maxsize>0 */
void push_raw_to_channel_history(CHANNEL *chan, RAW *raw) {
	copy_raw_z(raw);
	if (chan->history->length == GET_CHANNEL_MAX_HISTORY_SIZE(chan)) {		
		history_push_element(chan->history, raw);
		return;
	} 

	history_append_element(chan->history, raw);
}

void post_channel_history(CHANNEL *chan, USERS *user, acetables *g_ape) {
	if (GET_CHANNEL_MAX_HISTORY_SIZE(chan) > 0) {
		CHANNEL_HISTORY_NODE *node = chan->history->head;

		if (node != NULL) {
			do {
				post_raw(node->value, user, g_ape);
				node = node->next;
			} while (node != chan->history->head);
		}
	}

}

