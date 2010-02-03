/*
  Copyright (C) 2006, 2007, 2008, 2009  Anthony Catel <a.catel@weelya.com>

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

/* raw.h */

#ifndef _RAW_H
#define _RAW_H

#include <stdbool.h>

#include "main.h"
#include "users.h"
#include "channel.h"
#include "proxy.h"
#include "transports.h"
#include "sock.h"
#include "log.h"

typedef enum {
	RAW_PRI_LO,
	RAW_PRI_HI
} raw_priority_t;

typedef struct RAW
{
	char *data;
	struct RAW *next;
	
	raw_priority_t priority;
	
	int len;
	int refcount;
} RAW;


RAW *forge_raw(const char *raw, json_item *jlist);
void free_raw(void *p);
void delete_raw(RAW *fraw);
RAW *copy_raw(RAW *input);
RAW *copy_raw_z(RAW *input);

void post_raw(RAW *raw, USERS *user, acetables *g_ape);
void post_raw_sub(RAW *raw, subuser *sub, acetables *g_ape);
void post_raw_channel(RAW *raw, struct CHANNEL *chan, acetables *g_ape);
void post_raw_restricted(RAW *raw, USERS *user, subuser *sub, acetables *g_ape);
void post_raw_channel_restricted(RAW *raw, struct CHANNEL *chan, USERS *ruser, acetables *g_ape);
void proxy_post_raw(RAW *raw, ape_proxy *proxy, acetables *g_ape);
int post_raw_pipe(RAW *raw, const char *pipe, acetables *g_ape);
json_item* post_to_pipe(json_item *jlist, const char *rawname, const char *pipe, subuser *from, acetables *g_ape, bool jcopy);

int send_raw_inline(ape_socket *client, transport_t transport, RAW *raw, acetables *g_ape);
int send_raws(subuser *user, acetables *g_ape);

struct _raw_pool *init_raw_pool(int n);
struct _raw_pool *expend_raw_pool(struct _raw_pool *ptr, int n);
void destroy_raw_pool(struct _raw_pool *ptr);

#ifdef POSTRAW_CHECK
#define POSTRAW_DONE(raw)									\
	do {													\
		if (raw && raw->refcount == 0) {					\
			alog_warn("post %s for nothing", raw->data);	\
			delete_raw(raw);								\
		}													\
	} while(0)
#else
#define POSTRAW_DONE(raw)
#endif

#endif
