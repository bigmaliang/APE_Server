#ifndef __LIBAPE_LCS_H__
#define __LIBAPE_LCS_H__

#define LCS_PIP_NAME	"LiveCSPipe_"
#define RAW_LCSDATA		"LCS_DATA"

enum {
	LCS_ST_BLACK = 0,
	LCS_ST_STRANGER,
	LCS_ST_FREE = 10,			/* 20 online, 20 history raw, 2 admin */
	LCS_ST_VIPED,				/* ED history raw */
	LCS_ST_VIP,					/* 200 online, one year history raw, 20 admin */
	LCS_ST_VVIP					/* unlimit online, history raw, admin */
} lcsStat;

enum {
	LCS_SMS_CLOSE = 0,
	LCS_SMS_OPEN,
	LCS_ADM_OFFLINE,
	LCS_ADM_ONLINE
} lcsFlag;

enum {
    ST_ONLINE = 0,
    ST_MSG_TOTAL,
	ST_NUM_USER
} lcsStastic;

typedef struct {
	unsigned long msg_total;
	unsigned long num_user;
} stLcs;

#define MAKE_LCS_STAT(g_ape, p)										\
	do {															\
		if (get_property(g_ape->properties, "lcsstatic") == NULL) {	\
			add_property(&g_ape->properties, "lcsstatic", p, free,	\
						 EXTEND_POINTER, EXTEND_ISPRIVATE);			\
		}															\
	} while (0)
#define GET_LCS_STAT(g_ape)												\
	(get_property(g_ape->properties, "lcsstatic") != NULL ?				\
	 (stLcs*)get_property(g_ape->properties, "lcsstatic")->val: NULL)



/*
 * USER PROPERTIES
 */
#define ADD_JID_FOR_USER(user, jid)								\
	do {														\
		if (get_property(user->properties, "jid") == NULL) {	\
			add_property(&user->properties, "jid", jid,	NULL,	\
						 EXTEND_STR, EXTEND_ISPUBLIC);			\
		}														\
	} while (0)
#define GET_JID_FROM_USER(user)									\
	(get_property(user->properties, "jid") != NULL ?			\
	 (char*)get_property(user->properties, "jid")->val: NULL)


#endif	/* __LIBAPE_LCS_H__ */
