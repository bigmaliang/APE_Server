/*

Copyright (c) 2005-2008, Simon Howard

Permission to use, copy, modify, and/or distribute this software 
for any purpose with or without fee is hereby granted, provided 
that the above copyright notice and this permission notice appear 
in all copies. 

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL 
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE 
AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR 
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, 
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN      
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 

 */

#include <stdlib.h>

#include "queue.h"

/* malloc() / free() testing */

#ifdef ALLOC_TESTING
#include "alloc-testing.h"
#endif

/* A double-ended queue */

Queue *queue_new(unsigned int max, QueueFreeFunc free_func)
{
	Queue *queue;

	queue = (Queue *) malloc(sizeof(Queue));

	if (queue == NULL) {
		return NULL;
	}
	
	queue->head = NULL;
	queue->tail = NULL;
	queue->num = 0;
	queue->max = max;
	queue->free_func = free_func;

	return queue;
}

void queue_free(Queue *queue)
{
	if (!queue) return;
	
	/* Empty the queue */
	
	while (!queue_is_empty(queue)) {
		queue_pop_head(queue);
	}

	/* Free back the queue */

	free(queue);
}

void queue_destroy(void *p)
{
	if (!p) return;
	
	Queue *queue = (Queue*)p;
	QueueValue val;
	/* Destroy the queue */
	
	while (!queue_is_empty(queue)) {
		val = queue_pop_head(queue);
		if(queue->free_func) queue->free_func(val);
	}

	/* Free back the queue */

	free(queue);
}

int queue_push_head(Queue *queue, QueueValue data)
{
	QueueEntry *new_entry;

	/* Create the new entry and fill in the fields in the structure */

	new_entry = malloc(sizeof(QueueEntry));

	if (new_entry == NULL) {
		return 0;
	}
	
	new_entry->data = data;
	new_entry->prev = NULL;
	new_entry->next = queue->head;
	
	/* Insert into the queue */

	if (queue->head == NULL) {

		/* If the queue was previously empty, both the head and tail must
		 * be pointed at the new entry */

		queue->head = new_entry;
		queue->tail = new_entry;

	} else {

		/* First entry in the list must have prev pointed back to this
		 * new entry */

		queue->head->prev = new_entry;

		/* Only the head must be pointed at the new entry */

		queue->head = new_entry;
	}

	queue->num++;

	return 1;
}

QueueValue queue_pop_head(Queue *queue)
{
	QueueEntry *entry;
	QueueValue result;

	/* Check the queue is not empty */

	if (queue_is_empty(queue)) {
		return QUEUE_NULL;
	}

	/* Unlink the first entry from the head of the queue */

	entry = queue->head;
	queue->head = entry->next;
	result = entry->data;

	if (queue->head == NULL) {

		/* If doing this has unlinked the last entry in the queue, set
		 * tail to NULL as well. */

		queue->tail = NULL;
	} else {

		/* The new first in the queue has no previous entry */

		queue->head->prev = NULL;
	}

	/* Free back the queue entry structure */

	free(entry);
	
	if (queue->num > 0) queue->num--;

	return result;    
}

QueueValue queue_peek_head(Queue *queue)
{
	if (queue_is_empty(queue)) {
		return QUEUE_NULL;
	} else {
		return queue->head->data;
	}
}

int queue_push_tail(Queue *queue, QueueValue data)
{
	QueueEntry *new_entry;

	/* Create the new entry and fill in the fields in the structure */

	new_entry = malloc(sizeof(QueueEntry));

	if (new_entry == NULL) {
		return 0;
	}
	
	new_entry->data = data;
	new_entry->prev = queue->tail;
	new_entry->next = NULL;
	
	/* Insert into the queue tail */

	if (queue->tail == NULL) {

		/* If the queue was previously empty, both the head and tail must
		 * be pointed at the new entry */

		queue->head = new_entry;
		queue->tail = new_entry;

	} else {

		/* The current entry at the tail must have next pointed to this
		 * new entry */

		queue->tail->next = new_entry;

		/* Only the tail must be pointed at the new entry */

		queue->tail = new_entry;
	}

	queue->num++;

	return 1;
}

QueueValue queue_pop_tail(Queue *queue)
{
	QueueEntry *entry;
	QueueValue result;

	/* Check the queue is not empty */

	if (queue_is_empty(queue)) {
		return QUEUE_NULL;
	}

	/* Unlink the first entry from the tail of the queue */

	entry = queue->tail;
	queue->tail = entry->prev;
	result = entry->data;

	if (queue->tail == NULL) {

		/* If doing this has unlinked the last entry in the queue, set
		 * head to NULL as well. */

		queue->head = NULL;

	} else {

		/* The new entry at the tail has no next entry. */

		queue->tail->next = NULL;
	}

	/* Free back the queue entry structure */

	free(entry);
	
	if (queue->num > 0) queue->num--;

	return result;    
}

QueueValue queue_peek_tail(Queue *queue)
{
	if (queue_is_empty(queue)) {
		return QUEUE_NULL;
	} else {
		return queue->tail->data;
	}
}

int queue_is_empty(Queue *queue)
{
	return queue->head == NULL;
}

int queue_find(Queue *queue, QueueValue data, QueueCompareFunc compare_func)
{
	QueueEntry *entry = queue->head;
	unsigned int pos = 0;

	while (entry != NULL) {
		if (compare_func(entry->data, data) == 0)
			return pos;

		pos++;
		entry = entry->next;
	}
	
	return -1;
}

int queue_fixlen_push_head(Queue *queue, QueueValue data)
{
	QueueValue val;
	
	while (queue->num >= queue->max) {
		val = queue_pop_tail(queue);
		if (queue->free_func) queue->free_func(val);
	}

	return queue_push_head(queue, data);
}

int queue_fixlen_push_tail(Queue *queue, QueueValue data)
{
	QueueValue val;
	
	while (queue->num >= queue->max) {
		val = queue_pop_head(queue);
		if (queue->free_func) queue->free_func(val);
	}

	return queue_push_tail(queue, data);
}

unsigned int queue_length(Queue *queue)
{
	if (queue == NULL || queue->head == NULL) return 0;

	return queue->num;

#if 0
	QueueEntry *entry = queue->head;
	unsigned int length = 0;

	while (entry != NULL) {
		length++;
		entry = entry->next;
	}

	return length;
#endif
}
