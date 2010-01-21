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

/* channel_history.h */

#ifndef _CHANNEL_HISTORY_H
#define _CHANNEL_HISTORY_H

#include "main.h"
#include "channel.h"
#include "json.h"
#include "raw.h"
#include "extend.h"
#include "users.h"

#define SET_CHANNEL_MAX_HISTORY_SIZE(chan, size)					\
	do {															\
		add_property(&chan->properties, "max_history_size", size,	\
					 EXTEND_STR, EXTEND_ISPRIVATE);					\
	} while (0)
#define GET_CHANNEL_MAX_HISTORY_SIZE(chan) (strtol(get_property_val((chan)->properties, "max_history_size"), NULL, 10))


typedef struct CHANNEL_HISTORY_NODE
{
	struct RAW *value;
	struct CHANNEL_HISTORY_NODE *next;
} CHANNEL_HISTORY_NODE;

typedef struct CHANNEL_HISTORY_DEQUE
{
	int length;	
	struct CHANNEL_HISTORY_NODE *head;
	struct CHANNEL_HISTORY_NODE *tail;
} CHANNEL_HISTORY_DEQUE;


CHANNEL_HISTORY_DEQUE *init_channel_history(void);
void free_channel_history(CHANNEL_HISTORY_DEQUE *deque);

void push_raw_to_channel_history(CHANNEL *chan, RAW *raw);
void update_chan_history_size(CHANNEL *chan);
void post_channel_history(CHANNEL *chan, USERS *user, acetables *g_ape);

#endif
