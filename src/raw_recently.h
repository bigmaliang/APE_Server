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

/* raw_recently.h */

#ifndef _RAW_RECENTLY_H
#define _RAW_RECENTLY_H

#include "main.h"
#include "json.h"
#include "raw.h"
#include "extend.h"
#include "users.h"

/* raw_recently_type, sync with ape.conf */
enum {
	RRC_TYPE_NORMAL = 0,
	RRC_TYPE_MIXED,
	RRC_TYPE_MAX
};

#define RRC_TYPE_OK(type) (type >= 0 && type < RRC_TYPE_MAX)
#define RRC_TYPE_NOK(type) (type < 0 || type >= RRC_TYPE_MAX)

void init_raw_recently(acetables *g_ape);
void free_raw_recently(acetables *g_ape);

void push_raw_recently(acetables *g_ape, RAW *raw, char *key);
void push_raw_recently_byme(acetables *g_ape, RAW *raw, char *from, char *to);

void post_raw_recently(acetables *g_ape, USERS *user, char *key);
void post_raw_sub_recently(acetables *g_ape, subuser *sub, char *key);

void post_raw_recently_byme(acetables *g_ape, USERS *user, char *from, char *to);
void post_raw_sub_recently_byme(acetables *g_ape, subuser *sub, char *from, char *to);

/* TODO: distinct offline, online message in deque */

#endif
