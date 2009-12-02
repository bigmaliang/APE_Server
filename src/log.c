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

/* log.c */

#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "utils.h"
#include "log.h"
#include "main.h"
#include "config.h"


static char *trace_level[LOG_LEVELS] = {"DIE", "MESSAGE", "ERROR", "WARNING", "INFO", "DEBUG", "NOISE"};
static int dftlv = ALOG_ERROR;
static int logfd = -1;

void ape_log_init(acetables *g_ape)
{
	if (CONFIG_VAL(Log, loglevel, g_ape->srv) != NULL) {
		ape_log_setlv(atoi(CONFIG_VAL(Log, loglevel, g_ape->srv)));
	}
	
	if (CONFIG_VAL(Log, logfile, g_ape->srv) != NULL) {
		if (ape_log_open(CONFIG_VAL(Log, logfile, g_ape->srv)) != 1) {
			printf("\nOpen log file %s failure\n",
				   CONFIG_VAL(Log, logfile, g_ape->srv));
			exit(1);
		}
	} else {
		ape_log_open("-");
	}
}

int ape_log_open(char *logfname)
{
	if (logfname == NULL) {
		logfd = -1;
		return 1;
	}

	if (strcmp(logfname, "-") == 0) {
		logfd = 1;
		return 1;
	}

	logfd = open(logfname, O_WRONLY | O_APPEND | O_CREAT, 0660);
	if (logfd < 0)
		return 0;

	return 1;
}

int ape_log_reopen(char *logfname)
{
	ape_log_done();
	return ape_log_open(logfname);
}

void ape_log_setlv(int lv)
{
	if (lv < 0 || lv > LOG_LEVELS)
		return;
	dftlv = lv;
}

void ape_log_done()
{
	if (logfd != 1 && logfd != -1)
		close(logfd);
}

void ape_log(const char *func, const char *file, long line,
			 ape_log_lvl_t level, const char *fmt, ...)
{
	int r, tr, ir;
	va_list ap;
	char timestr[32];
	char infostr[128];
	char logstr[MAX_LOG_STR];
	time_t t;
	struct tm *tmp;

	if (logfd == -1 || level > dftlv)
		return;

	t = time(NULL);
	tmp = localtime(&t);
	tr = strftime(timestr, 32, "[%F %H:%M:%S]", tmp);
	ir = snprintf(infostr, 128, "[%s][%s:%li %s]", trace_level[level], file, line, func);

	va_start(ap, fmt);
	r = vsnprintf(logstr, MAX_LOG_STR, fmt, ap);
	va_end(ap);

	write(logfd, timestr, tr);
	write(logfd, infostr, ir);
	write(logfd, logstr, r);
	write(logfd, "\n", 1);
}
