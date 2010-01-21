#ifndef __LIBAPE_PUSH_H__
#define __LIBAPE_PUSH_H__

#define FRIEND_PIP_NAME	"*FriendPipe"
#define MAX_CLIENT_PERPUSH	500

typedef struct {
    unsigned long msg_notice;
    unsigned long msg_feed;
    unsigned long msg_limited;
	unsigned long num_login;
	unsigned long num_user;
} st_push;

enum {
    ST_ONLINE = 0,
    ST_NOTICE,
    ST_FEED,
	ST_NUM_LOGIN,
	ST_NUM_USER
} push_stastic;

#define MAKE_PUSH_STAT(g_ape, p)										\
	do {																\
		if (get_property(g_ape->properties, "pushstatic") == NULL) {	\
			add_property(&g_ape->properties, "pushstatic", p,			\
						 EXTEND_POINTER, EXTEND_ISPRIVATE);				\
		}																\
	} while (0)
#define GET_PUSH_STAT(g_ape)											\
	(get_property(g_ape->properties, "pushstatic") != NULL ?			\
	 (st_push*)get_property(g_ape->properties, "pushstatic")->val: NULL)

#endif
