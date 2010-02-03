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

/**
 * @file queue.h
 *
 * @brief Double-ended queue.
 *
 * A double ended queue stores a list of values in order.  New values
 * can be added and removed from either end of the queue.
 *
 * To create a new queue, use @ref queue_new.  To destroy a queue, use
 * @ref queue_free.
 *
 * To add values to a queue, use @ref queue_push_head and
 * @ref queue_push_tail.
 *
 * To read values from the ends of a queue, use @ref queue_pop_head
 * and @ref queue_pop_tail.  To examine the ends without removing values
 * from the queue, use @ref queue_peek_head and @ref queue_peek_tail.
 *
 */

#ifndef ALGORITHM_QUEUE_H
#define ALGORITHM_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A double-ended queue.
 */
	
/**
 * A value stored in a @ref Queue.
 */

typedef void *QueueValue;
typedef struct _Queue Queue;
typedef struct _QueueEntry QueueEntry;

/**
 * Callback function used to compare values in a queue.
 *
 * @param value1      The first value to compare.
 * @param value2      The second value to compare.
 * @return            A negative value if value1 should be sorted before 
 *                    value2, a positive value if value1 should be sorted 
 *                    after value2, zero if value1 and value2 are equal.
 */

typedef int (*QueueCompareFunc)(QueueValue value1, QueueValue value2);

/**
 * Callback function used to free values in a queue.
 *
 * @param value       The value to be free.
 */

typedef void (*QueueFreeFunc)(QueueValue value);

/*
 * put _QueueEntry here for public use.
 */
struct _QueueEntry {
	QueueValue data;
	QueueEntry *prev;
	QueueEntry *next;
};

struct _Queue {
	QueueEntry *head;
	QueueEntry *tail;
	unsigned int num;
	unsigned int max;
	QueueFreeFunc free_func;
};

/**
 * A null @ref QueueValue.
 */

#define QUEUE_NULL ((void *) 0)

/**
 * Create a new double-ended queue.
 *
 * @param max       The max size on fixlen_xxx operation.
 * @param free_func  The data free function.
 * @return           A new queue, or NULL if it was not possible to allocate
 *                   the memory.
 */

Queue *queue_new(unsigned int max, QueueFreeFunc free_func);

/**
 * Destroy a queue.
 *
 * @param queue      The queue to destroy.
 */

void queue_free(Queue *queue);

/**
 * Destroy a queue, and free the Entry
 *
 * @param queue      The queue to destroy.
 */

void queue_destroy(void *p);

/**
 * Add a value to the head of a queue.
 *
 * @param queue      The queue.
 * @param data       The value to add.
 * @return           Non-zero if the value was added successfully, or zero
 *                   if it was not possible to allocate the memory for the
 *                   new entry. 
 */

int queue_push_head(Queue *queue, QueueValue data);

/**
 * Remove a value from the head of a queue.
 *
 * @param queue      The queue.
 * @return           Value that was at the head of the queue, or
 *                   @ref QUEUE_NULL if the queue is empty.
 */

QueueValue queue_pop_head(Queue *queue);

/**
 * Read value from the head of a queue, without removing it from
 * the queue.
 *
 * @param queue      The queue.
 * @return           Value at the head of the queue, or @ref QUEUE_NULL if the 
 *                   queue is empty.
 */

QueueValue queue_peek_head(Queue *queue);

/**
 * Add a value to the tail of a queue.
 *
 * @param queue      The queue.
 * @param data       The value to add.
 * @return           Non-zero if the value was added successfully, or zero
 *                   if it was not possible to allocate the memory for the
 *                   new entry. 
 */

int queue_push_tail(Queue *queue, QueueValue data);

/**
 * Remove a value from the tail of a queue.
 *
 * @param queue      The queue.
 * @return           Value that was at the head of the queue, or
 *                   @ref QUEUE_NULL if the queue is empty.
 */

QueueValue queue_pop_tail(Queue *queue);

/**
 * Read a value from the tail of a queue, without removing it from
 * the queue.
 *
 * @param queue      The queue.
 * @return           Value at the tail of the queue, or QUEUE_NULL if the 
 *                   queue is empty.
 */

QueueValue queue_peek_tail(Queue *queue);

/**
 * Query if any values are currently in a queue.
 *
 * @param queue      The queue.
 * @return           Zero if the queue is not empty, non-zero if the queue
 *                   is empty.
 */

int queue_is_empty(Queue *queue);

/**
 * Find the postion of data
 *
 * @param queue      The queue.
 * @param data       The value to find.
 * @param compare_func      The data compare function.
 * @return           The position of the data, -1 on not found.
 */

int queue_find(Queue *queue, QueueValue data, QueueCompareFunc compare_func);

/**
 * Add a value to the head of a queue, when data num reach max,
 * pop data from tail, and free it
 *
 * @param queue      The queue.
 * @param data       The value to be add.
 * @return           Non-zero if the value was added successfully, or zero
 *                   if it was not possible to allocate the memory for the
 *                   new entry. 
 */

int queue_fixlen_push_head(Queue *queue, QueueValue data);

/**
 * Add a value to the tail of a queue, when data num reach max,
 * pop data from head, and free it
 *
 * @param queue      The queue.
 * @param data       The value to be add.
 * @return           Non-zero if the value was added successfully, or zero
 *                   if it was not possible to allocate the memory for the
 *                   new entry. 
 */

int queue_fixlen_push_tail(Queue *queue, QueueValue data);

 /**
 * Find the length of a queue
 *
 * @param queue      The queue.
 * @return           The number of the entries in the queue.
 */

unsigned int queue_length(Queue *queue);

#define queue_iterate(q, qe)						\
	for (qe = q->head; qe != NULL; qe = qe->next)

#ifdef __cplusplus
}
#endif

#endif /* #ifndef ALGORITHM_QUEUE_H */

