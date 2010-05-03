#ifndef __LIBAPE_LCS_H__
#define __LIBAPE_LCS_H__

#define LCS_PIP_NAME "LiveCSPipe_"
#define RAW_LCSDATA	"LCS_DATA"

enum {
	LCS_SERVICE_LIMITED = 0,
	LCS_SERVICE_NORMAL = 100,
} lcs_stat;

enum {
	ST_LCS_MSG_TOTAL = 100,
	ST_LCS_ALIVE_GROUP
} lcs_stastic;

typedef struct {
	unsigned long msg_total;
} st_lcs;

#endif
