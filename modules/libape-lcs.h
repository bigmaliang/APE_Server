#ifndef __LIBAPE_LCS_H__
#define __LIBAPE_LCS_H__

#define LCS_PIP_NAME	"LiveCSPipe_"
#define RAW_LCSDATA		"LCS_DATA"

enum {
	LCS_ST_STRANGER = 0,
	LCS_ST_LIMITED,
	LCS_ST_FREE,				/* 30 online/month, 20 history raw, 2 admin */
	LCS_ST_VIPED,				/* ED history raw */
	LCS_ST_VIPING,				/* 300 online/month, 180 history raw, 20 admin */
	LCS_ST_VVIPING				/* unlimit online/month, unlimit history raw, unlimit admin */
} lcs_stat;

enum {
	LCS_SMS_CLOSE = 0,
	LCS_SMS_OPEN,
	LCS_ADM_OFFLINE,
	LCS_ADM_ONLINE
} lcs_flag;

typedef struct {
	unsigned long msg_total;
} st_lcs;

#endif	/* __LIBAPE_LCS_H__ */
