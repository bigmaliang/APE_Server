/*
  Copyright (C) 2006, 2007, 2008, 2009	Anthony Catel <a.catel@weelya.com>

  This file is part of APE Server.
  APE is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  APE is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with APE ; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/* log.h */

#ifndef _LOG_H
#define _LOG_H

#include "main.h"

/* Maximum string to log */
#define MAX_LOG_STR 1024
#define LOG_LEVELS	7

typedef enum {
	ALOG_DIE = 0,
	ALOG_FOO,
	ALOG_ERROR,
	ALOG_WARNING,	/* warning's pri is higher than debug. */
	ALOG_INFO,
	ALOG_DEBUG,
	ALOG_NOISE
} ape_log_lvl_t;

void ape_log_init(acetables *g_ape);
int ape_log_open(char *logfname);
int ape_log_reopen(char *logfname);
void ape_log_setlv(int lv);
void ape_log_done();

#define alog_die(f,...)													\
	do {																\
		ape_log(__PRETTY_FUNCTION__,__FILE__,__LINE__,ALOG_DIE,f,##__VA_ARGS__); \
		exit(-1);														\
	} while(0)
#define alog_foo(f,...)		ape_log(__PRETTY_FUNCTION__,__FILE__,__LINE__,ALOG_FOO,f,##__VA_ARGS__)
#define alog_err(f,...)		ape_log(__PRETTY_FUNCTION__,__FILE__,__LINE__,ALOG_ERROR,f,##__VA_ARGS__)
#define alog_warn(f,...)	ape_log(__PRETTY_FUNCTION__,__FILE__,__LINE__,ALOG_WARNING,f,##__VA_ARGS__)
#define alog_dbg(f,...)		ape_log(__PRETTY_FUNCTION__,__FILE__,__LINE__,ALOG_DEBUG,f,##__VA_ARGS__)
#define alog_info(f,...)	ape_log(__PRETTY_FUNCTION__,__FILE__,__LINE__,ALOG_INFO,f,##__VA_ARGS__)
#define alog_noise(f,...)	ape_log(__PRETTY_FUNCTION__,__FILE__,__LINE__,ALOG_NOISE,f,##__VA_ARGS__)

#define alog_errlog(s)		ape_log(__PRETTY_FUNCTION__,__FILE__,__LINE__,ALOG_ERROR,"%s: %s",s,strerror(errno))


/* Normal logging, printf()-alike */
void ape_log(const char *func, const char *file, long line,
			 ape_log_lvl_t level, const char *fmt, ...);

#endif
