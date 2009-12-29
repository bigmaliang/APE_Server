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
} stastic_type;

#define SET_USER_FOR_APE(ape, uin, user)								\
	do {																\
		hashtbl_append(get_property(ape->properties, "userlist")->val,	\
					   uin, user);										\
	} while (0)
#define GET_USER_FROM_APE(ape, uin)										\
	(get_property(ape->properties, "userlist") != NULL ?				\
	 hashtbl_seek(get_property(ape->properties, "userlist")->val, uin): NULL)


#define SET_USER_FOR_ONLINE(ape, uin, user)								\
	do {																\
		hashtbl_append(get_property(ape->properties, "onlineuser")->val, \
					   uin, user);										\
	} while (0)
#define GET_USER_FROM_ONLINE(ape, uin)									\
	(get_property(ape->properties, "onlineuser") != NULL ?				\
	 hashtbl_seek(get_property(ape->properties, "onlineuser")->val, uin): NULL)
#define GET_ONLINE_TBL(ape)											\
	(get_property(ape->properties, "onlineuser") != NULL ?			\
	 (HTBL*)get_property(ape->properties, "onlineuser")->val: NULL)


#define SET_UIN_FOR_USER(user, uin)					\
	do {											\
		add_property(&user->properties, "uin", uin,	\
					 EXTEND_STR, EXTEND_ISPUBLIC);	\
	} while (0)
#define GET_UIN_FROM_USER(user)									\
	(get_property(user->properties, "uin") != NULL ?			\
	 (char*)get_property(user->properties, "uin")->val: NULL)


#define GET_USER_FRIEND_TBL(user)									\
	(get_property(user->properties, "friend") != NULL ?				\
	 (HTBL*)get_property(user->properties, "friend")->val: NULL)

#define GET_USER_LIST(g_ape)											\
	(get_property(g_ape->properties, "userlist") != NULL ?				\
	 (HTBL*)get_property(g_ape->properties, "userlist")->val: NULL)

#define GET_STAT_LIST(g_ape)											\
	(get_property(g_ape->properties, "msgstatic") != NULL ?				\
	 (st_push*)get_property(g_ape->properties, "msgstatic")->val: NULL)

#endif
